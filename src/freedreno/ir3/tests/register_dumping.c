/*
 * Copyright © 2021 Igalia S.L.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <err.h>
#include <stdio.h>

#include "ir3.h"
#include "ir3_assembler.h"
#include "ir3_shader.h"

/*
 * Check that instrumentation for register dumping works.
 */

#define TEST(...) { # __VA_ARGS__ }

static const struct test {
	const char *asmstr;
} tests[] = {
	TEST(
		mov.f32f32 r0.x, c0.x
		mov.s32s32 r0.z, 1
		mov.s32s32 r<a0.x + 4>, r2.x
		mov.f16f16 hr0.x, hr0.x
		mova1 a1.x, h(0)
		add.s r0.x, r0.x, r0.z
		and.b p0.x, hr2.y, h(1)
		add.f hr0.z, r0.y, c<a0.x + 33>
	),
	TEST(
		isam.base0 (u32)(x)r0.x, r0.x, s#0, t#0
		isamm (f16)(xyz)hr0.x, r0.w, s#0, t#0
		sam.base0 (f32)(xyzw)r0.x, r0.z, s#1, a1.x
		atomic.s.add.untyped.1d.u32.1.g r1.y, g[1], r0.x, r0.w, r0.x
	),
	TEST(
		stp.u32 p[r0.z], r0.x, 2
		ldp.u32 r0.x, p[r0.z], 3
		ldg.u32 r1.x, g[r0.z+4], 2
	),
};

static struct ir3_shader *
parse_asm(struct ir3_compiler *c, const char *asmstr)
{
	struct ir3_kernel_info info = {};
	FILE *in = fmemopen((void *)asmstr, strlen(asmstr), "r");
	struct ir3_shader *shader = ir3_parse_asm(c, &info, in);

	fclose(in);

	if (!shader)
		errx(-1, "assembler failed");

	return shader;
}

static uint32_t tmp_buf[1024] = { 0 };

static struct ir3_instrumentation_iova
create_dummy_iova(void *ctx, uint64_t size)
{
   struct ir3_instrumentation_iova iova = {
      .private_data = NULL,
      .iova = 0xFFFF,
      .map = tmp_buf,
   };

   return iova;
}

static void
destroy_dummy_iova(void *ctx, struct ir3_instrumentation_iova *iova)
{
   // Nothing
}

int
main(int argc, char **argv)
{
	struct ir3_compiler *c;
	int result = 0;

	c = ir3_compiler_create(NULL, 630);

	char *stream_data = NULL;
	size_t stream_size = 0;
	FILE *stream = open_memstream(&stream_data, &stream_size);

	for (int i = 0; i < ARRAY_SIZE(tests); i++) {
		for (int dump_wave = 0; dump_wave <= 1; dump_wave++) {
			if (dump_wave) {
				setenv("IR3_SHADER_INSTRUMENT_WAVE", "3", 1);
			} else {
				unsetenv("IR3_SHADER_INSTRUMENT_WAVE");
			}

			const struct test *test = &tests[i];
			struct ir3_shader *shader = parse_asm(c, test->asmstr);
			struct ir3_shader_variant *v = &shader->variants[0];

			shader->iova_func.ctx = NULL;
			shader->iova_func.create_iova = &create_dummy_iova;
			shader->iova_func.destroy_iova = &destroy_dummy_iova;

			ir3_instrument_shader(v);
			v->bin = ir3_shader_assemble(v);

			ir3_shader_destroy(shader);

			ir3_dump_all_instrumentation_results(stream);
		}
	}

	fclose(stream);
	free(stream_data);

	ir3_compiler_destroy(c);

	return result;
}
