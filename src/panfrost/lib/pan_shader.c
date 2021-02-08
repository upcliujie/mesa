/*
 * Copyright (C) 2021 Collabora, Ltd.
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

#include "pan_shader.h"
#include "panfrost/midgard/midgard_compile.h"
#include "panfrost/bifrost/bifrost_compile.h"

const nir_shader_compiler_options *
pan_shader_get_compiler_options(const struct panfrost_device *dev)
{
        if (pan_is_bifrost(dev))
                return &bifrost_nir_options;

        return &midgard_nir_options;
}

static enum pipe_format
varying_format(nir_alu_type t, unsigned ncomps)
{
#define VARYING_FORMAT(ntype, nsz, ptype, psz) \
        { \
                .type = nir_type_ ## ntype ## nsz, \
                .formats = { \
                        PIPE_FORMAT_R ## psz ## _ ## ptype, \
                        PIPE_FORMAT_R ## psz ## G ## psz ## _ ## ptype, \
                        PIPE_FORMAT_R ## psz ## G ## psz ## B ## psz ## _ ## ptype, \
                        PIPE_FORMAT_R ## psz ## G ## psz ## B ## psz  ## A ## psz ## _ ## ptype, \
                } \
        }

        static const struct {
                nir_alu_type type;
                enum pipe_format formats[4];
        } conv[] = {
                VARYING_FORMAT(bool, 1, UINT, 8),
                VARYING_FORMAT(bool, 8, UINT, 8),
                VARYING_FORMAT(bool, 16, UINT, 16),
                VARYING_FORMAT(bool, 32, UINT, 32),
                VARYING_FORMAT(int, 8, SINT, 8),
                VARYING_FORMAT(int, 16, SINT, 16),
                VARYING_FORMAT(int, 32, SINT, 32),
                VARYING_FORMAT(uint, 8, UINT, 8),
                VARYING_FORMAT(uint, 16, UINT, 16),
                VARYING_FORMAT(uint, 32, UINT, 32),
                VARYING_FORMAT(float, 16, FLOAT, 16),
                VARYING_FORMAT(float, 32, FLOAT, 32),
        };
#undef VARYING_FORMAT

        assert(ncomps > 0 && ncomps <= ARRAY_SIZE(conv[0].formats));

        for (unsigned i = 0; i < ARRAY_SIZE(conv); i++) {
                if (conv[i].type == t)
                        return conv[i].formats[ncomps - 1];
        }

        return PIPE_FORMAT_NONE;
}

static void
collect_varyings(nir_shader *s, nir_variable_mode varying_mode,
                 struct pan_shader_varying *varyings,
                 unsigned *varying_count)
{
        *varying_count = 0;

        nir_foreach_variable_with_modes(var, s, varying_mode) {
                unsigned loc = var->data.driver_location;
                unsigned sz = glsl_count_attribute_slots(var->type, FALSE);
                const struct glsl_type *column =
                        glsl_without_array_or_matrix(var->type);
                unsigned chan = glsl_get_components(column);
                enum glsl_base_type base_type = glsl_get_base_type(column);

                /* If we have a fractional location added, we need to increase the size
                 * so it will fit, i.e. a vec3 in YZW requires us to allocate a vec4.
                 * We could do better but this is an edge case as it is, normally
                 * packed varyings will be aligned.
                 */
                chan += var->data.location_frac;
                assert(chan >= 1 && chan <= 4);

                nir_alu_type type = nir_get_nir_type_for_glsl_base_type(base_type);

                type = nir_alu_type_get_base_type(type);

                /* Demote to fp16 where possible. int16 varyings are TODO as the hw
                 * will saturate instead of wrap which is not conformant, so we need to
                 * insert i2i16/u2u16 instructions before the st_vary_32i/32u to get
                 * the intended behaviour.
                 */
                if (type == nir_type_float &&
                    (var->data.precision == GLSL_PRECISION_MEDIUM ||
                     var->data.precision == GLSL_PRECISION_LOW)) {
                        type |= 16;
                } else {
                        type |= 32;
                }

                enum pipe_format format = varying_format(type, chan);
                assert(format != PIPE_FORMAT_NONE);

                for (int c = 0; c < sz; ++c) {
                        varyings[loc + c].location = var->data.location + c;
                        varyings[loc + c].format = format;
                }

                *varying_count = MAX2(*varying_count, loc + sz);
        }
}

void
pan_shader_compile(const struct panfrost_device *dev,
                   nir_shader *s,
                   const struct panfrost_compile_inputs *inputs,
                   struct util_dynarray *binary,
                   struct pan_shader_info *info)
{
        memset(info, 0, sizeof(*info));

        if (pan_is_bifrost(dev))
                bifrost_compile_shader_nir(s, inputs, binary, info);
        else
                midgard_compile_shader_nir(s, inputs, binary, info);

        info->stage = s->info.stage;
        switch (info->stage) {
        case MESA_SHADER_VERTEX:
                info->attribute_count = util_bitcount64(s->info.inputs_read);

                bool vertex_id = BITSET_TEST(s->info.system_values_read,
                                             SYSTEM_VALUE_VERTEX_ID);
                if (vertex_id)
                        info->attribute_count = MAX2(info->attribute_count, PAN_VERTEX_ID + 1);

                bool instance_id = BITSET_TEST(s->info.system_values_read,
                                               SYSTEM_VALUE_INSTANCE_ID);
                if (instance_id)
                        info->attribute_count = MAX2(info->attribute_count, PAN_INSTANCE_ID + 1);

                info->vs.writes_point_size =
                        s->info.outputs_written & (1 << VARYING_SLOT_PSIZ);
                collect_varyings(s, nir_var_shader_out, info->varyings.output,
                                 &info->varyings.output_count);
                break;
        case MESA_SHADER_FRAGMENT:
                if (s->info.outputs_written & BITFIELD64_BIT(FRAG_RESULT_DEPTH))
                        info->fs.writes_depth = true;
                if (s->info.outputs_written & BITFIELD64_BIT(FRAG_RESULT_STENCIL))
                        info->fs.writes_stencil = true;

                uint64_t outputs_read = s->info.outputs_read;
                if (outputs_read & BITFIELD64_BIT(FRAG_RESULT_COLOR))
                        outputs_read |= BITFIELD64_BIT(FRAG_RESULT_DATA0);

                info->fs.outputs_read = outputs_read >> FRAG_RESULT_DATA0;
                info->fs.can_discard = s->info.fs.uses_discard;
                info->fs.helper_invocations = s->info.fs.needs_quad_helper_invocations;

                /* List of reasons we need to execute frag shaders when things
                 * are masked off */

                info->fs.sidefx = s->info.writes_memory ||
                                  s->info.fs.uses_discard ||
                                  s->info.fs.uses_demote;
                info->fs.reads_frag_coord =
                        (s->info.inputs_read & (1 << VARYING_SLOT_POS)) ||
                        BITSET_TEST(s->info.system_values_read, SYSTEM_VALUE_FRAG_COORD);
                info->fs.reads_point_coord =
                        s->info.inputs_read & (1 << VARYING_SLOT_PNTC);
                info->fs.reads_face =
                        (s->info.inputs_read & (1 << VARYING_SLOT_FACE)) ||
                        BITSET_TEST(s->info.system_values_read, SYSTEM_VALUE_FRONT_FACE);
                collect_varyings(s, nir_var_shader_in, info->varyings.input,
                                 &info->varyings.input_count);
                break;
        case MESA_SHADER_COMPUTE:
                info->wls_size = s->info.cs.shared_size;
                break;
        default:
                unreachable("Unknown shader state");
        }

        info->attribute_count += util_bitcount(s->info.images_used);
        info->writes_global = s->info.writes_memory;
        info->outputs_written = s->info.outputs_written;

        /* Separate as primary uniform count is truncated. Sysvals are prefix
         * uniforms */
        if (!pan_is_bifrost(dev)) {
                info->uniform_count =
                        MIN2(s->num_uniforms + info->sysval_count,
                             info->midgard.uniform_cutoff);
        }

        /* off-by-one for uniforms. Not needed on Bifrost since uniforms
         * have been lowered to UBOs using nir_lower_uniforms_to_ubo() which
         * already increments s->info.num_ubos. We do have to account for the
         * "no uniform, no UBO" case though, otherwise sysval passed through
         * uniforms won't work correctly.
         */
        if (pan_is_bifrost(dev))
                info->ubo_count = MAX2(s->info.num_ubos, 1);
        else
                info->ubo_count = s->info.num_ubos + 1;

        info->texture_count = s->info.num_textures;
}

static void
midgard_prepare_rsd(const struct pan_shader_info *info,
                    struct MALI_RENDERER_STATE *rsd)
{
        rsd->properties.uniform_buffer_count = info->ubo_count;
        rsd->properties.midgard.uniform_count = info->uniform_count;
        rsd->properties.midgard.shader_has_side_effects = info->writes_global;

        /* TODO: Select the appropriate mode. Suppresing inf/nan works around
         * some bugs in gles2 apps (eg glmark2's terrain scene) but isn't
         * conformant on gles3 */
        rsd->properties.midgard.fp_mode = MALI_FP_MODE_GL_INF_NAN_SUPPRESSED;

        /* For fragment shaders, work register count, early-z, reads at draw-time */

        if (info->stage != MESA_SHADER_FRAGMENT)
                rsd->properties.midgard.work_register_count = info->work_reg_count;
}

static void
bifrost_prepare_rsd(const struct pan_shader_info *info,
                    struct MALI_RENDERER_STATE *rsd)
{
        switch (info->stage) {
        case MESA_SHADER_VERTEX:
                rsd->properties.bifrost.zs_update_operation = MALI_PIXEL_KILL_STRONG_EARLY;
                rsd->properties.uniform_buffer_count = info->ubo_count;

                rsd->preload.uniform_count = info->uniform_count;
                rsd->preload.vertex.vertex_id = true;
                rsd->preload.vertex.instance_id = true;
                break;

        case MESA_SHADER_FRAGMENT:
                /* Early-Z set at draw-time */
                if (info->fs.writes_depth || info->fs.writes_stencil) {
                        rsd->properties.bifrost.zs_update_operation = MALI_PIXEL_KILL_FORCE_LATE;
                        rsd->properties.bifrost.pixel_kill_operation = MALI_PIXEL_KILL_FORCE_LATE;
                } else if (info->fs.can_discard) {
                        rsd->properties.bifrost.zs_update_operation = MALI_PIXEL_KILL_FORCE_LATE;
                        rsd->properties.bifrost.pixel_kill_operation = MALI_PIXEL_KILL_WEAK_EARLY;
                } else {
                        rsd->properties.bifrost.zs_update_operation = MALI_PIXEL_KILL_STRONG_EARLY;
                        rsd->properties.bifrost.pixel_kill_operation = MALI_PIXEL_KILL_FORCE_EARLY;
                }
                rsd->properties.uniform_buffer_count = info->ubo_count;
                rsd->properties.bifrost.shader_modifies_coverage = info->fs.can_discard;
                rsd->properties.bifrost.shader_wait_dependency_6 = info->bifrost.wait_6;
                rsd->properties.bifrost.shader_wait_dependency_7 = info->bifrost.wait_7;

                rsd->preload.uniform_count = info->uniform_count;
                rsd->preload.fragment.fragment_position = info->fs.reads_frag_coord;
                rsd->preload.fragment.coverage = true;
                rsd->preload.fragment.primitive_flags = info->fs.reads_face;
                break;

        case MESA_SHADER_COMPUTE:
                rsd->properties.uniform_buffer_count = info->ubo_count;

                rsd->preload.uniform_count = info->uniform_count;
                rsd->preload.compute.local_invocation_xy = true;
                rsd->preload.compute.local_invocation_z = true;
                rsd->preload.compute.work_group_x = true;
                rsd->preload.compute.work_group_y = true;
                rsd->preload.compute.work_group_z = true;
                rsd->preload.compute.global_invocation_x = true;
                rsd->preload.compute.global_invocation_y = true;
                rsd->preload.compute.global_invocation_z = true;
                break;

        default:
                unreachable("TODO");
        }
}

void
pan_shader_prepare_rsd(const struct panfrost_device *dev,
                       const struct pan_shader_info *shader_info,
                       mali_ptr shader_ptr,
                       struct MALI_RENDERER_STATE *rsd)
{
        if (!pan_is_bifrost(dev))
                shader_ptr |= shader_info->midgard.first_tag;

        rsd->shader.shader = shader_ptr;
        rsd->shader.attribute_count = shader_info->attribute_count;
        rsd->shader.varying_count = shader_info->varyings.input_count +
                                   shader_info->varyings.output_count;
        rsd->shader.texture_count = shader_info->texture_count;
        rsd->shader.sampler_count = shader_info->texture_count;

        if (shader_info->stage == MESA_SHADER_FRAGMENT) {
                rsd->properties.stencil_from_shader =
                        shader_info->fs.writes_stencil;
                rsd->properties.shader_contains_barrier =
                        shader_info->fs.helper_invocations;
                rsd->properties.depth_source =
                        shader_info->fs.writes_depth ?
                        MALI_DEPTH_SOURCE_SHADER :
                        MALI_DEPTH_SOURCE_FIXED_FUNCTION;
        } else {
                rsd->properties.depth_source = MALI_DEPTH_SOURCE_FIXED_FUNCTION;
        }

        if (pan_is_bifrost(dev))
                bifrost_prepare_rsd(shader_info, rsd);
        else
                midgard_prepare_rsd(shader_info, rsd);
}
