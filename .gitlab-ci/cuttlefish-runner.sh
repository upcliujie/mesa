#!/usr/bin/env bash
# shellcheck disable=SC2086 # we want word splitting

set -xe

host_setup() {
    export HOME=/cuttlefish
    export PATH=$PATH:/cuttlefish/bin
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${CI_PROJECT_DIR}/install/lib/:/cuttlefish/lib64
    export EGL_PLATFORM=surfaceless
    export ANDROID_SERIAL=vsock:3:5555
    export RESULTS=/data/results

    syslogd
    chown root.kvm /dev/kvm
    /etc/init.d/cuttlefish-host-resources start
    cd /cuttlefish
    launch_cvd --verbosity=DEBUG --report_anonymous_usage_stats=n --cpus=8 --memory_mb=8192 --gpu_mode="$ANDROID_GPU_MODE" --daemon --enable_minimal_mode=true --guest_enforce_security=false --use_overlay=false
    sleep 1
    cd -
}

android_connect_init() {
    adb connect $ANDROID_SERIAL
    adb wait-for-device
    adb root
    adb wait-for-device
    adb shell echo Hi from Android
    # shellcheck disable=SC2035
    adb logcat dEQP:D *:S &
}

android_overlay_vendor_folder() {
    OV_TMPFS="/data/overlay-remount"
    adb shell mkdir -p "$OV_TMPFS"
    adb shell mount -t tmpfs none "$OV_TMPFS"

    adb shell mkdir -p "$OV_TMPFS/vendor-upper"
    adb shell mkdir -p "$OV_TMPFS/vendor-work"

    opts="lowerdir=/vendor,upperdir=$OV_TMPFS/vendor-upper,workdir=$OV_TMPFS/vendor-work"
    adb shell mount -t overlay -o "$opts" none /vendor

    adb shell setenforce 0
}

android_install_test_suite() {
    adb push /deqp/modules/egl/deqp-egl-android /data/.
    adb push /deqp/assets/gl_cts/data/mustpass/egl/aosp_mustpass/3.2.6.x/egl-master.txt /data/.
    adb push /deqp-runner/deqp-runner /data/.
    adb push install/all-skips.txt /data/.
    adb push install/$GPU_VERSION-flakes.txt /data/.
    adb push install/deqp-$DEQP_SUITE.toml /data/.
}

download_mesa() {
    MESA_ANDROID_ARTIFACT_URL=https://${PIPELINE_ARTIFACTS_BASE}/${S3_ARTIFACT_NAME}.tar.zst
    curl -L --retry 4 -f --retry-all-errors --retry-delay 60 -o ${S3_ARTIFACT_NAME}.tar.zst ${MESA_ANDROID_ARTIFACT_URL}
    tar -xvf ${S3_ARTIFACT_NAME}.tar.zst
    rm "${S3_ARTIFACT_NAME}.tar.zst" &
}

android_replace_vendor_mesa_libs() {
    # remove 32 bits libs from /vendor/lib

    adb shell rm /vendor/lib/dri/${ANDROID_DRIVER}_dri.so
    adb shell rm /vendor/lib/libglapi.so
    adb shell rm /vendor/lib/egl/libGLES_mesa.so

    adb shell rm /vendor/lib/egl/libEGL_angle.so
    adb shell rm /vendor/lib/egl/libEGL_emulation.so
    adb shell rm /vendor/lib/egl/libGLESv1_CM_angle.so
    adb shell rm /vendor/lib/egl/libGLESv1_CM_emulation.so
    adb shell rm /vendor/lib/egl/libGLESv2_angle.so
    adb shell rm /vendor/lib/egl/libGLESv2_emulation.so

    # replace on /vendor/lib64

    # Cuttlefish delivers a builtin EGL_mesa implementation.
    # Android looks for both bundled EGL/GLES library or the split one.
    # Bundled: /vendor/lib{,64}/egl/libGLES_mesa.so
    # Split:
    # - /vendor/lib{,64}/egl/libEGL_mesa.so
    # - /vendor/lib{,64}/egl/libGLESv1_CM_mesa.so
    # - /vendor/lib{,64}/egl/libGLESv2_mesa.so
    # For reference https://android.googlesource.com/platform/frameworks/native/+/master/opengl/libs/EGL/Loader.cpp#41
    adb shell rm /vendor/lib64/egl/libGLES_mesa.so
    adb push install/lib/libEGL.so /vendor/lib64/egl/libEGL_mesa.so
    adb push install/lib/libGLESv1_CM.so /vendor/lib64/egl/libGLESv1_CM_mesa.so
    adb push install/lib/libGLESv2.so /vendor/lib64/egl/libGLESv2_mesa.so

    adb push install/lib/dri/${ANDROID_DRIVER}_dri.so /vendor/lib64/dri/${ANDROID_DRIVER}_dri.so
    adb push install/lib/libglapi.so /vendor/lib64/libglapi.so

    adb shell rm /vendor/lib64/egl/libEGL_angle.so
    adb shell rm /vendor/lib64/egl/libEGL_emulation.so
    adb shell rm /vendor/lib64/egl/libGLESv1_CM_angle.so
    adb shell rm /vendor/lib64/egl/libGLESv1_CM_emulation.so
    adb shell rm /vendor/lib64/egl/libGLESv2_angle.so
    adb shell rm /vendor/lib64/egl/libGLESv2_emulation.so
}

android_run_deqp_runner() {
    adb shell "mkdir /data/results; export EGL_PLATFORM=$EGL_PLATFORM; cd /data; ./deqp-runner \
        suite \
        --suite /data/deqp-$DEQP_SUITE.toml \
        --output $RESULTS \
        --skips /data/all-skips.txt $DEQP_SKIPS \
        --flakes /data/$GPU_VERSION-flakes.txt \
        --testlog-to-xml /deqp/executor/testlog-to-xml \
        --fraction-start $CI_NODE_INDEX \
        --fraction $(( CI_NODE_TOTAL * ${DEQP_FRACTION:-1})) \
        --jobs ${FDO_CI_CONCURRENT:-4} \
        $DEQP_RUNNER_OPTIONS"
}

save_results() {
    adb pull $RESULTS results

    cp /cuttlefish/cuttlefish/instances/cvd-1/logs/logcat results
    cp /cuttlefish/cuttlefish/instances/cvd-1/kernel.log results
    cp /cuttlefish/cuttlefish/instances/cvd-1/logs/launcher.log results
}

section_start cuttlefish_setup "cuttlefish: setup"
host_setup
android_connect_init
android_overlay_vendor_folder
download_mesa
android_install_test_suite
android_replace_vendor_mesa_libs

uncollapsed_section_switch cuttlefish_test "cuttlefish: testing"
set +e
android_run_deqp_runner
EXIT_CODE=$?
set -e
section_switch cuttlefish_results "cuttlefish: gathering the results"

save_results

section_end cuttlefish_results

exit $EXIT_CODE
