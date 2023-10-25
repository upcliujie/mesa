#!/usr/bin/env bash

set -ex

TFLITE_VERSION="2.15.0"

git clone -b v"$TFLITE_VERSION" --single-branch --depth 1 https://github.com/tensorflow/tensorflow.git tflite
pushd tflite

cmake tensorflow/lite/c -DBUILD_SHARED_LIBS=ON -B build
cmake --build build --parallel

mkdir /tflite-libs
mv build/tensorflow-lite/libtensorflow-lite.so /tflite-libs/.
mv build/_deps/farmhash-build/libfarmhash.so /tflite-libs/.
mv build/_deps/abseil-cpp-build/absl/*/libabsl*.so /tflite-libs/.
mv build/_deps/fft2d-build/libfft2d_fftsg2d.so /tflite-libs/.
mv build/_deps/fft2d-build/libfft2d_fftsg.so /tflite-libs/.
mv build/_deps/gemmlowp-build/libeight_bit_int_gemm.so /tflite-libs/.
mv build/pthreadpool/libpthreadpool.so /tflite-libs/.
mv build/_deps/cpuinfo-build/libcpuinfo.so /tflite-libs/.
mv build/_deps/xnnpack-build/libXNNPACK.so /tflite-libs/.

popd
rm -rf tflite
