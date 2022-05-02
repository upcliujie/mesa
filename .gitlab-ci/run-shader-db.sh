set -e
set -v

ARTIFACTSDIR=`pwd`/shader-db
mkdir -p $ARTIFACTSDIR
export DRM_SHIM_DEBUG=true

LIBDIR=`pwd`/install/lib
export LD_LIBRARY_PATH=$LIBDIR

cd /usr/local/shader-db

for driver in freedreno intel v3d; do
    echo "Running drm-shim for $driver"
    env LD_PRELOAD=$LIBDIR/lib${driver}_noop_drm_shim.so \
        ./run -j${FDO_CI_CONCURRENT:-4} ./shaders \
            > $ARTIFACTSDIR/${driver}-shader-db.txt
done

# Run shader-db for r300 (RV370 and RV515)
for chipset in 0x5460 0x7140; do
    echo "Running drm-shim for r300 - $chipset"
    env LD_PRELOAD=$LIBDIR/libradeon_noop_drm_shim.so \
        RADEON_GPU_ID=${chipset} \
        ./run -j${FDO_CI_CONCURRENT:-4} ./shaders \
            > $ARTIFACTSDIR/r300-${chipset}-shader-db.txt
done
