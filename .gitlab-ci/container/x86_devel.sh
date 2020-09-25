#!/bin/bash

set -e
set -o xtrace

export DEBIAN_FRONTEND=noninteractive

# We don't want to uninstall the building software so let's just
# manage the apt command on our own ...

ACTUAL_APT_GET=`which apt-get`

function apt-get() {
    case $1 in
        "purge")
            echo "Nothing to do for \"apt $@\""
            ;;
        "autoremove")
            echo "Nothing to do for \"apt $@\""
            ;;
    *)
        "$ACTUAL_APT_GET" $@
        ;;
    esac
}


# We don't want to build either some other projects, so let's skip
# them ...

#echo -e '#!/bin/bash\n\necho nothing to do for "build-virglrenderer.sh"' > .gitlab-ci/build-virglrenderer.sh

echo -e '#!/bin/bash\n\necho nothing to do for "build-piglit.sh"' > .gitlab-ci/build-piglit.sh

echo -e '#!/bin/bash\n\necho nothing to do for "build-deqp.sh"' > .gitlab-ci/build-deqp.sh

# libdrm is already built ...
echo -e '#!/bin/bash\n\necho nothing to do for "build-libdrm.sh"' > .gitlab-ci/build-libdrm.sh


# And bring everything needed over ...

. .gitlab-ci/container/x86_test-base.sh

. .gitlab-ci/container/x86_test-gl.sh

. .gitlab-ci/container/x86_test-vk.sh


# And our custom extras ...

apt-get install -y --no-remove \
      gdb \
      gdbserver \
      less \
      nano


# Set env variables so we can use all of the above

cat >>~/.bashrc <<EOF

# Reference build dir
BUILDS="/builds/mesa/mesa"

# Set up ccache
export CCACHE_COMPILERCHECK="content"
export CCACHE_COMPRESS="true"
export CCACHE_DIR="/cache/mesa/ccache"
export CCACHE_BASEDIR="\$BUILDS"
export PATH="/usr/lib/ccache:\$PATH"

export LLVM_VERSION="9"
export LLVM_CONFIG="llvm-config-\${LLVM_VERSION}"

INSTALL="\$BUILDS/install"

# Set up the driver environment.
export LD_LIBRARY_PATH="\$LD_LIBRARY_PATH:\$INSTALL/lib/"
#export LIBGL_DRIVERS_PATH="\$INSTALL/lib/dri"

# Set environment for renderdoc libraries.
export PYTHONPATH="\$PYTHONPATH:/renderdoc/build/lib"
export LD_LIBRARY_PATH="\$LD_LIBRARY_PATH:/renderdoc/build/lib"

# Set environment for the waffle library.
export LD_LIBRARY_PATH="/waffle/build/lib:\$LD_LIBRARY_PATH"

# Set environment for apitrace executable.
export PATH="/apitrace/build:\$PATH"

# Set environment for wflinfo executable.
export PATH="/waffle/build/bin:\$PATH"

# Set the Vulkan driver to use.
export VK_ICD_FILENAMES="\$INSTALL/share/vulkan/icd.d/\${VK_DRIVER}_icd.x86_64.json"

# Set environment for VulkanTools' VK_LAYER_LUNARG_screenshot layer.
export VK_LAYER_PATH="\$VK_LAYER_PATH:/VulkanTools/build/etc/vulkan/explicit_layer.d"
export LD_LIBRARY_PATH="\$LD_LIBRARY_PATH:/VulkanTools/build/lib"

# Set environment for Wine
export WINEDEBUG="-all"
export WINEPREFIX="/dxvk-wine64"
export WINEESYNC=1

# Set environment for DXVK
export DXVK_LOG_LEVEL="none"
export DXVK_STATE_CACHE=0

# Set environment for gfxreconstruct executables.
export PATH="/gfxreconstruct/build/bin:\$PATH"

EOF
