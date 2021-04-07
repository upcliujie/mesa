/* Given a blend state, the source color, and the destination color,
 * return the blended color
 */

static nir_ssa_def *
blend(
   nir_builder *b,
   nir_lower_blend_options options,
   nir_ssa_def *src, nir_ssa_def *src1, nir_ssa_def *dst)
{
   if (options.logicop_enable)
      return nir_blend_logicop(b, options, src, dst);

   /* Grab the blend constant ahead of time */
   nir_ssa_def *bconst;
   if (options.is_bifrost) {
      /* Bifrost is a scalar architecture, so let's split loads now to avoid a
       * lowering pass.
       */
      bconst = nir_vec4(b,
                        nir_load_blend_const_color_r_float(b),
                        nir_load_blend_const_color_g_float(b),
                        nir_load_blend_const_color_b_float(b),
                        nir_load_blend_const_color_a_float(b));
   } else {
      bconst = nir_load_blend_const_color_rgba(b);
   }

   if (options.half)
      bconst = nir_f2f16(b, bconst);

   /* We blend per channel and recombine later */
   nir_ssa_def *channels[4];

   for (unsigned c = 0; c < 4; ++c) {
      /* Decide properties based on channel */
      pan_lower_blend_channel chan =
         (c < 3) ? options.rgb : options.alpha;

      nir_ssa_def *psrc = nir_channel(b, src, c);
      nir_ssa_def *pdst = nir_channel(b, dst, c);

      if (nir_blend_factored(chan.func)) {
         psrc = nir_blend_factor(
                   b, psrc,
                   src, src1, dst, bconst, c,
                   chan.src_factor, chan.invert_src_factor, options.half);

         pdst = nir_blend_factor(
                   b, pdst,
                   src, src1, dst, bconst, c,
                   chan.dst_factor, chan.invert_dst_factor, options.half);
      }

      channels[c] = nir_blend_func(b, chan.func, psrc, pdst);
   }

   /* Then just recombine with an applied colormask */
   nir_ssa_def *blended = nir_vec(b, channels, 4);
   return nir_color_mask(b, options.colormask, blended, dst);
}

void
nir_lower_blend(nir_shader *shader)
{
   assert(shader->info.stage == MESA_SHADER_FRAGMENT);
   nir_foreach_function(func, shader) {
      nir_foreach_block(block, func->impl) {
         nir_foreach_instr_safe(instr, block) {
            if (instr->type != nir_instr_type_intrinsic)
               continue;

            nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
            if (intr->intrinsic != nir_intrinsic_store_deref)
               continue;

            nir_variable *var = nir_intrinsic_get_var(intr, 0);
            if (var->data.location != FRAG_RESULT_COLOR //
                var->data.location < FRAG_RESULT_DATA0)
               continue;

            nir_builder b;
            nir_builder_init(&b, func->impl);
            b.cursor = nir_before_instr(instr);

            /* source color */
            nir_ssa_def *src = nir_ssa_for_src(&b, intr->src[1], 4);

            /* destination color; mark output as fbfetch first */
            var->data.fb_fetch_output = true;
            shader->info.outputs_read |= BITFIELD64_BIT(var->data.location);
            shader->info.fs.uses_fbfetch_output = true;
            nir_ssa_def *dst = nir_load_var(&b, var);

            /* Blend the two colors per the passed options */
            nir_ssa_def *blended = blend(&b, src, dst, dst);

            /* Write out the final color instead of the input */
            nir_instr_rewrite_src(instr, &intr->src[1],
                                  nir_src_for_ssa(blended));

         }
      }

      nir_metadata_preserve(func->impl, nir_metadata_block_index |
                            nir_metadata_dominance);
   }
}
