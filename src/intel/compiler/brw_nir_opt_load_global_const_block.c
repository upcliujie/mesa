#include "brw_nir.h"
#include "compiler/nir/nir_builder.h"

static void
reswizzle_alu_uses(nir_ssa_def *def, uint8_t *reswizzle)
{
   nir_foreach_use(use_src, def) {
      /* all uses must be ALU instructions */
      assert(use_src->parent_instr->type == nir_instr_type_alu);
      nir_alu_src *alu_src = (nir_alu_src*)use_src;

      /* reswizzle ALU sources */
      for (unsigned i = 0; i < NIR_MAX_VEC_COMPONENTS; i++)
         alu_src->swizzle[i] = reswizzle[alu_src->swizzle[i]];
   }
}

static bool
is_only_used_by_alu(nir_ssa_def *def)
{
   nir_foreach_use(use_src, def) {
      if (use_src->parent_instr->type != nir_instr_type_alu)
         return false;
   }

   return true;
}

static void
print_can_t(nir_instr *instr, const char *reason)
{
   fprintf(stderr, "can't shrink block_%u (%s):\n", instr->block->index, reason);
   nir_print_instr(instr, stderr);
   fprintf(stderr, "\n");
}

static bool
brw_nir_opt_load_global_const_block_instr(nir_builder *b,
                                          nir_instr *instr,
                                          UNUSED void *cb_data)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
   if (intrin->intrinsic != nir_intrinsic_load_global_const_block_intel)
      return false;

   b->cursor = nir_before_instr(instr);

   nir_ssa_def *def = &intrin->dest.ssa;
   assert(def->bit_size == 32);
   unsigned load_size_b = def->num_components * 4;

   /* The minimum load size for this intrinsic is 32bytes. */
   if (load_size_b == 32) {
      print_can_t(instr, "too small");
      return false;
   }

   /* don't remove any channels if used by non-ALU */
   if (!is_only_used_by_alu(def)) {
      print_can_t(instr, "used non alu");
      return false;
   }

   unsigned mask = nir_ssa_def_components_read(def);

   /* If nothing was read, leave it up to DCE. */
   if (!mask) {
      print_can_t(instr, "no mask");
      return false;
   }

   bool progress = false;
   uint8_t reswizzle[NIR_MAX_VEC_COMPONENTS] = { 0 };
   for (unsigned c = 0; c < def->num_components; c++)
      reswizzle[c] = c;

   /* Trim the top components */
   const unsigned top_mask = 0xff << (def->num_components - 8);
   if ((mask & top_mask) == 0) {
      def->num_components -= 8;
      intrin->num_components -= 8;
      progress = true;
   }

   /* Trim the bottom components */
   if ((mask & 0xff) == 0) {
      for (unsigned c = 8; c < def->num_components; c++)
         reswizzle[c] = c - 8;

      nir_ssa_def *addr = intrin->src[0].ssa;
      nir_instr_rewrite_src_ssa(instr,
                                &intrin->src[0],
                                nir_iadd_imm(b, addr, 32));
      def->num_components -= 8;
      intrin->num_components -= 8;

      mask >>= 8;
      progress = true;
   }

   if (progress)
      reswizzle_alu_uses(def, reswizzle);

   return progress;

}

bool
brw_nir_opt_load_global_const_block(nir_shader *shader)
{
   return nir_shader_instructions_pass(shader,
                                       brw_nir_opt_load_global_const_block_instr,
                                       nir_metadata_block_index |
                                       nir_metadata_dominance,
                                       NULL);
}
