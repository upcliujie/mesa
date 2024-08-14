VirGL
=====

What is VirGL?
--------------

VirGL is a virtual 3D GPU for use inside QEMU virtual machines, that
allows the guest operating system to use the capabilities of the host GPU
to accelerate 3D rendering. The plan is to have a guest GPU that is fully
independent of the host GPU.

What exactly does it entail?
----------------------------

The project entails creating a virtual 3D capable graphics card for
virtual machines running inside QEMU. The design of this card is based
around the concepts of Gallium3D to make writing Mesa and (eventually)
Direct3D drivers for it easy. The card natively uses the Gallium TGSI
intermediate representation for its shaders. The implementation of
rendering for the card is done in the host system as part of QEMU and is
implemented purely on OpenGL so you can get accelerated rendering on any
sufficiently capable card/driver combination.

The project also consists of a complete Linux guest stack, composed of a
Linux kernel DRM/KMS driver, X.org 2D DDX driver and Mesa 3D drivers:
:doc:`Venus <drivers/venus>`Vulkan driver and VirGL Gallium3D driver.

Current status
--------------

* Many pieces are now upstreamed in various projects.
* Mesa contains the VirGL Gallium3D driver.
* Mesa contains the Venus Vulkan driver.
* The virglrenderer library seems mostly API stable.
* Mesa 23.2 and newers releases supports OpenGL 4.6 and GLES 3.2 features.

So what can it do now?
^^^^^^^^^^^^^^^^^^^^^^

Run a desktop and most 3D games I've thrown at it.

Scope
-----

The project is currently investigating the desktop virtualization use case
only. This use case is where the viewer, host and guest are all running on
the same machine (i.e. workstation or laptop). Some areas are in scope for
future investigation but not being looked at, at this time.

Future scope
^^^^^^^^^^^^

* Remoting rendering using a codec solution.
* Windows guest, Direct3D drivers.
* Other architectures.

Out of scope
^^^^^^^^^^^^

* Passing through GPUs or subsets of GPU capabilities.

Repos
-----

All upstream parts are being developed upstream.

virglrenderer: the GL renderer https://gitlab.freedesktop.org/virgl/virglrenderer

Authors and Contributors
------------------------

VirGL is a project undertaken by Dave Airlie at Red Hat. It builds on lots
of open source work in a number of projects, primarily the Gallium 3D code
from the Mesa project.

Support or Contact
------------------

mailing list: virglrenderer-devel@lists.freedesktop.org

https://lists.freedesktop.org/mailman/listinfo/virglrenderer-devel

IRC: `#virgil3d on OFTC <irc://irc.oftc.net/virgil3d>`__.
