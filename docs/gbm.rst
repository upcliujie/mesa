GBM
===

GBM, or Generic Buffer Manager, is a memory allocation API for device buffers.
Device buffers can refer to memory local to a given device, or simply memory
accessible via a specified usage of that device. Both types of memory are
allocated relative to a GBM device object. Buffers can be allocated as
stand-alone objects, or as part of a GBM surface. GBM devices can be used as
the native display object backing an EGLDisplay, GBM buffers can be used as the
backing store for an EGLImage, and GBM surfaces can be used as the native
surface backing an EGL window surface.

GBM Implementation Components
-----------------------------

Mesa provides both a GBM loader library and a default GBM backend or driver
component. The loader defines and exports the application facing GBM interface
and loads backend libraries as needed to provide an implementation of that API's
functionality. The loader also defines a second interface: The GBM
loader<->backend interface. This interface defines a stable, backwards and
forwards-compatible ABI that backend drivers must implement to expose GBM
functionality.

Making Changes to GBM Interfaces
--------------------------------

As noted, the GBM loader defines two interfaces. Generally, adding new GBM
functionality will require modifying both of them. Care must be taken to
maintain compatibility when modifying this code.

Modifying the GBM API
~~~~~~~~~~~~~~~~~~~~~

The GBM application programming interface is defined in

  :file:`src/gbm/main/gbm.h`

This interface defines a stable ABI between applications and the GBM loader
library and as such, modifications must be backwards compatible. For example:

- Existing ``gbm_bo_flag`` values must not be modified. Add new flags by
  appending an enum to the end of the existing flags.

- Prototypes of existing functions must only be modified with great care, as
  the resulting prototype must be functionally equivalent as viewed by a C
  compiler and linker across all platforms.

- Structs such as ``gbm_import_fd_modifier_data`` can only be modified by
  appending fields to them.

Ensure these rules are followed when modifying or reviewing changes to gbm.h to
avoid breaking existing applications making use of the GBM API.

Modifying the GBM Loader<->Backend Interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The GBM loader<->backend interface is defined in

  :file:`src/gbm/main/gbm_backend_abi.h`

Like the application-facing interface, the GBM loader<->backend interface
defines a stable ABI, in this case between backend implementations and the GBM
loader library. However, there are even stricter requirements on compatibility
in this interface: Changes must be both backwards and forwards compatible. While
applications are expected to determine for themselves whether a given GBM loader
library is new enough to support a given feature, the loader<->backend interface
must support both newer and older backends, and likewise backends are expected
to support both newer and older loaders. Therefore, care must be taken to
explicitly version all components of this interface. The above list of
guidelines for modifying gbm.h applies when modifying gbm_backend_abi.h as well.
In addition, the following guidelines apply:

- Any modifications to the header that may change the binary interface defined
  by gbm_backend_abi.h on any platform require incrementing the value of
  ``GBM_BACKEND_ABI_VERSION``.

- Even if no modifications to the binary interface are made,
  ``GBM_BACKEND_ABI_VERSION`` must be incremented if previously disallowed usage
  of any component of the interface is now allowed, or if the intended
  interpretation of some usage by the backend driver is changed in any way.

- The loader is responsible for preserving prior behavior when using a backend
  that only supports prior values of ``GBM_BACKEND_ABI_VERSION``, as reported
  in ``gbm_device::gbm_device_v0::backend_version`` and
  ``gbm_backend::gbm_backend_v0::backend_version``.

How to adhere to the last point can be non-obvious at times. As an example, the
loader must not pass new enum values to existing functions that are expected to
succeed on the old backend. If it is expected that the old backend will fail the
operation, such as allocating a buffer with a new usage flag, the loader need
not filter out the usage flag itself. However, if the usage flag is defined such
that existing backends will satisfy it implicitly, the loader should remove it
from the application-provided flags before forwarding the request to the
backend to allow the call to succeed on older backends.

Because these guidelines are somewhat subtler than the general ABI guidelines
that apply for gbm.h modifications, the ABI version is baked into the members
of all top-level structures included in the loader<->backend interface. When
modifying these top-level structures, which necessarily involves adding a new
field to them, define a new child structure containing the new field, rather
than adding it to the existing explicitly-versioned child structures or directly
to the top-level structure. In addition, there is a unit-test that attempts to
detect common violations of these guidelines:

  :file:`src/gbm/main/gbm_abi_check.c`

An Example: Adding an Application-Visible Device Name
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As example, suppose someone wanted to add a more descriptive name to the
gbm_device struct that applications could retrieve using a new GBM API function,
and the current value of ``GBM_BACKEND_ABI_VERSION`` was 2.  The modifications
required might consist of the following:

- In gbm_backend_abi.h:

  1. Change the value of ``GBM_BACKEND_ABI_VERSION`` to 3.

  2. Define a new struct named ``gbm_device_v3`` containing a pointer to the new
     name string. Let's call this ``pretty_name``.

  3. Add an instance of struct ``gbm_device_v3`` named ``v3`` to the end of the
     definition of the definition of ``struct gbm_device``.

- In gbm_abi-check.c:

  1. Add a new ABI version comment header just above the "Structure/member
     ABI-checking helper macros" comment. It should be of the form:

    .. code-block:: C

      /*
       * From: <author> - <commit short description line>
       */

  2. Copy/paste the entire contents of gbm_backend_abi.h just below this
     comment, appending ``_abi3`` to the name of all structs, defines, and
     typedefs.

  3. In the main function, Copy/paste all the existing ``CHECK_*_CURRENT`` calls
     as a block above themselves and rename+modify the new block to read
     ``CHECK_*(..., _abi1, _abi2, ...)``. This preserves the existing ABI
     checks.

  4. In the old block, increase the value of the ``_abi1`` parameters to
     ``_abi2`` and add a ``CHECK_MEMBER_CURRENT()`` call for the ``pretty_name``
     member of ``gbm_device_v3``.

- In gbm.h:

  1. Declare a new top-level GBM function:

     .. code-block:: C

       const char *gbm_get_device_name(struct gbm_device *gbm);

- In gbm.c:

  1. Define the function in gbm.c. The function would first check the ABI
     version of the specified GBM device struct. If it was less than 3, it
     would return NULL. It must never dereference ``gbm->v3`` in this case.
     Otherwise, it would return ``gbm->v3->pretty_name``.

Writing a GBM Backend Driver
----------------------------

A GBM backend driver is a library that implements the backend side of the
GBM loader<->backend interface defined in :file:`gbm_backend_abi.h`. It
must export at a minimum the function ``gbmint_get_backend``, which the loader
will use to extract the rest of the interface entry points from the backend.
The backend code should utilize the macro ``GBM_GET_BACKEND_PROC`` when defining
this function to ensure the correct name is used.

When called, this function must return a pointer to an instance of the
gbm_backend structure that will remain valid for the life of the backend. The
backend_version member should be set to the maximum GBM loader<->backend ABI
version the backend supports, regardless of the version supported by the loader
in use. The backend_name field can be set to an arbitrary string, but is
expected to remain constant across subsequent versions of the backend and should
be unique enough that applications can key off this value for vendor-specific
workarounds. Generally, the gbm_backend object would be statically defined as a
global variable in the backend at build time using the latest backend ABI
version known at build time. For example:

  .. code-block:: C

    static const struct gbm_backend vendor_gbm_backend = {
        .v0.backend_version = GBM_BACKEND_ABI_VERSION,
        .v0.backend_name = "vendor_name",
        .v0.create_device = vendor_gbm_create_device,
    };

Dynamically-loadable backend drivers must be written to support loaders using
both older and newer GBM loader<->backend interface versions. In general, it is
the responsibility of the loader code to support older backend versions and the
responsibility of the backend to explicitly support older loader versions. For
example, a backend supporting backend ABI 3 is free to return a larger ABI 3-
sized GBM device struct to a loader supporting only backend ABI 2, but it must
not touch any fields in the gbm_core struct that were added by backend ABI 3 or
later in this situation. The backend may determine the version of the loader
by examining the "core_version" member of the core_ptr parameter the loader
provides when calling the ``GBM_GET_BACKEND_PROC`` entry point. The pointer
provided at this time is guaranteed to remain accessible by any thread calling
into the backend from the loader as long as the backend is loaded and hence can
be safely cached for later use by the backend code.

When implementing the other backend functions, such as the function
``.v0.create_device`` points to in the above example, the built-in DRI backend
is generally a good reference. However, note that as a built-in backend, it
does not need to check ABI versions. When developing a dynamically-loaded
backend driver, be sure the necessary ABI version checks are added to retain
support with older and newer loaders. As alluded to above, this includes
failing the operation rather than ignoring unknown or invalid flag or parameter
values received from the loader layer.

The current loader code utilizes a naming and filesystem path convention to
discover dynamically loadable backend libraries at runtime. To be loaded
automatically by the Mesa GBM loader library, the backend library filename must
be of the form ``<DRM driver name>_gbm.so`` and it must be located in the
directory ``$GBM_BACKENDS_PATH``, which is a system-specific path determined at
loader build time that defaults to ``$LIBDIR/gbm``, where ``$LIBDIR`` itself is
a system-specific path determined at build time.
