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

# Run shader-db for r300
for gpuclass in R300 R500; do
    echo "Running drm-shim for r300 - $chipset"
    env MESA_LOADER_DRIVER_OVERRIDE=r300 \
        LD_PRELOAD=$LIBDIR/libradeon_noop_drm_shim.so \
        RADEON_GPU_ID=${gpuclass} \
        ./run -j${FDO_CI_CONCURRENT:-4} ./shaders \
            > $ARTIFACTSDIR/r300-${gpuclass}-shader-db.txt
done
