/*
 * Copyright © 2020 Mike Blumenkrantz
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 * 
 * Authors:
 *    Mike Blumenkrantz <michael.blumenkrantz@gmail.com>
 */

#include "nir.h"
#include "nir_builder.h"

/**
 * This pass splits gl_FragColor into gl_FragData[0-7] for drivers which handle
 * the former but not the latter, e.g., zink.
 *
 * This pass needs to handle both store_deref (pre-nir_lower_io) and
 * store_output (post-nir_lower_io), as hardware drivers need to call
 * nir_lower_fragcolor after I/O lowering but layered drivers don't lower I/O at
 * all.
 */

/*
 If a fragment shader writes to "gl_FragColor", DrawBuffersIndexedEXT
 specifies a set of draw buffers into which the color written to
 "gl_FragColor" is written. If a fragment shader writes to
 gl_FragData, DrawBuffersIndexedEXT specifies a set of draw buffers
 into which each of the multiple output colors defined by these
 variables are separately written. If a fragment shader writes to
 neither gl_FragColor nor gl_FragData, the values of the fragment
 colors following shader execution are undefined, and may differ
 for each fragment color.

 - EXT_multiview_draw_buffers

 */

static bool
lower_fragcolor_instr(nir_builder *b, nir_instr *intr, void *data)
{
   if (intr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *instr = nir_instr_as_intrinsic(intr);
   unsigned *max_draw_buffers = data;

   nir_variable *out = NULL;

   if (instr->intrinsic == nir_intrinsic_store_deref) {
      out = nir_deref_instr_get_variable(nir_src_as_deref(instr->src[0]));

      if (out->data.location != FRAG_RESULT_COLOR || out->data.mode != nir_var_shader_out)
         return false;

      ralloc_free(out->name);

      const char *name = out->data.index == 0 ? "gl_FragData[0]" :
         "gl_SecondaryFragDataEXT[0]";
      out->name = ralloc_strdup(out, name);

      /* translate gl_FragColor -> gl_FragData since this is already handled */
      out->data.location = FRAG_RESULT_DATA0;
   } else if (instr->intrinsic == nir_intrinsic_store_output) {
      nir_io_semantics sem = nir_intrinsic_io_semantics(instr);
      if (sem.location != FRAG_RESULT_COLOR)
         return false;
   } else {
      return false;
   }

   b->cursor = nir_after_instr(&instr->instr);

   unsigned idx = out ? 1 : 0;
   assert(instr->src[idx].is_ssa);
   nir_ssa_def *frag_color = instr->src[idx].ssa;
      nir_component_mask_t writemask = nir_intrinsic_write_mask(instr);
   b->shader->info.outputs_written &= ~BITFIELD64_BIT(FRAG_RESULT_COLOR);
   b->shader->info.outputs_written |= BITFIELD64_BIT(FRAG_RESULT_DATA0);

   for (unsigned i = (out ? 1 : 0); i < *max_draw_buffers; i++) {
      if (out == NULL) {
         nir_io_semantics semantics = {
            .location = FRAG_RESULT_DATA0 + i,
            .num_slots = 1
         };

         nir_store_output(b, frag_color, nir_ssa_for_src(b, instr->src[1], 1),
               .src_type = nir_intrinsic_src_type(instr),
               .write_mask = writemask,
               .io_semantics = semantics);
      } else {
         char name[28];
         const char *name_tmpl = out->data.index == 0 ? "gl_FragData[%u]" :
            "gl_SecondaryFragDataEXT[%u]";
         snprintf(name, sizeof(name), name_tmpl, i);
         nir_variable *out_color = nir_variable_create(b->shader, nir_var_shader_out,
               out->type, name);
         out_color->data.location = FRAG_RESULT_DATA0 + i;
         out_color->data.driver_location = b->shader->num_outputs++;
         out_color->data.index = out->data.index;
         nir_store_var(b, out_color, frag_color, writemask);
      }

      b->shader->info.outputs_written |= BITFIELD64_BIT(FRAG_RESULT_DATA0 + i);
   }

   if (out == NULL)
      nir_instr_remove(&instr->instr);

   return true;
}

bool
nir_lower_fragcolor(nir_shader *shader, unsigned max_draw_buffers)
{
   if (shader->info.stage != MESA_SHADER_FRAGMENT)
      return false;

   return nir_shader_instructions_pass(shader, lower_fragcolor_instr,
         nir_metadata_block_index | nir_metadata_dominance, &max_draw_buffers);
}
