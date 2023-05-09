### virtgpu_vtest backend

A shim virtgpu LD_PRELOAD shim which uses the vtest protocol to
communicate to virglrenderer.

Limitations:
- Requires SW_SYNC enabled in kernel (and accessible to user using
  this drm-shim) for dma-fence fd's
- No "host storage" support yet
- Missing a few ioctls needed by virgl (but not venus or drm
  native contexts)
- No dma-buf fd import (export works)

The upshot is that it currently only works with drm native contexts,
but if host storage were emulated then venus could be supported as
well.

