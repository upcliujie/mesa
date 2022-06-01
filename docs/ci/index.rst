Continuous Integration
======================

GitLab CI
---------

GitLab provides a convenient framework for running commands in response to Git pushes.
We use it to test merge requests (MRs) before merging them (pre-merge testing),
as well as post-merge testing, for everything that hits ``main``
(this is necessary because we still allow commits to be pushed outside of MRs,
and even then the MR CI runs in the forked repository, which might have been
modified and thus is unreliable).

The CI runs a number of tests, from trivial build-testing to complex GPU rendering:

- Build testing for a number of build systems, configurations and platforms
- Sanity checks (``meson test``)
- Some drivers (softpipe, llvmpipe, freedreno and panfrost) are also tested
  using `VK-GL-CTS <https://github.com/KhronosGroup/VK-GL-CTS>`__
- Replay of application traces

A typical run takes between 20 and 30 minutes, although it can go up very quickly
if the GitLab runners are overwhelmed, which happens sometimes. When it does happen,
not much can be done besides waiting it out, or cancel it.

Due to limited resources, we currently do not run the CI automatically
on every push; instead, we only run it automatically once the MR has
been assigned to ``Marge``, our merge bot.

If you're interested in the details, the main configuration file is ``.gitlab-ci.yml``,
and it references a number of other files in ``.gitlab-ci/``.

If the GitLab CI doesn't seem to be running on your fork (or MRs, as they run
in the context of your fork), you should check the "Settings" of your fork.
Under "CI / CD" → "General pipelines", make sure "Custom CI config path" is
empty (or set to the default ``.gitlab-ci.yml``), and that the
"Public pipelines" box is checked.

If you're having issues with the GitLab CI, your best bet is to ask
about it on ``#freedesktop`` on OFTC and tag `Daniel Stone
<https://gitlab.freedesktop.org/daniels>`__ (``daniels`` on IRC) or
`Eric Anholt <https://gitlab.freedesktop.org/anholt>`__ (``anholt`` on
IRC).

The three GitLab CI systems currently integrated are:


.. toctree::
   :maxdepth: 1

   bare-metal
   LAVA
   docker

Application traces replay
-------------------------

The CI replays application traces with various drivers in two different jobs. The first
job replays traces listed in ``src/<driver>/ci/traces-<driver>.yml`` files and if any
of those traces fail the pipeline fails as well. The second job replays traces listed in
``src/<driver>/ci/restricted-traces-<driver>.yml`` and it is allowed to fail. This second
job is only created when the pipeline is triggered by `marge-bot` or any other user that
has been granted access to these traces.

A traces YAML file also includes a ``download-url`` pointing to a MinIO
instance where to download the traces from. While the first job should always work with
publicly accessible traces, the second job could point to an url with restricted access.

Restricted traces are those that have been made available to Mesa developers without a
license to redistribute at will, and thus should not be exposed to the public. Failing to
access that URL would not prevent the pipeline to pass, therefore forks made by
contributors without permissions to download non-redistributable traces can be merged
without friction.

As an aside, only maintainers of such non-redistributable traces are responsible for
ensuring that replays are successful, since other contributors would not be able to
download and test them by themselves.

Those Mesa contributors that believe they could have permission to access such
non-redistributable traces can request permission to Daniel Stone <daniels@collabora.com>.

gitlab.freedesktop.org accounts that are to be granted access to these traces will be
added to the OPA policy for the MinIO repository as per
https://gitlab.freedesktop.org/freedesktop/helm-gitlab-config/-/commit/a3cd632743019f68ac8a829267deb262d9670958 .

So the jobs are created in personal repositories, the name of the user's account needs
to be added to the rules attribute of the Gitlab CI job that accesses the restricted
accounts.

Intel CI
--------

The Intel CI is not yet integrated into the GitLab CI.
For now, special access must be manually given (file a issue in
`the Intel CI configuration repo <https://gitlab.freedesktop.org/Mesa_CI/mesa_jenkins>`__
if you think you or Mesa would benefit from you having access to the Intel CI).
Results can be seen on `mesa-ci.01.org <https://mesa-ci.01.org>`__
if you are *not* an Intel employee, but if you are you
can access a better interface on
`mesa-ci-results.jf.intel.com <http://mesa-ci-results.jf.intel.com>`__.

The Intel CI runs a much larger array of tests, on a number of generations
of Intel hardware and on multiple platforms (X11, Wayland, DRM & Android),
with the purpose of detecting regressions.
Tests include
`Crucible <https://gitlab.freedesktop.org/mesa/crucible>`__,
`VK-GL-CTS <https://github.com/KhronosGroup/VK-GL-CTS>`__,
`dEQP <https://android.googlesource.com/platform/external/deqp>`__,
`Piglit <https://gitlab.freedesktop.org/mesa/piglit>`__,
`Skia <https://skia.googlesource.com/skia>`__,
`VkRunner <https://github.com/Igalia/vkrunner>`__,
`WebGL <https://github.com/KhronosGroup/WebGL>`__,
and a few other tools.
A typical run takes between 30 minutes and an hour.

If you're having issues with the Intel CI, your best bet is to ask about
it on ``#dri-devel`` on OFTC and tag `Nico Cortes
<https://gitlab.freedesktop.org/ngcortes>`__ (``ngcortes`` on IRC).

.. _CI-farm-expectations:

CI farm expectations
--------------------

To make sure that testing of one vendor's drivers doesn't block
unrelated work by other vendors, we require that a given driver's test
farm produces a spurious failure no more than once a week.  If every
driver had CI and failed once a week, we would be seeing someone's
code getting blocked on a spurious failure daily, which is an
unacceptable cost to the project.

Additionally, the test farm needs to be able to provide a short enough
turnaround time that we can get our MRs through marge-bot without the
pipeline backing up.  As a result, we require that the test farm be
able to handle a whole pipeline's worth of jobs in less than 15 minutes
(to compare, the build stage is about 10 minutes).

If a test farm is short the HW to provide these guarantees, consider dropping
tests to reduce runtime.  dEQP job logs print the slowest tests at the end of
the run, and piglit logs the runtime of tests in the results.json.bz2 in the
artifacts.  Or, you can add the following to your job to only run some fraction
(in this case, 1/10th) of the deqp tests.

.. code-block:: yaml

    variables:
      DEQP_FRACTION: 10

to just run 1/10th of the test list.

If a HW CI farm goes offline (network dies and all CI pipelines end up
stalled) or its runners are consistently spuriously failing (disk
full?), and the maintainer is not immediately available to fix the
issue, please push through an MR disabling that farm's jobs by adding
'.' to the front of the jobs names until the maintainer can bring
things back up.  If this happens, the farm maintainer should provide a
report to mesa-dev@lists.freedesktop.org after the fact explaining
what happened and what the mitigation plan is for that failure next
time.

Personal runners
----------------

Mesa's CI is currently run primarily on packet.net's m1xlarge nodes
(2.2Ghz Sandy Bridge), with each job getting 8 cores allocated.  You
can speed up your personal CI builds (and marge-bot merges) by using a
faster personal machine as a runner.  You can find the gitlab-runner
package in Debian, or use GitLab's own builds.

To do so, follow `GitLab's instructions
<https://docs.gitlab.com/ce/ci/runners/#create-a-specific-runner>`__ to
register your personal GitLab runner in your Mesa fork.  Then, tell
Mesa how many jobs it should serve (``concurrent=``) and how many
cores those jobs should use (``FDO_CI_CONCURRENT=``) by editing these
lines in ``/etc/gitlab-runner/config.toml``, for example::

  concurrent = 2

  [[runners]]
    environment = ["FDO_CI_CONCURRENT=16"]


Docker caching
--------------

The CI system uses Docker images extensively to cache
infrequently-updated build content like the CTS.  The `freedesktop.org
CI templates
<https://gitlab.freedesktop.org/freedesktop/ci-templates/>`_ help us
manage the building of the images to reduce how frequently rebuilds
happen, and trim down the images (stripping out manpages, cleaning the
apt cache, and other such common pitfalls of building Docker images).

When running a container job, the templates will look for an existing
build of that image in the container registry under
``MESA_IMAGE_TAG``.  If it's found it will be reused, and if
not, the associated `.gitlab-ci/containers/<jobname>.sh`` will be run
to build it.  So, when developing any change to container build
scripts, you need to update the associated ``MESA_IMAGE_TAG`` to
a new unique string.  We recommend using the current date plus some
string related to your branch (so that if you rebase on someone else's
container update from the same day, you will get a Git conflict
instead of silently reusing their container)

When developing a given change to your Docker image, you would have to
bump the tag on each ``git commit --amend`` to your development
branch, which can get tedious.  Instead, you can navigate to the
`container registry
<https://gitlab.freedesktop.org/mesa/mesa/container_registry>`_ for
your repository and delete the tag to force a rebuild.  When your code
is eventually merged to main, a full image rebuild will occur again
(forks inherit images from the main repo, but MRs don't propagate
images from the fork into the main repo's registry).

Building locally using CI docker images
---------------------------------------

It can be frustrating to debug build failures on an environment you
don't personally have.  If you're experiencing this with the CI
builds, you can use Docker to use their build environment locally.  Go
to your job log, and at the top you'll see a line like::

    Pulling docker image registry.freedesktop.org/anholt/mesa/debian/android_build:2020-09-11

We'll use a volume mount to make our current Mesa tree be what the
Docker container uses, so they'll share everything (their build will
go in _build, according to ``meson-build.sh``).  We're going to be
using the image non-interactively so we use ``run --rm $IMAGE
command`` instead of ``run -it $IMAGE bash`` (which you may also find
useful for debug).  Extract your build setup variables from
.gitlab-ci.yml and run the CI meson build script:

.. code-block:: console

    IMAGE=registry.freedesktop.org/anholt/mesa/debian/android_build:2020-09-11
    sudo docker pull $IMAGE
    sudo docker run --rm -v `pwd`:/mesa -w /mesa $IMAGE env PKG_CONFIG_PATH=/usr/local/lib/aarch64-linux-android/pkgconfig/:/android-ndk-r21d/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/pkgconfig/ GALLIUM_DRIVERS=freedreno UNWIND=disabled EXTRA_OPTION="-D android-stub=true -D llvm=disabled" DRI_LOADERS="-D glx=disabled -D gbm=disabled -D egl=enabled -D platforms=android" CROSS=aarch64-linux-android ./.gitlab-ci/meson-build.sh

All you have left over from the build is its output, and a _build
directory.  You can hack on mesa and iterate testing the build with:

.. code-block:: console

    sudo docker run --rm -v `pwd`:/mesa $IMAGE ninja -C /mesa/_build


Conformance Tests
-----------------

Some conformance tests require a special treatment to be maintained on Gitlab CI.
This section lists their documentation pages.

.. toctree::
  :maxdepth: 1

  skqp

Updating Gitlab CI Dependencies
-------------------------------
Currently, the CI builds some dependencies required to run jobs in the pipeline,
such as (not comprehensive):

#. Linux Kernel
#. piglit
#. deqp-runner
#. skqp

For the sake of job execution times, each image is tagged. Thus the jobs which
build dependencies will only rebuild them if the tag that is associated with
the image is updated. So even when the dependency version is bumped, it will not
be built in the job images unless the user bumps the related tag.

Image tags
"""""""""""""""""""
Whenever a dependency needs to be rebuilt, a developer should update the
the corresponding dependency tag and also update tags related to the container images.
E.g: to update libdrm, one should grep for `build.libdrm` in `.gitlab-ci`
folder, looking for affected tags. The following script helps with that.

.. code-block:: console

  # Running in container folder to simplify the script a bit
  cd .gitlab-ci/container
  # The name of the dependency
  DEP=libdrm;
  # Print rootfs tags related to the dependency
  grep -P "${DEP^^}_TAG" ../image-tags.yml
  # Look for scripts which uses functions/helper scripts that builds the dependency $DEP
  grep -RP "build.$DEP[ (.]" -l |
        # remove file extension
        sed 's/.sh$//g' |
        # find jobs that will be affected by this dependency
        xargs -I affected_files find ../.. -type f -name '*.yml' -exec grep -P "^affected_files" {} \; |
        # sort and remove duplicates
        sort -u |
        # Base jobs tags are split into their children, reuse prefix
        sed 's/-base//g' |
        # transform into YAML-style variables
        tr '/' '_' | tr -d ':' | tr a-z A-Z | tr '-' '_' |
        # find which tags needs to be updated
        xargs -I pattern grep -P pattern.*_TAG ../image-tags.yml |
        # sort and remove duplicates
        sort -u

This example would give the following result, depending on the commit you are in:

.. code-block:: console

  ROOTFS_LIBDRM_TAG: "2022-05-23"
  DEBIAN_X86_TEST_GL_TAG: "2022-05-31-cts-1.3.2.0"
  DEBIAN_X86_TEST_VK_TAG: "2022-05-31-cts-1.3.2.0"
  FEDORA_X86_BUILD_TAG: "2022-04-24-spirv-tools-5"

RootFS dependencies
"""""""""""""""""""

Each dependency can have its own build flags and optimizations and must be
tagged accordingly via the `image-tags.yml
<https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/.gitlab-ci/image-tags.yml>`_
file, following the pattern:

:code:`ROOTFS_<DEPENDENCY_NAME>_TAG`

Where: `<DEPENDENCY_NAME>` is the name of dependency in uppercase.

Currently, the rootfs building is optimized via a custom artifact repository
located at freedesktop minio instance via `lava_build.sh` script.
Each dependency build script is wrapped in a respective bash function with the
pattern `build_<dependency_name>`, where the installation files should be
located inside `$TMP_ROOTFS_PATH`, instead of the final rootfs directory.

This way, the script can manage how the files will be synchronized with the
cloud and will extract the files to the final destination correctly.

.. note::
  There is a strong binding between the lava_build.sh function name and the tag in image-tags.yml.
  Both must have the same `<DEPENDENCY_NAME>` token, lowercase for the former
  and uppercase for the latter. Otherwise, the build script will fail.

MINIO build artifacts
+++++++++++++++++++++

The following dependency graph shows the relation among the rootfs file, build
components files, and the container images.
Whenever a node is tagged, the related environment variable is shown on the
right side.

.. graphviz::

  digraph dep_graph {
      concentrate=True;
      node [shape=record];
      node [color=lightblue];
      "rootfs" [label="rootfs\n|{$ROOTFS_TAG}"];
      "deps" [label="build components\n|{$ROOTFS_\<DEPENDENCY_NAME\>_TAG}"]
      "image"[label="container image\n|{$MESA_IMAGE_TAG}"];
      "ci-fairy"[label="ci-fairy\n|{$MESA_TEMPLATES_COMMIT}"];

      "deps" -> "image"
      "image" -> "arch" [arrowhead = diamond];
      "rootfs" -> "deps"
      "rootfs" -> "image"
      "rootfs" -> "ci-fairy"
  }

Both build components and rootfs depend on container image since it is the base
OS where the build packages are installed. E.g: a Debian upgrade would require
rebuilding every build component, including the rootfs.

rootfs is a particular case, as it also relies on `ci-fairy` tool from ci-templates
project and in the group of the build dependencies.

To map every file uniquely in the cloud, we use the proper tags to name some folders and suffix some files.
The current target files are the following:

.. code-block:: console

  # A build component path in the cloud
  https://minio-packet.freedesktop.org/mesa-lava/user/mesa/${DEBIAN_BASE_TAG}/<dependency_name>_${ROOTFS_<DEPENDENCY_NAME>_TAG}.tar.xz
  # A rootfs path in the cloud
  https://minio-packet.freedesktop.org/mesa-lava/user/mesa/${DISTRIBUTION_TAG}/rootfs_${ROOTFS_TAG}.tgz

.. note::
  `$ROOTFS_TAG` is created from a hash of the concatenation of all variables
  that obey `$ROOTFS_.*_TAG` pattern.

  `$DISTRIBUTION_TAG` is an amalgam of `$MESA_IMAGE_TAG`,
  `$MESA_TEMPLATES_COMMIT` and the image architecture.

Algorithm
*********

.. graphviz::

  digraph G {
      concentrate=true;
      splines=false;
      node [shape=rect]
      dep [label="call wrapper\nfor build_$DEP\nbash function", color=lightblue]
      fn [label="call build_$DEP\nbash function"]

      download
      upload

      clean  [label="clean\ntemporary\nrootfs folder"]
      extract [label="extract to\nfinal rootfs\nfolder"]
      compress [label="compress from\ntemporary\nrootfs folder"]
      fail [shape=oval, color=red]

      synced [shape=diamond, label="is resulting\ntarball file\nalready in the cloud?"]
      tagged [shape=diamond, label="is there a tag\nfor that component?"]

      dep -> tagged
      tagged -> synced [ label = "Yes"]
      tagged -> fail [ label = "No"]
      synced -> download [ label = "Yes"]
      synced -> fn [ label = "No"]
      download -> extract

      fn -> compress -> upload -> extract -> clean

      {
          rank=same;
          download; fn;
      }
  }

.. graphviz::

  digraph G {
      concentrate=true;
      splines=false;
      node [shape=rect]
      lb [label="Running lava-build.sh", color=lightblue]
      itc [label="Is the respective rootfs file in the cloud?"]
      dep [label="call wrapper\nfor build_$DEP\nbash function", color=lightblue]
      end [label="exit script", shape=oval]
      compress [label="compress\nfrom final\nrootfs folder"]
      upload

      lb -> itc
      itc -> dep [ label = "No" ]
      dep -> compress -> upload -> end
      itc -> end [ label = "Yes" ]
  }

Updating Linux Kernel
+++++++++++++++++++++

Gitlab CI usually runs a bleeding-edge kernel. The following documentation has
instructions on how to uprev Linux Kernel in the Gitlab CI ecosystem.

.. toctree::
  :maxdepth: 1

  kernel
