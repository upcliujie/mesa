/*
 * Copyright 2014, 2015 Red Hat.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* the virgl hw tgsi vs what the current gallium want will diverge over time.
   so add a transform stage to remove things we don't want to send unless
   the receiver supports it.
*/

#include "tgsi/tgsi_transform.h"
#include "tgsi/tgsi_info.h"
#include "virgl_context.h"
#include "virgl_screen.h"

struct virgl_transform_context {
   struct tgsi_transform_context base;
   bool cull_enabled;
   bool has_precise;
   bool fake_fp64;

   unsigned next_temp;

   unsigned clipdist0_out;
   unsigned clipdist1_out;
   unsigned clipdist_out_temp;

   unsigned layer_in;
   unsigned layer_in_temp;
   unsigned viewport_in;
   unsigned viewport_in_temp;
};

static void
virgl_tgsi_transform_declaration(struct tgsi_transform_context *ctx,
                                 struct tgsi_full_declaration *decl)
{
   struct virgl_transform_context *vtctx = (struct virgl_transform_context *)ctx;

   switch (decl->Declaration.File) {
   case TGSI_FILE_CONSTANT:
      if (decl->Declaration.Dimension) {
         if (decl->Dim.Index2D == 0)
            decl->Declaration.Dimension = 0;
      }
      break;
   case TGSI_FILE_INPUT:
      if (decl->Semantic.Name == TGSI_SEMANTIC_LAYER)
         vtctx->layer_in = decl->Range.First;
      if (decl->Semantic.Name == TGSI_SEMANTIC_VIEWPORT_INDEX)
         vtctx->viewport_in = decl->Range.First;
      break;
   case TGSI_FILE_OUTPUT:
      if (decl->Semantic.Name == TGSI_SEMANTIC_CLIPDIST) {
         if (decl->Semantic.Index == 0) {
            vtctx->clipdist0_out = decl->Range.First;
            if (decl->Range.Last != decl->Range.First)
               vtctx->clipdist1_out = decl->Range.Last;
         } else {
            vtctx->clipdist1_out = decl->Range.First;
         }
      }
      break;
   case TGSI_FILE_TEMPORARY:
      vtctx->next_temp = MAX2(vtctx->next_temp, decl->Range.Last + 1);
      break;
   default:
      break;
   }
   ctx->emit_declaration(ctx, decl);

}

/* for now just strip out the new properties the remote doesn't understand
   yet */
static void
virgl_tgsi_transform_property(struct tgsi_transform_context *ctx,
                              struct tgsi_full_property *prop)
{
   struct virgl_transform_context *vtctx = (struct virgl_transform_context *)ctx;
   switch (prop->Property.PropertyName) {
   case TGSI_PROPERTY_NUM_CLIPDIST_ENABLED:
   case TGSI_PROPERTY_NUM_CULLDIST_ENABLED:
      if (vtctx->cull_enabled)
	 ctx->emit_property(ctx, prop);
      break;
   case TGSI_PROPERTY_NEXT_SHADER:
      break;
   default:
      ctx->emit_property(ctx, prop);
      break;
   }
}

static void
virgl_tgsi_transform_prolog(struct tgsi_transform_context * ctx)
{
   struct virgl_transform_context *vtctx = (struct virgl_transform_context *)ctx;

   if (vtctx->clipdist0_out != ~0 || vtctx->clipdist1_out != ~0) {
      vtctx->clipdist_out_temp = vtctx->next_temp += 2;
      tgsi_transform_temps_decl(ctx, vtctx->clipdist_out_temp, vtctx->clipdist_out_temp + 1);
   }

   if (vtctx->layer_in != ~0) {
      vtctx->layer_in_temp = vtctx->next_temp++;
      tgsi_transform_temp_decl(ctx, vtctx->layer_in_temp);
   }

   if (vtctx->viewport_in != ~0) {
      vtctx->viewport_in_temp = vtctx->next_temp++;
      tgsi_transform_temp_decl(ctx, vtctx->viewport_in_temp);
   }

   /* virglrenderer makes mistakes in the types of layer/viewport input
    * references from unsigned ops, so we use a temp that we do a no-op unsigned
    * op to at the top of the shader.
    *
    * https://gitlab.freedesktop.org/virgl/virglrenderer/-/merge_requests/615
    */
   if (vtctx->layer_in != ~0) {
         tgsi_transform_op2_inst(ctx, TGSI_OPCODE_IMAX,
                                 TGSI_FILE_TEMPORARY, vtctx->layer_in_temp, TGSI_WRITEMASK_XYZW,
                                 TGSI_FILE_INPUT, vtctx->layer_in,
                                 TGSI_FILE_INPUT, vtctx->layer_in, 0);
   }

   if (vtctx->viewport_in != ~0) {
         tgsi_transform_op2_inst(ctx, TGSI_OPCODE_IMAX,
                                 TGSI_FILE_TEMPORARY, vtctx->viewport_in_temp, TGSI_WRITEMASK_XYZW,
                                 TGSI_FILE_INPUT, vtctx->viewport_in,
                                 TGSI_FILE_INPUT, vtctx->viewport_in, 0);
   }
}

static void
virgl_tgsi_transform_instruction(struct tgsi_transform_context *ctx,
				 struct tgsi_full_instruction *inst)
{
   struct virgl_transform_context *vtctx = (struct virgl_transform_context *)ctx;
   if (vtctx->fake_fp64 &&
       (tgsi_opcode_infer_src_type(inst->Instruction.Opcode, 0) == TGSI_TYPE_DOUBLE ||
        tgsi_opcode_infer_dst_type(inst->Instruction.Opcode, 0) == TGSI_TYPE_DOUBLE)) {
      debug_printf("VIRGL: ARB_gpu_shader_fp64 is exposed but not supported.");
      return;
   }

   if (!vtctx->has_precise && inst->Instruction.Precise)
      inst->Instruction.Precise = 0;

   for (unsigned i = 0; i < inst->Instruction.NumDstRegs; i++) {
      /* virglrenderer would fail to compile on clipdist writes without a full
       * writemask.  So, we write our clipdist writes to a temp and store that
       * temp with a full writemask.
       *
       * https://gitlab.freedesktop.org/virgl/virglrenderer/-/merge_requests/616
       */
      if (inst->Dst[i].Register.File == TGSI_FILE_OUTPUT &&
         (inst->Dst[i].Register.Index == vtctx->clipdist0_out ||
          inst->Dst[i].Register.Index == vtctx->clipdist1_out)) {
         int index = inst->Dst[i].Register.Index == vtctx->clipdist1_out;

         inst->Dst[i].Register.File = TGSI_FILE_TEMPORARY;
         inst->Dst[i].Register.Index = vtctx->clipdist_out_temp + index;
      }
   }

   for (unsigned i = 0; i < inst->Instruction.NumSrcRegs; i++) {
      if (inst->Src[i].Register.File == TGSI_FILE_CONSTANT &&
          inst->Src[i].Register.Dimension &&
          inst->Src[i].Dimension.Index == 0)
         inst->Src[i].Register.Dimension = 0;

      if (inst->Src[i].Register.File == TGSI_FILE_INPUT) {
         if (inst->Src[i].Register.Index == vtctx->layer_in) {
            inst->Src[i].Register.File = TGSI_FILE_TEMPORARY;
            inst->Src[i].Register.Index = vtctx->layer_in_temp;
         }
         if (inst->Src[i].Register.Index == vtctx->viewport_in) {
            inst->Src[i].Register.File = TGSI_FILE_TEMPORARY;
            inst->Src[i].Register.Index = vtctx->viewport_in_temp;
         }
      }
   }
   ctx->emit_instruction(ctx, inst);

   for (unsigned i = 0; i < inst->Instruction.NumDstRegs; i++) {
      /* Emit the fixup MOV from the clipdist temporary to the real output. */
      if (inst->Dst[i].Register.File == TGSI_FILE_TEMPORARY &&
         inst->Dst[i].Register.Index >= vtctx->clipdist_out_temp &&
         inst->Dst[i].Register.Index <= vtctx->clipdist_out_temp + 1) {
         tgsi_transform_op1_inst(ctx, TGSI_OPCODE_MOV,
                                 TGSI_FILE_OUTPUT,
                                 (inst->Dst[i].Register.Index == vtctx->clipdist_out_temp ?
                                  vtctx->clipdist0_out :
                                  vtctx->clipdist1_out),
                                 TGSI_WRITEMASK_XYZW,
                                 inst->Dst[i].Register.File,
                                 inst->Dst[i].Register.Index);
      }
   }
}

struct tgsi_token *virgl_tgsi_transform(struct virgl_screen *vscreen, const struct tgsi_token *tokens_in)
{
   struct virgl_transform_context transform;
   const uint newLen = tgsi_num_tokens(tokens_in) * 2 /* XXX: how many to allocate? */;
   struct tgsi_token *new_tokens;

   new_tokens = tgsi_alloc_tokens(newLen);
   if (!new_tokens)
      return NULL;

   memset(&transform, 0, sizeof(transform));
   transform.base.transform_declaration = virgl_tgsi_transform_declaration;
   transform.base.transform_property = virgl_tgsi_transform_property;
   transform.base.transform_instruction = virgl_tgsi_transform_instruction;
   transform.base.prolog = virgl_tgsi_transform_prolog;
   transform.cull_enabled = vscreen->caps.caps.v1.bset.has_cull;
   transform.has_precise = vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_TGSI_PRECISE;
   transform.fake_fp64 =
      vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_FAKE_FP64;

   transform.clipdist0_out = ~0;
   transform.clipdist1_out = ~0;
   transform.clipdist_out_temp = ~0;

   transform.layer_in = ~0;
   transform.viewport_in = ~0;
   transform.layer_in_temp = ~0;
   transform.viewport_in_temp = ~0;

   tgsi_transform_shader(tokens_in, new_tokens, newLen, &transform.base);

   return new_tokens;
}
