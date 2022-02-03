#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#ifdef WIN32
#define EXPORT __stdcall __declspec(dllexport)
#else
#define EXPORT
#endif

#define debug(...) printf(__VA_ARGS__)

struct GrTexInfo {
	int small;
	int large;
	int aspect;
	int format;
	void *data;
};

typedef struct {
	float sow; /* s texture ordinate (s over w) */
	float tow; /* t texture ordinate (t over w) */
	float oow; /* 1/w (used mipmapping - really 0xfff/w) */
}  GrTmuVertex;

typedef struct {
	float x, y, z; /* X, Y, and Z of scrn space -- Z is ignored */
	float r, g, b; /* R, G, B, ([0..255.0]) */
	float ooz;     /* 65535/Z (used for Z-buffering) */
	float a;       /* Alpha [0..255.0] */
	float oow;     /* 1/W (used for W-buffering, texturing) */
	GrTmuVertex tmuvtx[2];
} GrVertex;

__attribute__((constructor)) void init(void);

EXPORT void _grGlideInit(void);
EXPORT void _grGlideShutdown(void);
EXPORT int _grSstQueryHardware(void *ptr);
EXPORT void _grSstSelect(int which);
EXPORT int _grSstOpen(int res, int ref, int format, int origin, int smooth, int num_buffers);
EXPORT void _grSstPassthruMode(int mode);
EXPORT void _grBufferClear(int color, int alpha, int depth);
EXPORT void _grBufferSwap(int interval);
EXPORT void _grLfbBegin(void);
EXPORT void _grLfbEnd(void);
EXPORT void _grLfbBypassMode(int mode);
EXPORT void _grLfbWriteMode(int mode);
EXPORT void *_grLfbGetWritePtr(int buffer);
EXPORT int _grTexMinAddress(int mode);
EXPORT int _grTexMaxAddress(int mode);
EXPORT int _grTexTextureMemRequired(int tmu, struct GrTexInfo *info);
EXPORT void _grTexDownloadMipMap(int tmu, int startAddress, int evenOdd, struct GrTexInfo *info);
EXPORT void _grTexSource(int tmu, int startAddress, int evenOdd, struct GrTexInfo *info);
EXPORT void _grTexCombineFunction(int tmu, int func);
EXPORT void _grDepthBufferMode(int mode);
EXPORT void _grCullMode(int mode);
EXPORT void _grErrorSetCallback(void *func);
EXPORT void _grClipWindow(int a, int b, int c, int d);
EXPORT void _grDrawTriangle(GrVertex *a, GrVertex *b, GrVertex *c);
EXPORT void _grChromakeyValue(int value);
EXPORT void _grChromakeyMode(int mode);
EXPORT void _grConstantColorValue(int value);
EXPORT void _grAlphaBlendFunction(int a, int b, int c, int d);
EXPORT void _guColorCombineFunction(int func);
EXPORT void _guAlphaSource(int source);
EXPORT void _guDrawTriangleWithClip(GrVertex *a, GrVertex *b, GrVertex *c);

static FILE *out = NULL;
static void *lfb = NULL;
static int using_write_ptr = 0;
static int lfb_frame = 1;

__attribute__((constructor)) void init() {
	out = fopen("dump/grtrace", "wb");
	// Magic and version.
	fwrite("grTR\0\0\0\0", 1, 8, out);
	lfb = malloc(1024 * 1024 * 2);
}

static const int ASPECT[] = {
	8,
	4,
	2,
	1,
	2,
	4,
	8,
};

static const int BPP[] = {
	1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
};

static const int LOD_SIZE[] = {
	256,
	128,
	64,
	32,
	16,
	8,
	4,
	2,
	1,
};

static int get_size(struct GrTexInfo *info) {
	int size = LOD_SIZE[info->large];
	int bpp = BPP[info->format];
	int aspect = ASPECT[info->aspect];
	return size * size * bpp / aspect;
}

static void dump(int func, int num_args, ...) {
	va_list ap;
	fwrite(&func, 1, 4, out);
	va_start(ap, num_args);
	for (int i = 0; i < num_args; ++i) {
		int arg = va_arg(ap, int);
		fwrite(&arg, 1, 4, out);
	}
	va_end(ap);
}

static void dump_tex(struct GrTexInfo *info) {
	int size = get_size(info);
	fwrite("gTEX", 1, 4, out);
	fwrite(info, sizeof(int), 4, out);
	fwrite(info->data, 1, size, out);
}

static void dump_lfb() {
	assert(using_write_ptr);
	fwrite("gLFB", 1, 4, out);
	fwrite(lfb, 1, 1024 * 1024 * 2, out);
	++lfb_frame;
	using_write_ptr = 0;
}

static void dump_vertex(GrVertex *vtx) {
	fwrite("gVTX", 1, 4, out);
	fwrite(vtx, 1, sizeof(GrVertex), out);
}

EXPORT void _grGlideInit(void) {
	debug("grGlideInit()\n");
	dump(0, 0);
}
EXPORT void _grGlideShutdown(void) {
	debug("grGlideShutdown()\n");
	dump(1, 0);
}
EXPORT int _grSstQueryHardware(void *ptr) {
	debug("grSstQueryHardware(%p)\n", ptr);
	dump(2, 1, ptr);
	return 1;
}
EXPORT void _grSstSelect(int which) {
	debug("grSstSelect(%d)\n", which);
	dump(3, 1, which);
}
EXPORT int _grSstOpen(int res, int ref, int format, int origin, int smooth, int num_buffers) {
	debug("grSstOpen(%d, %d, %d, %d, %d, %d)\n", res, ref, format, origin, smooth, num_buffers);
	dump(4, 6, res, ref, format, origin, smooth, num_buffers);
	return 1;
}
EXPORT void _grSstPassthruMode(int mode) {
	debug("grSstPassthruMode(%d)\n", mode);
	dump(5, 1, mode);
}
EXPORT void _grBufferClear(int color, int alpha, int depth) {
	debug("grBufferClear(%d, %d, %d)\n", color, alpha, depth);
	dump(6, 3, color, alpha, depth);
}
EXPORT void _grBufferSwap(int interval) {
	debug("grBufferSwap(%d)", interval);
	dump(7, 1, interval);
	if (using_write_ptr) {
		dump_lfb();
		debug("-> %d", lfb_frame);
	}
	debug("\n");
}
EXPORT void _grLfbBegin(void) {
	debug("grLfbBegin()\n");
	wchar_t magenta =
#if SIZEOF_WCHAR_T == 2
		0xf81f;
#elif SIZEOF_WCHAR_T == 4
		0xf81ff81f;
#else
#error "SIZEOF_WCHAR_T must be 2 or 4."
#endif
	wmemset(lfb, magenta, 1024 * 1024 * 2 / SIZEOF_WCHAR_T);
	dump(8, 0);
}
EXPORT void _grLfbEnd(void) {
	debug("grLfbEnd() -> %d\n", lfb_frame);
	dump(9, 0);
	dump_lfb();
}
EXPORT void _grLfbBypassMode(int mode) {
	debug("grLfbBypassMode(%d)\n", mode);
	dump(10, 1, mode);
}
EXPORT void _grLfbWriteMode(int mode) {
	debug("grLfbWriteMode(%d)\n", mode);
	dump(11, 1, mode);
}
EXPORT void *_grLfbGetWritePtr(int buffer) {
	debug("grLfbGetWritePtr(%d)\n", buffer);
	using_write_ptr = 1;
	dump(12, 1, buffer);
	return lfb;
}
EXPORT int _grTexMinAddress(int mode) {
	// Simulate a 4 MiB card.
	int ret = 0x100000;
	debug("grTexMinAddress(%d) -> 0x%08x\n", mode, ret);
	dump(13, 1, mode);
	return ret;
}
EXPORT int _grTexMaxAddress(int mode) {
	// Simulate a 4 MiB card.
	int ret = 0x500000;
	debug("grTexMaxAddress(%d) -> 0x%08x\n", mode, ret);
	dump(14, 1, mode);
	return ret;
}
EXPORT int _grTexTextureMemRequired(int tmu, struct GrTexInfo *info) {
	int size = get_size(info);
	debug("grTexTextureMemRequired(%d, %p) -> %d\n", tmu, info->data, size);
	dump(15, 5, tmu, info->small, info->large, info->aspect, info->format);
	return size;
}
EXPORT void _grTexDownloadMipMap(int tmu, int startAddress, int evenOdd, struct GrTexInfo *info) {
	debug("grTexDownloadMipMap(%d, %d, %d, %p)\n", tmu, startAddress, evenOdd, (void*)info);
	dump(16, 3, tmu, startAddress, evenOdd);
	dump_tex(info);
}
EXPORT void _grTexSource(int tmu, int startAddress, int evenOdd, struct GrTexInfo *info) {
	debug("grTexSource(%d, 0x%08x, %d, %p)\n", tmu, startAddress, evenOdd, (void*)info);
	dump(17, 7, tmu, startAddress, evenOdd, info->small, info->large, info->aspect, info->format);
}
EXPORT void _grTexCombineFunction(int tmu, int func) {
	debug("grTexCombineFunction(%d, %d)\n", tmu, func);
	dump(18, 2, tmu, func);
}
EXPORT void _grDepthBufferMode(int mode) {
	debug("grDepthBufferMode(%d)\n", mode);
	dump(19, 1, mode);
}
EXPORT void _grCullMode(int mode) {
	debug("grCullMode(%d)\n", mode);
	dump(20, 1, mode);
}
EXPORT void _grErrorSetCallback(void *func) {
	debug("grErrorSetCallback(%p)\n", func);
	dump(21, 1, func);
}
EXPORT void _grClipWindow(int a, int b, int c, int d) {
	debug("grClipWindow(%d, %d, %d, %d)\n", a, b, c, d);
	dump(22, 4, a, b, c, d);
}
EXPORT void _grDrawTriangle(GrVertex *a, GrVertex *b, GrVertex *c) {
	debug("grDrawTriangle(%p, %p, %p)\n", (void*)a, (void*)b, (void*)c);
	dump(23, 0);
	dump_vertex(a);
	dump_vertex(b);
	dump_vertex(c);
}
EXPORT void _grChromakeyValue(int value) {
	debug("grChromakeyValue(%d)\n", value);
	dump(24, 1, value);
}
EXPORT void _grChromakeyMode(int mode) {
	debug("grChromakeyMode(%d)\n", mode);
	dump(25, 1, mode);
}
EXPORT void _grConstantColorValue(int value) {
	debug("grConstantColorValue(%d)\n", value);
	dump(26, 1, value);
}
EXPORT void _grAlphaBlendFunction(int a, int b, int c, int d) {
	debug("grAlphaBlendFunction(%d, %d, %d, %d)\n", a, b, c, d);
	dump(27, 4, a, b, c, d);
}
EXPORT void _guColorCombineFunction(int func) {
	debug("guColorCombineFunction(%d)\n", func);
	dump(28, 1, func);
}
EXPORT void _guAlphaSource(int source) {
	debug("guAlphaSource(%d)\n", source);
	dump(29, 1, source);
}
EXPORT void _guDrawTriangleWithClip(GrVertex *a, GrVertex *b, GrVertex *c) {
	debug("guDrawTriangleWithClip(%p, %p, %p)\n", (void*)a, (void*)b, (void*)c);
	dump(30, 0);
	dump_vertex(a);
	dump_vertex(b);
	dump_vertex(c);
}
