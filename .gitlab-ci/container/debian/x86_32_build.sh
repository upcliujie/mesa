#!/usr/bin/env bash

. .gitlab-ci/container/container_pre_build.sh

arch=i386

. .gitlab-ci/container/cross_build.sh

. .gitlab-ci/container/container_post_build.sh
