/*
 * Copyright © 2018 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "tgsi/tgsi_from_mesa.h"
#include "st_nir.h"
#include "st_program.h"

#include "compiler/nir/nir_builder.h"
#include "compiler/glsl/gl_nir.h"
#include "compiler/glsl/gl_nir_linker.h"
#include "tgsi/tgsi_parse.h"

void
st_nir_finish_builtin_nir(struct st_context *st, nir_shader *nir)
{
   struct pipe_screen *screen = st->screen;
   gl_shader_stage stage = nir->info.stage;

   nir->info.separate_shader = true;
   if (stage == MESA_SHADER_FRAGMENT)
      nir->info.fs.untyped_color_outputs = true;

   NIR_PASS_V(nir, nir_lower_global_vars_to_local);
   NIR_PASS_V(nir, nir_split_var_copies);
   NIR_PASS_V(nir, nir_lower_var_copies);
   NIR_PASS_V(nir, nir_lower_system_values);
   NIR_PASS_V(nir, nir_lower_compute_system_values, NULL);

   if (nir->options->lower_to_scalar) {
      nir_variable_mode mask =
          (stage > MESA_SHADER_VERTEX ? nir_var_shader_in : 0) |
          (stage < MESA_SHADER_FRAGMENT ? nir_var_shader_out : 0);

      NIR_PASS_V(nir, nir_lower_io_to_scalar_early, mask);
   }

   if (st->lower_rect_tex) {
      const struct nir_lower_tex_options opts = { .lower_rect = true, };
      NIR_PASS_V(nir, nir_lower_tex, &opts);
   }

   nir_shader_gather_info(nir, nir_shader_get_entrypoint(nir));

   st_nir_assign_vs_in_locations(nir);
   st_nir_assign_varying_locations(st, nir);

   st_nir_lower_samplers(screen, nir, NULL, NULL);
   st_nir_lower_uniforms(st, nir);
   if (!screen->get_param(screen, PIPE_CAP_NIR_IMAGES_AS_DEREF))
      NIR_PASS_V(nir, gl_nir_lower_images, false);

   if (screen->finalize_nir) {
      char *msg = screen->finalize_nir(screen, nir);
      free(msg);
   } else {
      gl_nir_opts(nir);
   }
}

struct pipe_shader_state *
st_nir_finish_builtin_shader(struct st_context *st,
                             nir_shader *nir)
{
   st_nir_finish_builtin_nir(st, nir);

   struct pipe_shader_state state = {
      .type = PIPE_SHADER_IR_NIR,
      .ir.nir = nir,
   };

   return st_create_nir_shader(st, &state);
}

/**
 * Make a simple shader that copies inputs to corresponding outputs.
 */
struct pipe_shader_state *
st_nir_make_passthrough_shader(struct st_context *st,
                               const char *shader_name,
                               gl_shader_stage stage,
                               unsigned num_vars,
                               unsigned *input_locations,
                               unsigned *output_locations,
                               unsigned *interpolation_modes,
                               unsigned sysval_mask)
{
   const struct glsl_type *vec4 = glsl_vec4_type();
   const nir_shader_compiler_options *options =
      st_get_nir_compiler_options(st, stage);

   nir_builder b = nir_builder_init_simple_shader(stage, options,
                                                  "%s", shader_name);

   char var_name[15];

   for (unsigned i = 0; i < num_vars; i++) {
      nir_variable *in;
      if (sysval_mask & (1 << i)) {
         snprintf(var_name, sizeof(var_name), "sys_%u", input_locations[i]);
         in = nir_variable_create(b.shader, nir_var_system_value,
                                  glsl_int_type(), var_name);
      } else {
         snprintf(var_name, sizeof(var_name), "in_%u", input_locations[i]);
         in = nir_variable_create(b.shader, nir_var_shader_in, vec4, var_name);
      }
      in->data.location = input_locations[i];
      if (interpolation_modes)
         in->data.interpolation = interpolation_modes[i];

      snprintf(var_name, sizeof(var_name), "out_%u", output_locations[i]);
      nir_variable *out =
         nir_variable_create(b.shader, nir_var_shader_out, in->type, var_name);
      out->data.location = output_locations[i];
      out->data.interpolation = in->data.interpolation;

      nir_copy_var(&b, out, in);
   }

   return st_nir_finish_builtin_shader(st, b.shader);
}

/**
 * Make a simple shader that reads color value from a constant buffer
 * and uses it to clear all color buffers.
 */
struct pipe_shader_state *
st_nir_make_clearcolor_shader(struct st_context *st)
{
   const nir_shader_compiler_options *options =
      st_get_nir_compiler_options(st, MESA_SHADER_FRAGMENT);

   nir_builder b = nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT, options,
                                                  "clear color FS");
   b.shader->info.num_ubos = 1;
   b.shader->num_outputs = 1;
   b.shader->num_uniforms = 1;

   /* Read clear color from constant buffer */
   nir_ssa_def *clear_color = nir_load_uniform(&b, 4, 32, nir_imm_int(&b,0),
                                               .range = 16,
                                               .dest_type = nir_type_float32);

   nir_variable *color_out =
      nir_variable_create(b.shader, nir_var_shader_out, glsl_vec_type(4),
                             "outcolor");
   color_out->data.location = FRAG_RESULT_COLOR;

   /* Write out the color */
   nir_store_var(&b, color_out, clear_color, 0xf);

   return st_nir_finish_builtin_shader(st, b.shader);
}

/**
 * Make a pass-thru GS which passes:
 *
 *    gl_Position = in_glPosition[i];
 *    out_color   = in_color[i];
 *    gl_Layer    = in_gl_Layer[i];
 *
 */
struct pipe_shader_state *
st_nir_make_nir_layered_clear_gs_shader(struct st_context *st)
{
   const nir_shader_compiler_options *options =
      st_get_nir_compiler_options(st, MESA_SHADER_GEOMETRY);
   nir_builder b = nir_builder_init_simple_shader(MESA_SHADER_GEOMETRY, options,
                                                  "layered clear GS");
   nir_shader *nir = b.shader;

   nir->info.inputs_read = 1ull << VARYING_SLOT_POS;
   nir->info.outputs_written = (1ull << VARYING_SLOT_POS) |
                               (1ull << VARYING_SLOT_LAYER);
   nir->info.gs.input_primitive = SHADER_PRIM_TRIANGLES;
   nir->info.gs.output_primitive = SHADER_PRIM_TRIANGLE_STRIP;
   nir->info.gs.vertices_in = 3;
   nir->info.gs.vertices_out = 3;
   nir->info.gs.invocations = 1;
   nir->info.gs.active_stream_mask = 0x1;

   /* in vec4 in_gl_Position[3] */
   nir_variable *gs_in_pos =
      nir_variable_create(b.shader, nir_var_shader_in,
                          glsl_array_type(glsl_vec4_type(), 3, 0),
                          "in_gl_Position");
   gs_in_pos->data.location = VARYING_SLOT_POS;

   /* in vec4 in_color[3] */
   nir_variable *gs_in_col =
      nir_variable_create(b.shader, nir_var_shader_in,
                          glsl_array_type(glsl_vec4_type(), 3, 0),
                          "in_color");
   gs_in_col->data.location = VARYING_SLOT_COL0;

   /* in int in_gl_Layer[3] */
   nir_variable *gs_in_layer =
      nir_variable_create(b.shader, nir_var_shader_in,
                          glsl_array_type(glsl_int_type(), 3, 0),
                          "in_gl_Layer");
   gs_in_layer->data.location = VARYING_SLOT_LAYER;

   /* out vec4 gl_Position */
   nir_variable *gs_out_pos =
      nir_variable_create(b.shader, nir_var_shader_out, glsl_vec4_type(),
                          "gl_Position");
   gs_out_pos->data.location = VARYING_SLOT_POS;

   /* out vec4 out_color */
   nir_variable *gs_out_col =
      nir_variable_create(b.shader, nir_var_shader_out, glsl_vec4_type(),
                          "out_color");
   gs_out_col->data.location = VARYING_SLOT_COL0;

   /* out float gl_Layer */
   nir_variable *gs_out_layer =
      nir_variable_create(b.shader, nir_var_shader_out, glsl_int_type(),
                          "out_gl_Layer");
   gs_out_layer->data.location = VARYING_SLOT_LAYER;

   /* Emit output triangle */
   for (uint32_t i = 0; i < 3; i++) {

      /* gl_Position = in_gl_Position[i] */
      nir_deref_instr *in_pos_i =
         nir_build_deref_array_imm(&b, nir_build_deref_var(&b, gs_in_pos), i);
      nir_copy_deref(&b, nir_build_deref_var(&b, gs_out_pos), in_pos_i);

      /* out_color = in_color[i] */
      nir_deref_instr *in_col_i =
         nir_build_deref_array_imm(&b, nir_build_deref_var(&b, gs_in_col), i);
      nir_copy_deref(&b, nir_build_deref_var(&b, gs_out_col), in_col_i);

      /* gl_Layer = in_gl_Layer[i] */
      nir_deref_instr *in_layer_i =
         nir_build_deref_array_imm(&b, nir_build_deref_var(&b, gs_in_layer), i);
      nir_copy_deref(&b, nir_build_deref_var(&b, gs_out_layer), in_layer_i);

      nir_emit_vertex(&b, 0);
   }

   nir_end_primitive(&b, 0);

   return st_nir_finish_builtin_shader(st, nir);
}
