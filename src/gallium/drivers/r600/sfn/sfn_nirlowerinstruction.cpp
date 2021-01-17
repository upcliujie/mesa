#include "sfn_nirlowerinstruction.h"

namespace r600 {

NirLowerInstruction::NirLowerInstruction():
   b(nullptr)
{

}

bool NirLowerInstruction::run(nir_shader *shader)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      if (function->impl &&
          run(function->impl))
         progress = true;
   }

   return progress;
}

static nir_instr *
cursor_next_instr(nir_cursor cursor)
{
   switch (cursor.option) {
   case nir_cursor_before_block:
      for (nir_block *block = cursor.block; block;
           block = nir_block_cf_tree_next(block)) {
         nir_instr *instr = nir_block_first_instr(block);
         if (instr)
            return instr;
      }
      return NULL;

   case nir_cursor_after_block:
      cursor.block = nir_block_cf_tree_next(cursor.block);
      if (cursor.block == NULL)
         return NULL;

      cursor.option = nir_cursor_before_block;
      return cursor_next_instr(cursor);

   case nir_cursor_before_instr:
      return cursor.instr;

   case nir_cursor_after_instr:
      if (nir_instr_next(cursor.instr))
         return nir_instr_next(cursor.instr);

      cursor.option = nir_cursor_after_block;
      cursor.block = cursor.instr->block;
      return cursor_next_instr(cursor);
   }

   unreachable("Inavlid cursor option");
}

ASSERTED static bool
dest_is_ssa(nir_dest *dest, void *_state)
{
   (void) _state;
   return dest->is_ssa;
}

bool NirLowerInstruction::run(nir_function_impl *func)
{
   nir_builder builder;
   nir_builder_init(&builder, func);

   b = &builder;

   nir_metadata preserved = nir_metadata_block_index |
                            nir_metadata_dominance;

   bool progress = false;
   nir_cursor iter = nir_before_cf_list(&func->body);
   nir_instr *instr;
   while ((instr = cursor_next_instr(iter)) != NULL) {
      if (!filter(instr)) {
         iter = nir_after_instr(instr);
         continue;
      }

      assert(nir_foreach_dest(instr, dest_is_ssa, NULL));
      nir_ssa_def *old_def = nir_instr_ssa_def(instr);
      struct list_head old_uses, old_if_uses;
      if (old_def != NULL) {
         /* We're about to ask the callback to generate a replacement for instr.
          * Save off the uses from instr's SSA def so we know what uses to
          * rewrite later.  If we use nir_ssa_def_rewrite_uses, it fails in the
          * case where the generated replacement code uses the result of instr
          * itself.  If we use nir_ssa_def_rewrite_uses_after (which is the
          * normal solution to this problem), it doesn't work well if control-
          * flow is inserted as part of the replacement, doesn't handle cases
          * where the replacement is something consumed by instr, and suffers
          * from performance issues.  This is the only way to 100% guarantee
          * that we rewrite the correct set efficiently.
          */

         list_replace(&old_def->uses, &old_uses);
         list_inithead(&old_def->uses);
         list_replace(&old_def->if_uses, &old_if_uses);
         list_inithead(&old_def->if_uses);
      }

      builder.cursor = nir_after_instr(instr);
      nir_ssa_def *new_def = lower(instr);
      if (new_def && new_def != NIR_LOWER_INSTR_PROGRESS &&
          new_def != NIR_LOWER_INSTR_PROGRESS_REPLACE) {
         assert(old_def != NULL);
         if (new_def->parent_instr->block != instr->block)
            preserved = nir_metadata_none;

         nir_src new_src = nir_src_for_ssa(new_def);
         list_for_each_entry_safe(nir_src, use_src, &old_uses, use_link)
            nir_instr_rewrite_src(use_src->parent_instr, use_src, new_src);

         list_for_each_entry_safe(nir_src, use_src, &old_if_uses, use_link)
            nir_if_rewrite_condition(use_src->parent_if, new_src);

         if (list_is_empty(&old_def->uses) && list_is_empty(&old_def->if_uses)) {
            iter = nir_instr_remove(instr);
         } else {
            iter = nir_after_instr(instr);
         }
         progress = true;
      } else {
         /* We didn't end up lowering after all.  Put the uses back */
         if (old_def) {
            list_replace(&old_uses, &old_def->uses);
            list_replace(&old_if_uses, &old_def->if_uses);
         }
         if (new_def == NIR_LOWER_INSTR_PROGRESS_REPLACE) {
            /* Only instructions without a return value can be removed like this */
            assert(!old_def);
            iter = nir_instr_remove(instr);
            progress = true;
         } else
            iter = nir_after_instr(instr);

         if (new_def == NIR_LOWER_INSTR_PROGRESS)
            progress = true;
      }
   }

   if (progress) {
      nir_metadata_preserve(func, preserved);
   } else {
      nir_metadata_preserve(func, nir_metadata_all);
   }

   b = nullptr;
   return progress;
}

}
