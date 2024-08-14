/*
 * Copyright © 2024 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "aco_builder.h"
#include "aco_ir.h"

#include "util/memstream.h"
#include "util/u_debug.h"

#include <array>
#include <iomanip>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aco {

struct instr_context;

typedef void (*disasm_instr_cb)(instr_context& ctx);

struct encoding_info {
   amd_gfx_level min_gfx_level;
   amd_gfx_level max_gfx_level;
   uint32_t encoding;
   uint32_t encoding_bitsize;
   uint32_t size;
   disasm_instr_cb disasm;
};

struct disasm_context {
   Program* program;
   std::vector<bool> referenced_blocks;
   const int16_t* opcode_encodings;
   std::unordered_map<Format, std::unordered_map<uint16_t, uint16_t>> opcodes;
   std::unordered_map<uint16_t, uint16_t> vop3_opcodes;
   std::unordered_map<aco_opcode, const char*> opcode_renames;
   std::vector<encoding_info> encoding_infos;
   std::unordered_map<uint32_t, uint32_t> block_offsets;
   FILE* output;
};

struct instr_context {
   const disasm_context* disasm;
   const encoding_info* encoding;
   uint32_t* dwords;
   uint32_t instr_offset;
   uint32_t total_size;
   uint16_t op;
   Format format;
   Format encoded_format;
   bool printed_operand;
   bool has_def;
   bool has_sdwa;
   bool has_dpp8;
   bool has_dpp8_fi;
   bool has_dpp16;
   bool has_literal;
};

void
print_block_markers(const disasm_context& ctx, uint32_t* next_block, uint32_t pos)
{
   while (*next_block < ctx.program->blocks.size() &&
          pos == ctx.program->blocks[*next_block].offset) {
      if (ctx.referenced_blocks[*next_block])
         fprintf(ctx.output, "BB%u:\n", *next_block);
      (*next_block)++;
   }
}

void
print_constant_data(const disasm_context& ctx)
{
   if (ctx.program->constant_data.empty())
      return;

   fputs("\n/* constant data */\n", ctx.output);
   for (uint32_t i = 0; i < ctx.program->constant_data.size(); i += 32) {
      fprintf(ctx.output, "[%.6u]", i);
      uint32_t line_size = std::min<size_t>(ctx.program->constant_data.size() - i, 32);
      for (uint32_t j = 0; j < line_size; j += 4) {
         uint32_t size = std::min<size_t>(ctx.program->constant_data.size() - (i + j), 4);
         uint32_t v = 0;
         memcpy(&v, &ctx.program->constant_data[i + j], size);
         fprintf(ctx.output, " %.8x", v);
      }
      fputc('\n', ctx.output);
   }
}

uint32_t
bfe(const instr_context& ctx, uint32_t start, uint32_t count)
{
   assert(count);

   uint32_t first_bit_count = start % 32;
   uint32_t lower = ctx.dwords[start / 32] >> first_bit_count;
   uint32_t upper = first_bit_count ? ctx.dwords[start / 32 + 1] << (32 - first_bit_count) : 0;
   uint32_t total = lower | upper;

   return count != 32 ? total & ((1u << count) - 1u) : total;
}

uint32_t
bfe(uint32_t dword, uint32_t start, uint32_t count)
{
   assert(count);

   return (dword >> start) & BITFIELD_MASK(count);
}

int32_t
u2i(uint32_t word, uint32_t bitsize)
{
   if (word & BITFIELD_BIT(bitsize - 1))
      return (int32_t)word - (int32_t)BITFIELD_BIT(bitsize);
   else
      return word;
}

bool
print_flag(instr_context& ctx, const char* name, uint32_t bit)
{
   if (bfe(ctx, bit, 1)) {
      fprintf(ctx.disasm->output, "%s", name);
      return true;
   } else {
      return false;
   }
}

static void
print_integer_array(instr_context& ctx, const char* name, uint32_t* data, uint32_t length,
                    uint32_t ignored)
{
   if (!length)
      return;

   bool ignore = true;
   for (uint32_t i = 0; i < length; i++) {
      if (data[i] != ignored) {
         ignore = false;
         break;
      }
   }

   if (ignore)
      return;

   fprintf(ctx.disasm->output, " %s:[", name);
   for (uint32_t i = 0; i < length; i++) {
      if (i == 0)
         fprintf(ctx.disasm->output, "%u", data[i]);
      else
         fprintf(ctx.disasm->output, ",%u", data[i]);
   }
   fprintf(ctx.disasm->output, "]");
}

void
parse_opcode(instr_context& ctx, Format format, uint16_t opcode)
{
   const auto& ops = ctx.disasm->opcodes.at(format);
   if (ops.find(opcode) != ops.end()) {
      ctx.op = ops.at(opcode);
      ctx.format = format;
      ctx.encoded_format = format;

      Instruction instr = {
         .opcode = (aco_opcode)ctx.op,
         .format = ctx.format,
      };
      ctx.has_def =
         !!get_definition_info(ctx.disasm->program->gfx_level, &instr, 0).type != alu_invalid_type;
   } else {
      ctx.op = (uint16_t)aco_opcode::num_opcodes;
   }
}

bool
aco_opcode_has_e64(aco_opcode op)
{
   switch (op) {
   case aco_opcode::v_swap_b32:
   case aco_opcode::v_swaprel_b32:
   case aco_opcode::v_readfirstlane_b32:
   case aco_opcode::v_fmamk_f16:
   case aco_opcode::v_fmaak_f16:
   case aco_opcode::v_madak_f16:
   case aco_opcode::v_madmk_f16:
   case aco_opcode::v_fmamk_f32:
   case aco_opcode::v_fmaak_f32:
   case aco_opcode::v_madak_f32:
   case aco_opcode::v_madmk_f32:
   case aco_opcode::v_pk_fmac_f16: return false;
   default: return true;
   }
}

void
print_opcode(instr_context& ctx, Format format, uint16_t opcode)
{
   parse_opcode(ctx, format, opcode);
   if (ctx.op < (uint16_t)aco_opcode::num_opcodes) {
      if (ctx.disasm->opcode_renames.find((aco_opcode)ctx.op) != ctx.disasm->opcode_renames.end())
         fprintf(ctx.disasm->output, "%s", ctx.disasm->opcode_renames.at((aco_opcode)ctx.op));
      else
         fprintf(ctx.disasm->output, "%s", instr_info.name[ctx.op]);

      if (ctx.has_dpp8 || ctx.has_dpp8_fi || ctx.has_dpp16) {
         if (ctx.encoding->size == 2)
            fprintf(ctx.disasm->output, "_e64");
         fprintf(ctx.disasm->output, "_dpp");
         ctx.total_size++;
      } else if (ctx.has_sdwa) {
         if (ctx.disasm->program->gfx_level > GFX8 || format != Format::VOPC)
            fprintf(ctx.disasm->output, "_sdwa");
         ctx.total_size++;
      } else if ((format == Format::VOP1 || format == Format::VOP2 || format == Format::VOPC ||
                  format == Format::VINTRP) &&
                 aco_opcode_has_e64((aco_opcode)ctx.op)) {
         fprintf(ctx.disasm->output, "_e%u", ctx.encoding->size * 32);
      }
   } else {
      fprintf(ctx.disasm->output, "(invalid opcode)");
   }
}

enum {
   vgpr = 0x100,
};

void
print_sdwa_sel(instr_context& ctx, const char* src, uint32_t sel)
{
   switch (sel) {
   case 0: fprintf(ctx.disasm->output, " %s:BYTE_0", src); break;
   case 1: fprintf(ctx.disasm->output, " %s:BYTE_1", src); break;
   case 2: fprintf(ctx.disasm->output, " %s:BYTE_2", src); break;
   case 3: fprintf(ctx.disasm->output, " %s:BYTE_3", src); break;
   case 4: fprintf(ctx.disasm->output, " %s:WORD_0", src); break;
   case 5: fprintf(ctx.disasm->output, " %s:WORD_1", src); break;
   case 6: fprintf(ctx.disasm->output, " %s:DWORD", src); break;
   default: break;
   }
}

void
print_sdwa_unused(instr_context& ctx, uint32_t unused)
{
   switch (unused) {
   case 0: fprintf(ctx.disasm->output, " dst_unused:UNUSED_PAD"); break;
   case 1: fprintf(ctx.disasm->output, " dst_unused:UNUSED_SEXT"); break;
   case 2: fprintf(ctx.disasm->output, " dst_unused:UNUSED_PRESERVE"); break;
   default: break;
   }
}

void
print_omod(instr_context& ctx, uint32_t omod)
{
   switch (omod) {
   case 1: fprintf(ctx.disasm->output, " mul:2"); break;
   case 2: fprintf(ctx.disasm->output, " mul:4"); break;
   case 3: fprintf(ctx.disasm->output, " div:2"); break;
   default: break;
   }
}

PhysReg
parse_reg_src(instr_context& ctx, uint32_t reg)
{
   if (ctx.disasm->program->gfx_level >= GFX11) {
      if (reg == m0.reg())
         return sgpr_null;
      else if (reg == sgpr_null.reg())
         return m0;
   }
   return PhysReg(reg);
}

// uint32_t
// get_type_size(instr_context& ctx, aco_type type)
// {
//    switch (type) {
//    case type_m0:
//    case type_scc: return 0;
//    case type_exec_lo: return 4;
//    case type_exec:
//    case type_vcc: return ctx.disasm->program->wave_size / 8;
//    default: return type_get_bytes(type);
//    }
// }

enum operand_opsel {
   operand_opsel_full,
   operand_opsel_lo,
   operand_opsel_hi,
};

struct additional_operand_info {
   bool skip_comma;
   bool print_null;
   uint32_t min_count;
   uint32_t count;
   operand_opsel opsel;
   bool neg;
   bool abs;
   bool tfe;
};

enum {
   operand_index_def = (1u << 31u),
};

// aco_type
// get_operand_type(instr_context& ctx, uint32_t index)
// {
//    Instruction instr = {
//       .opcode = (aco_opcode)ctx.op,
//    };

//    if (index & operand_index_def) {
//       return get_definition_type(ctx.disasm->program->gfx_level, &instr, 0);
//    } else {
//       if (instr.opcode == aco_opcode::v_fma_mix_f32 ||
//           instr.opcode == aco_opcode::v_fma_mixlo_f16 ||
//           instr.opcode == aco_opcode::v_fma_mixhi_f16)
//          return aco_type::type_f32;

//       switch (ctx.format) {
//       case Format::EXP:
//       case Format::SMEM:
//       case Format::DS:
//       case Format::LDSDIR:
//       case Format::MIMG:
//       case Format::FLAT:
//       case Format::GLOBAL:
//       case Format::SCRATCH: return aco_type::type_f32;
//       default: return get_operand_type(&instr, index);
//       }
//    }
// }

void
print_literal(instr_context& ctx, uint32_t literal, uint32_t size, operand_opsel opsel)
{
   /* TODO: LLVM displays certain literals like inline constants. */
   if (opsel == operand_opsel_full) {
      if (size == 2)
         fprintf(ctx.disasm->output, "0x%x", literal & 0xFFFF);
      else
         fprintf(ctx.disasm->output, "0x%x", literal);
   } else if (opsel == operand_opsel_lo) {
      if ((literal & 0xFFFF) > 64)
         fprintf(ctx.disasm->output, "0x%x", literal & 0xFFFF);
      else
         fprintf(ctx.disasm->output, "%u", literal & 0xFFFF);
   } else if (opsel == operand_opsel_hi) {
      fprintf(ctx.disasm->output, "0x%x", literal >> 16);
   }
}

bool
print_operand(instr_context& ctx, uint32_t operand, uint32_t index,
              std::optional<additional_operand_info> additional_info = std::nullopt)
{
   Instruction instr = {
      .opcode = (aco_opcode)ctx.op,
      .format = ctx.format,
   };
   SrcDestInfo info;
   if (index == operand_index_def)
      info = get_definition_info(ctx.disasm->program->gfx_level, &instr, 0);
   else
      info = get_operand_info(ctx.disasm->program->gfx_level, &instr, index);

   uint32_t bytes = info.bytes();
   if (info.bitsize == 1)
      bytes = ctx.disasm->program->wave_size / 8;

   if (index != operand_index_def) {
      if (instr.opcode == aco_opcode::v_fma_mix_f32 ||
          instr.opcode == aco_opcode::v_fma_mixlo_f16 ||
          instr.opcode == aco_opcode::v_fma_mixhi_f16)
         bytes = 4;

      switch (ctx.format) {
      case Format::EXP:
      case Format::SMEM:
      case Format::DS:
      case Format::LDSDIR:
      case Format::MIMG:
      case Format::FLAT:
      case Format::GLOBAL:
      case Format::SCRATCH: bytes = 4; break;
      default: break;
      }
   }

   if (info.should_be_fixed) {
      switch (info.reg) {
      case m0:
      case scc: bytes = 0; break;
      case exec:
      case vcc:
         if (bytes == 0)
            bytes = ctx.disasm->program->wave_size / 8;
         break;
      default: break;
      }
   }

   if (additional_info)
      bytes = MAX2(bytes, additional_info->min_count * 4);
   if (bytes == 0)
      return false;
   if (additional_info && additional_info->count)
      bytes = additional_info->count * 4;

   if (ctx.has_sdwa && index < 2) {
      if (index == 0)
         operand = bfe(ctx, 32, 8);

      if (bfe(ctx, 55 + index * 8, 1))
         operand &= ~vgpr;
      else
         operand |= vgpr;
   }

   if ((ctx.has_dpp8 || ctx.has_dpp8_fi || ctx.has_dpp16) && index == 0)
      operand = bfe(ctx, ctx.encoding->size * 32, 8) | vgpr;

   bool has_opsel_gfx11 = false;
   bool opsel_gfx11 = false;
   if (ctx.disasm->program->gfx_level >= GFX11 &&
       (ctx.encoded_format == Format::VOP1 || ctx.encoded_format == Format::VOP2 ||
        ctx.encoded_format == Format::VOPC) &&
       bytes == 2 && (operand & vgpr)) {
      switch ((aco_opcode)ctx.op) {
      case aco_opcode::v_cvt_u32_u16:
      case aco_opcode::v_cvt_i32_i16:
      case aco_opcode::v_cmp_le_i16: break;
      default:
         has_opsel_gfx11 = true;
         opsel_gfx11 = operand & 128;
         operand &= ~128;
         break;
      }
   }

   PhysReg reg = parse_reg_src(ctx, operand);
   if (reg == sgpr_null && !(additional_info && additional_info->print_null))
      return false;

   bool is_reg = reg < PhysReg{106} || reg > PhysReg{255};

   if ((ctx.printed_operand || ctx.has_def) &&
       (!(index & operand_index_def) || (index & ~operand_index_def) > 0)) {
      if (!additional_info || !additional_info->skip_comma)
         fprintf(ctx.disasm->output, ", ");
   } else {
      fprintf(ctx.disasm->output, " ");
   }

   bool abs = additional_info && additional_info->abs;
   bool neg = additional_info && additional_info->neg;

   if (ctx.has_sdwa && index < 2) {
      neg |= !!bfe(ctx, 52 + index * 8, 1);
      abs |= !!bfe(ctx, 53 + index * 8, 1);
   }

   if (ctx.has_dpp16 && index < 2) {
      neg |= !!bfe(ctx, ctx.encoding->size * 32 + 20 + index * 2, 1);
      abs |= !!bfe(ctx, ctx.encoding->size * 32 + 21 + index * 2, 1);
   }

   if (neg && is_reg)
      fprintf(ctx.disasm->output, "-");

   uint32_t modifiers = 0;
   if (ctx.has_sdwa && index < 2)
      modifiers += !!print_flag(ctx, "sext(", 51 + index * 8);

   if (neg && !is_reg) {
      fprintf(ctx.disasm->output, "neg(");
      modifiers++;
   }

   if (abs)
      fprintf(ctx.disasm->output, "|");

   if (additional_info && additional_info->tfe)
      bytes += 4;

   if (reg == PhysReg{255}) {
      if (bytes) {
         operand_opsel opsel = operand_opsel_full;
         if (additional_info)
            opsel = additional_info->opsel;

         print_literal(ctx, ctx.dwords[ctx.encoding->size], bytes, opsel);

         if (!ctx.has_literal) {
            ctx.total_size++;
            ctx.has_literal = true;
         }
      }
   } else {
      if (reg == scc && !(index & operand_index_def))
         fprintf(ctx.disasm->output, "src_");

      if (ctx.disasm->program->gfx_level <= GFX9) {
         switch (reg) {
         case 102: fprintf(ctx.disasm->output, "flat_scratch_lo"); break;
         case 103: fprintf(ctx.disasm->output, "flat_scratch_hi"); break;
         case 104: fprintf(ctx.disasm->output, "xnack_mask_lo"); break;
         case 105: fprintf(ctx.disasm->output, "xnack_mask_hi"); break;
         default:
            print_phys_reg(reg, ctx.disasm->output, align(bytes, 4), print_no_ssa, ':');
            break;
         }
      } else {
         print_phys_reg(reg, ctx.disasm->output, align(bytes, 4), print_no_ssa, ':');
      }
   }

   if (has_opsel_gfx11) {
      if (opsel_gfx11)
         fprintf(ctx.disasm->output, ".h");
      else
         fprintf(ctx.disasm->output, ".l");
   }

   if (abs)
      fprintf(ctx.disasm->output, "|");

   for (uint32_t i = 0; i < modifiers; i++)
      fprintf(ctx.disasm->output, ")");

   ctx.printed_operand = true;

   return true;
}

void
print_definition(instr_context& ctx, uint32_t def)
{
   if (!ctx.has_def)
      return;

   if (ctx.has_sdwa && ctx.format == Format::VOPC) {
      def = bfe(ctx, 40, 7);
      if (bfe(ctx, 47, 1) == 0) {
         def = vcc.reg();
      }
   }

   print_operand(ctx, def, operand_index_def);
}

void
print_dpp(instr_context& ctx)
{
   if (ctx.has_dpp8 || ctx.has_dpp8_fi) {
      fprintf(ctx.disasm->output, " dpp8:[");
      for (uint32_t i = 0; i < 8; i++) {
         if (i > 0)
            fprintf(ctx.disasm->output, ",");
         fprintf(ctx.disasm->output, "%u", bfe(ctx, ctx.encoding->size * 32 + 8 + i * 3, 3));
      }
      fprintf(ctx.disasm->output, "]");

      if (ctx.has_dpp8_fi)
         fprintf(ctx.disasm->output, " fi:1");
   }

   if (!ctx.has_dpp16)
      return;

   uint32_t dpp_ctrl = bfe(ctx, ctx.encoding->size * 32 + 8, 9);

   if (dpp_ctrl <= 0xFF) {
      fprintf(ctx.disasm->output, " quad_perm:[%u,%u,%u,%u]", bfe(dpp_ctrl, 0, 2),
              bfe(dpp_ctrl, 2, 2), bfe(dpp_ctrl, 4, 2), bfe(dpp_ctrl, 6, 2));
   } else if (dpp_ctrl >= 0x101 && dpp_ctrl <= 0x10f) {
      fprintf(ctx.disasm->output, " row_shl:%u", dpp_ctrl - 0x100);
   } else if (dpp_ctrl >= 0x111 && dpp_ctrl <= 0x11f) {
      fprintf(ctx.disasm->output, " row_shr:%u", dpp_ctrl - 0x110);
   } else if (dpp_ctrl >= 0x121 && dpp_ctrl <= 0x12f) {
      fprintf(ctx.disasm->output, " row_ror:%u", dpp_ctrl - 0x120);
   } else if (dpp_ctrl == 0x140) {
      fprintf(ctx.disasm->output, " row_mirror");
   } else if (dpp_ctrl == 0x141) {
      fprintf(ctx.disasm->output, " row_half_mirror");
   }

   if (ctx.disasm->program->gfx_level <= GFX9) {
      if (dpp_ctrl == 0x130) {
         fprintf(ctx.disasm->output, " wave_shl:1");
      } else if (dpp_ctrl == 0x134) {
         fprintf(ctx.disasm->output, " wave_rol:1");
      } else if (dpp_ctrl == 0x138) {
         fprintf(ctx.disasm->output, " wave_shr:1");
      } else if (dpp_ctrl == 0x13C) {
         fprintf(ctx.disasm->output, " wave_ror:1");
      } else if (dpp_ctrl == 0x142) {
         fprintf(ctx.disasm->output, " row_bcast:15");
      } else if (dpp_ctrl == 0x143) {
         fprintf(ctx.disasm->output, " row_bcast:31");
      }
   }

   if (ctx.disasm->program->gfx_level >= GFX11) {
      if (dpp_ctrl >= 0x150 && dpp_ctrl <= 0x15F) {
         fprintf(ctx.disasm->output, " row_share:%u", dpp_ctrl - 0x150);
      } else if (dpp_ctrl >= 0x160 && dpp_ctrl <= 0x16F) {
         fprintf(ctx.disasm->output, " row_xmask:%u", dpp_ctrl - 0x160);
      }
   }

   fprintf(ctx.disasm->output, " row_mask:0x%x", bfe(ctx, ctx.encoding->size * 32 + 28, 4));
   fprintf(ctx.disasm->output, " bank_mask:0x%x", bfe(ctx, ctx.encoding->size * 32 + 24, 4));
   print_flag(ctx, " bound_ctrl:1", ctx.encoding->size * 32 + 19);

   if (ctx.disasm->program->gfx_level > GFX9)
      print_flag(ctx, " fi:1", ctx.encoding->size * 32 + 18);
}

void
disasm_sop1(instr_context& ctx)
{
   print_opcode(ctx, Format::SOP1, bfe(ctx, 8, 8));
   print_operand(ctx, bfe(ctx, 16, 7), operand_index_def,
                 additional_operand_info{.print_null = true});
   print_operand(ctx, bfe(ctx, 0, 8), 0);
}

void
disasm_sop2(instr_context& ctx)
{
   print_opcode(ctx, Format::SOP2, bfe(ctx, 23, 7));
   print_definition(ctx, bfe(ctx, 16, 7));
   print_operand(ctx, bfe(ctx, 0, 8), 0);
   print_operand(ctx, bfe(ctx, 8, 8), 1);
}

static void
print_hwreg(instr_context& ctx, uint16_t reg)
{
   switch (reg & 31) {
   case 1: fprintf(ctx.disasm->output, "hwreg(HW_REG_MODE)"); break;
   case 2: fprintf(ctx.disasm->output, "hwreg(HW_REG_STATUS)"); break;
   case 3: fprintf(ctx.disasm->output, "hwreg(HW_REG_TRAPSTS)"); break;
   case 4: fprintf(ctx.disasm->output, "hwreg(HW_REG_HW_ID)"); break;
   case 5: fprintf(ctx.disasm->output, "hwreg(HW_REG_GPR_ALLOC)"); break;
   case 6: fprintf(ctx.disasm->output, "hwreg(HW_REG_LDS_ALLOC)"); break;
   case 7: fprintf(ctx.disasm->output, "hwreg(HW_REG_IB_STS)"); break;
   case 15: fprintf(ctx.disasm->output, "hwreg(HW_REG_SH_MEM_BASES)"); break;
   case 16: fprintf(ctx.disasm->output, "hwreg(HW_REG_TBA_LO)"); break;
   case 17: fprintf(ctx.disasm->output, "hwreg(HW_REG_TBA_HI)"); break;
   case 18: fprintf(ctx.disasm->output, "hwreg(HW_REG_TMA_LO)"); break;
   case 19: fprintf(ctx.disasm->output, "hwreg(HW_REG_TMA_HI)"); break;
   case 20: fprintf(ctx.disasm->output, "hwreg(HW_REG_FLAT_SCR_LO)"); break;
   case 21: fprintf(ctx.disasm->output, "hwreg(HW_REG_FLAT_SCR_HI)"); break;
   case 22: fprintf(ctx.disasm->output, "hwreg(HW_REG_XNACK_MASK)"); break;
   case 23: fprintf(ctx.disasm->output, "hwreg(HW_REG_HW_ID1)"); break;
   case 24: fprintf(ctx.disasm->output, "hwreg(HW_REG_HW_ID2)"); break;
   case 25: fprintf(ctx.disasm->output, "hwreg(HW_REG_POPS_PACKER)"); break;
   case 29: fprintf(ctx.disasm->output, "hwreg(HW_REG_SHADER_CYCLES)"); break;
   }
}

void
disasm_sopk(instr_context& ctx)
{
   print_opcode(ctx, Format::SOPK, bfe(ctx, 23, 5));

   uint16_t imm = bfe(ctx, 0, 16);
   if ((aco_opcode)ctx.op == aco_opcode::s_setreg_b32 ||
       (aco_opcode)ctx.op == aco_opcode::s_setreg_imm32_b32) {
      fprintf(ctx.disasm->output, " ");
      print_hwreg(ctx, imm);
      fprintf(ctx.disasm->output, ",");
   }

   print_operand(ctx, bfe(ctx, 16, 7), operand_index_def,
                 additional_operand_info{.print_null = true, .min_count = 1});

   if ((aco_opcode)ctx.op == aco_opcode::s_getreg_b32) {
      fprintf(ctx.disasm->output, ", ");
      print_hwreg(ctx, imm);
   } else if ((aco_opcode)ctx.op != aco_opcode::s_setreg_b32 &&
              (aco_opcode)ctx.op != aco_opcode::s_setreg_imm32_b32) {
      fprintf(ctx.disasm->output, ", 0x%x", imm);
   }
}

void
disasm_sopc(instr_context& ctx)
{
   print_opcode(ctx, Format::SOPC, bfe(ctx, 16, 7));
   fprintf(ctx.disasm->output, " ");
   print_operand(ctx, bfe(ctx, 0, 8), 0, additional_operand_info{.skip_comma = true});
   print_operand(ctx, bfe(ctx, 8, 8), 1);
}

void
disasm_sopp(instr_context& ctx)
{
   print_opcode(ctx, Format::SOPP, bfe(ctx, 16, 7));

   SALU_instruction instr = {
      .imm = bfe(ctx, 0, 16),
   };
   if (instr_info.classes[ctx.op] == instr_class::branch)
      instr.imm = ctx.disasm->block_offsets.at(ctx.instr_offset + (int16_t)instr.imm + 1);

   instr.opcode = (aco_opcode)ctx.op;
   instr.format = Format::SOPP;

   uint16_t imm = instr.imm;
   switch (instr.opcode) {
   case aco_opcode::s_waitcnt:
   case aco_opcode::s_wait_loadcnt_dscnt:
   case aco_opcode::s_wait_storecnt_dscnt: {
      wait_imm unpacked;
      unpacked.unpack(ctx.disasm->program->gfx_level, &instr);
      const char* names[wait_type_num];
      names[wait_type_exp] = "expcnt";
      names[wait_type_vm] = ctx.disasm->program->gfx_level >= GFX12 ? "loadcnt" : "vmcnt";
      names[wait_type_lgkm] = ctx.disasm->program->gfx_level >= GFX12 ? "dscnt" : "lgkmcnt";
      names[wait_type_vs] = ctx.disasm->program->gfx_level >= GFX12 ? "storecnt" : "vscnt";
      names[wait_type_sample] = "samplecnt";
      names[wait_type_bvh] = "bvhcnt";
      names[wait_type_km] = "kmcnt";
      for (int32_t i = wait_type_num - 1; i >= 0; i--) {
         if (unpacked[i] != wait_imm::unset_counter)
            fprintf(ctx.disasm->output, " %s(%d)", names[i], unpacked[i]);
      }
      break;
   }
   case aco_opcode::s_wait_expcnt:
   case aco_opcode::s_wait_dscnt:
   case aco_opcode::s_wait_loadcnt:
   case aco_opcode::s_wait_storecnt:
   case aco_opcode::s_wait_samplecnt:
   case aco_opcode::s_wait_bvhcnt:
   case aco_opcode::s_wait_kmcnt: {
      fprintf(ctx.disasm->output, " imm:%u", imm);
      break;
   }
   case aco_opcode::s_waitcnt_depctr: {
      // unsigned va_vdst = (imm >> 12) & 0xf;
      // unsigned va_sdst = (imm >> 9) & 0x7;
      // unsigned va_ssrc = (imm >> 8) & 0x1;
      // unsigned hold_cnt = (imm >> 7) & 0x1;
      // unsigned vm_vsrc = (imm >> 2) & 0x7;
      // unsigned va_vcc = (imm >> 1) & 0x1;
      // unsigned sa_sdst = imm & 0x1;
      // if (va_vdst != 0xf)
      //    fprintf(ctx.disasm->output, " va_vdst(%d)", va_vdst);
      // if (va_sdst != 0x7)
      //    fprintf(ctx.disasm->output, " va_sdst(%d)", va_sdst);
      // if (va_ssrc != 0x1)
      //    fprintf(ctx.disasm->output, " va_ssrc(%d)", va_ssrc);
      // if (hold_cnt != 0x1)
      //    fprintf(ctx.disasm->output, " holt_cnt(%d)", hold_cnt);
      // if (vm_vsrc != 0x7)
      //    fprintf(ctx.disasm->output, " vm_vsrc(%d)", vm_vsrc);
      // if (va_vcc != 0x1)
      //    fprintf(ctx.disasm->output, " va_vcc(%d)", va_vcc);
      // if (sa_sdst != 0x1)
      //    fprintf(ctx.disasm->output, " sa_sdst(%d)", sa_sdst);
      fprintf(ctx.disasm->output, " 0x%x", imm);
      break;
   }
   case aco_opcode::s_delay_alu: {
      unsigned delay[2] = {imm & 0xfu, (imm >> 7) & 0xfu};
      unsigned skip = (imm >> 4) & 0x7;
      for (unsigned i = 0; i < 2; i++) {
         alu_delay_wait wait = (alu_delay_wait)delay[i];
         if (i && wait != alu_delay_wait::NO_DEP)
            fprintf(ctx.disasm->output, " |");

         if (i == 1 && skip) {
            if (skip == 1)
               fprintf(ctx.disasm->output, " instskip(NEXT) |");
            else
               fprintf(ctx.disasm->output, " instskip(SKIP_%u) |", skip - 1);
         }

         if (wait >= alu_delay_wait::VALU_DEP_1 && wait <= alu_delay_wait::VALU_DEP_4) {
            fprintf(ctx.disasm->output, " instid%u(VALU_DEP_%u)", i, delay[i]);
         } else if (wait >= alu_delay_wait::TRANS32_DEP_1 &&
                    wait <= alu_delay_wait::TRANS32_DEP_3) {
            fprintf(ctx.disasm->output, " trans32_dep_%u",
                    delay[i] - (unsigned)alu_delay_wait::TRANS32_DEP_1 + 1);
         } else if (wait == alu_delay_wait::FMA_ACCUM_CYCLE_1) {
            fprintf(ctx.disasm->output, " fma_accum_cycle_1");
         } else if (wait >= alu_delay_wait::SALU_CYCLE_1 && wait <= alu_delay_wait::SALU_CYCLE_3) {
            fprintf(ctx.disasm->output, " instid1(SALU_CYCLE_%u)",
                    delay[i] - (unsigned)alu_delay_wait::SALU_CYCLE_1 + 1);
         }
      }
      break;
   }
   case aco_opcode::s_endpgm:
   case aco_opcode::s_endpgm_saved:
   case aco_opcode::s_endpgm_ordered_ps_done:
   case aco_opcode::s_wakeup:
   case aco_opcode::s_barrier:
   case aco_opcode::s_icache_inv:
   case aco_opcode::s_ttracedata:
   case aco_opcode::s_set_gpr_idx_off: {
      break;
   }
   case aco_opcode::s_sendmsg: {
      unsigned id = imm & sendmsg_id_mask;
      static_assert(sendmsg_gs == sendmsg_hs_tessfactor);
      static_assert(sendmsg_gs_done == sendmsg_dealloc_vgprs);
      switch (id) {
      case sendmsg_none: fprintf(ctx.disasm->output, " sendmsg(MSG_NONE)"); break;
      case sendmsg_gs:
         if (ctx.disasm->program->gfx_level >= GFX11)
            fprintf(ctx.disasm->output, " sendmsg(hs_tessfactor)");
         else
            fprintf(ctx.disasm->output, " sendmsg(MSG_GS%s%s, %u)", imm & 0x10 ? ", GS_OP_CUT" : "",
                    imm & 0x20 ? ", GS_OP_EMIT" : "", imm >> 8);
         break;
      case sendmsg_gs_done:
         if (ctx.disasm->program->gfx_level >= GFX11)
            fprintf(ctx.disasm->output, " sendmsg(MSG_DEALLOC_VGPRS)");
         else
            fprintf(ctx.disasm->output, " sendmsg(MSG_GS_DONE%s%s, %u)",
                    imm & 0x10 ? ", GS_OP_CUT" : "", imm & 0x20 ? ", GS_OP_EMIT" : "", imm >> 8);
         break;
      case sendmsg_save_wave: fprintf(ctx.disasm->output, " sendmsg(MSG_SAVEWAVE)"); break;
      case sendmsg_stall_wave_gen:
         fprintf(ctx.disasm->output, " sendmsg(MSG_STALL_WAVE_GEN)");
         break;
      case sendmsg_halt_waves: fprintf(ctx.disasm->output, " sendmsg(MSG_HALT_WAVES)"); break;
      case sendmsg_ordered_ps_done:
         fprintf(ctx.disasm->output, " sendmsg(MSG_ORDERED_PS_DONE)");
         break;
      case sendmsg_early_prim_dealloc:
         fprintf(ctx.disasm->output, " sendmsg(MSG_EARLY_PRIM_DEALLOC)");
         break;
      case sendmsg_gs_alloc_req: fprintf(ctx.disasm->output, " sendmsg(MSG_GS_ALLOC_REQ)"); break;
      case sendmsg_get_doorbell: fprintf(ctx.disasm->output, " sendmsg(MSG_GET_DOORBELL)"); break;
      case sendmsg_get_ddid: fprintf(ctx.disasm->output, " sendmsg(MSG_GET_DDID)"); break;
      default: fprintf(ctx.disasm->output, " imm:%u", imm);
      }
      break;
   }
   case aco_opcode::s_wait_event: {
      if (is_wait_export_ready(ctx.disasm->program->gfx_level, &instr))
         fprintf(ctx.disasm->output, " wait_export_ready");
      break;
   }
   case aco_opcode::s_setprio:
   case aco_opcode::s_nop: {
      fprintf(ctx.disasm->output, " %u", imm);
      break;
   }
   default: {
      if (instr_info.classes[(int)instr.opcode] == instr_class::branch)
         fprintf(ctx.disasm->output, " BB%d", imm);
      else if (imm)
         fprintf(ctx.disasm->output, " 0x%x", imm);
      break;
   }
   }
}

static const std::unordered_set<aco_opcode> smem_buffer_ops = {
   aco_opcode::s_buffer_load_dword,     aco_opcode::s_buffer_load_dwordx2,
   aco_opcode::s_buffer_load_dwordx3,   aco_opcode::s_buffer_load_dwordx4,
   aco_opcode::s_buffer_load_dwordx8,   aco_opcode::s_buffer_load_dwordx16,
   aco_opcode::s_buffer_load_sbyte,     aco_opcode::s_buffer_load_ubyte,
   aco_opcode::s_buffer_load_sshort,    aco_opcode::s_buffer_load_ushort,
   aco_opcode::s_buffer_store_dword,    aco_opcode::s_buffer_store_dwordx2,
   aco_opcode::s_buffer_store_dwordx4,  aco_opcode::s_buffer_atomic_swap,
   aco_opcode::s_buffer_atomic_cmpswap, aco_opcode::s_buffer_atomic_add,
   aco_opcode::s_buffer_atomic_sub,     aco_opcode::s_buffer_atomic_smin,
   aco_opcode::s_buffer_atomic_umin,    aco_opcode::s_buffer_atomic_smax,
   aco_opcode::s_buffer_atomic_umax,    aco_opcode::s_buffer_atomic_and,
   aco_opcode::s_buffer_atomic_or,      aco_opcode::s_buffer_atomic_xor,
   aco_opcode::s_buffer_atomic_inc,     aco_opcode::s_buffer_atomic_dec,
   aco_opcode::s_buffer_atomic_swap_x2, aco_opcode::s_buffer_atomic_cmpswap_x2,
   aco_opcode::s_buffer_atomic_add_x2,  aco_opcode::s_buffer_atomic_sub_x2,
   aco_opcode::s_buffer_atomic_smin_x2, aco_opcode::s_buffer_atomic_umin_x2,
   aco_opcode::s_buffer_atomic_smax_x2, aco_opcode::s_buffer_atomic_umax_x2,
   aco_opcode::s_buffer_atomic_and_x2,  aco_opcode::s_buffer_atomic_or_x2,
   aco_opcode::s_buffer_atomic_xor_x2,  aco_opcode::s_buffer_atomic_inc_x2,
   aco_opcode::s_buffer_atomic_dec_x2,
};

void
disasm_smem(instr_context& ctx)
{
   print_opcode(ctx, Format::SMEM, bfe(ctx, 22, 5));

   print_definition(ctx, bfe(ctx, 15, 7));

   if ((aco_opcode)ctx.op == aco_opcode::s_memtime || (aco_opcode)ctx.op == aco_opcode::s_memtime ||
       (aco_opcode)ctx.op == aco_opcode::s_dcache_inv ||
       (aco_opcode)ctx.op == aco_opcode::s_dcache_inv_vol)
      return;

   if (smem_buffer_ops.find((aco_opcode)ctx.op) == smem_buffer_ops.end())
      print_operand(ctx, bfe(ctx, 9, 6) << 1, 0, additional_operand_info{.count = 2});
   else
      print_operand(ctx, bfe(ctx, 9, 6) << 1, 0, additional_operand_info{.count = 4});

   bool imm = !!bfe(ctx, 8, 1);
   uint32_t offset = bfe(ctx, 0, 8);
   if (imm) {
      fprintf(ctx.disasm->output, ", 0x%x", offset);
   } else {
      if (offset == 255) {
         fprintf(ctx.disasm->output, ", 0x%x", ctx.dwords[ctx.encoding->size]);
         ctx.has_literal = true;
      } else {
         print_operand(ctx, offset, 1);
      }
   }
}

void
disasm_smem_gfx8(instr_context& ctx)
{
   print_opcode(ctx, Format::SMEM, bfe(ctx, 18, 8));

   print_definition(ctx, bfe(ctx, 6, 7));

   if ((aco_opcode)ctx.op == aco_opcode::s_memtime || (aco_opcode)ctx.op == aco_opcode::s_memtime ||
       (aco_opcode)ctx.op == aco_opcode::s_dcache_inv ||
       (aco_opcode)ctx.op == aco_opcode::s_dcache_inv_vol)
      return;

   if (smem_buffer_ops.find((aco_opcode)ctx.op) == smem_buffer_ops.end())
      print_operand(ctx, bfe(ctx, 0, 6) << 1, 0, additional_operand_info{.count = 2});
   else
      print_operand(ctx, bfe(ctx, 0, 6) << 1, 0, additional_operand_info{.count = 4});

   uint32_t offset = bfe(ctx, 32, 21);

   bool printed_soffset = false;
   if (bfe(ctx, 14, 1)) {
      printed_soffset =
         print_operand(ctx, bfe(ctx, 57, 7), 1, additional_operand_info{.print_null = !offset});
   }

   if (bfe(ctx, 17, 1)) {
      /* LLVM consistent printing :) */
      if (printed_soffset)
         fprintf(ctx.disasm->output, " offset:0x%x", u2i(offset, 21));
      else
         fprintf(ctx.disasm->output, ", 0x%x", u2i(offset, 21));
   } else {
      print_operand(ctx, bfe(offset, 0, 7), 1, additional_operand_info{.count = 1});
   }

   print_flag(ctx, " nv", 15);
   print_flag(ctx, " glc", 16);
}

void
disasm_smem_gfx10(instr_context& ctx)
{
   print_opcode(ctx, Format::SMEM, bfe(ctx, 18, 8));

   print_definition(ctx, bfe(ctx, 6, 7));

   if ((aco_opcode)ctx.op == aco_opcode::s_memtime || (aco_opcode)ctx.op == aco_opcode::s_memtime ||
       (aco_opcode)ctx.op == aco_opcode::s_dcache_inv ||
       (aco_opcode)ctx.op == aco_opcode::s_dcache_inv_vol)
      return;

   if (smem_buffer_ops.find((aco_opcode)ctx.op) == smem_buffer_ops.end())
      print_operand(ctx, bfe(ctx, 0, 6) << 1, 0, additional_operand_info{.count = 2});
   else
      print_operand(ctx, bfe(ctx, 0, 6) << 1, 0, additional_operand_info{.count = 4});

   uint32_t offset = bfe(ctx, 32, 21);

   bool printed_soffset =
      print_operand(ctx, bfe(ctx, 57, 7), 1, additional_operand_info{.print_null = !offset});

   /* LLVM consistent printing :) */
   if (offset) {
      if (printed_soffset)
         fprintf(ctx.disasm->output, " offset:0x%x", u2i(offset, 21));
      else
         fprintf(ctx.disasm->output, ", 0x%x", u2i(offset, 21));
   }

   print_flag(ctx, " dlc", ctx.disasm->program->gfx_level >= GFX11 ? 13 : 14);
   print_flag(ctx, " glc", ctx.disasm->program->gfx_level >= GFX11 ? 14 : 16);
}
void
disasm_vop1(instr_context& ctx)
{
   ctx.has_sdwa = bfe(ctx, 0, 9) == 249;
   ctx.has_dpp8 = bfe(ctx, 0, 9) == 233;
   ctx.has_dpp8_fi = bfe(ctx, 0, 9) == 234;
   ctx.has_dpp16 = bfe(ctx, 0, 9) == 250;

   print_opcode(ctx, Format::VOP1, bfe(ctx, 9, 8));

   if ((aco_opcode)ctx.op == aco_opcode::v_readfirstlane_b32)
      print_definition(ctx, bfe(ctx, 17, 8));
   else
      print_definition(ctx, bfe(ctx, 17, 8) | vgpr);

   print_operand(ctx, bfe(ctx, 0, 9), 0);

   if (ctx.has_sdwa) {
      print_flag(ctx, " clamp", 45);
      print_omod(ctx, bfe(ctx, 46, 2));
      print_sdwa_sel(ctx, "dst_sel", bfe(ctx, 40, 3));
      print_sdwa_unused(ctx, bfe(ctx, 43, 2));
      print_sdwa_sel(ctx, "src0_sel", bfe(ctx, 48, 3));
   }

   print_dpp(ctx);
}

void
disasm_vop2(instr_context& ctx)
{
   ctx.has_sdwa = bfe(ctx, 0, 9) == 249;
   ctx.has_dpp8 = bfe(ctx, 0, 9) == 233;
   ctx.has_dpp8_fi = bfe(ctx, 0, 9) == 234;
   ctx.has_dpp16 = bfe(ctx, 0, 9) == 250;

   print_opcode(ctx, Format::VOP2, bfe(ctx, 25, 6));

   print_definition(ctx, bfe(ctx, 17, 8) | vgpr);

   switch ((aco_opcode)ctx.op) {
   case aco_opcode::v_addc_co_u32:
   case aco_opcode::v_subb_co_u32:
   case aco_opcode::v_subbrev_co_u32:
   case aco_opcode::v_add_co_u32:
   case aco_opcode::v_sub_co_u32:
   case aco_opcode::v_subrev_co_u32:
      print_operand(ctx, vcc.reg(), operand_index_def | 1,
                    additional_operand_info{.count = ctx.disasm->program->wave_size / 32});
      break;
   default: break;
   }

   print_operand(ctx, bfe(ctx, 0, 9), 0);

   switch ((aco_opcode)ctx.op) {
   case aco_opcode::v_fmamk_f16:
   case aco_opcode::v_madmk_f16:
   case aco_opcode::v_fmamk_f32:
   case aco_opcode::v_madmk_f32:
      fprintf(ctx.disasm->output, ", 0x%x", ctx.dwords[1]);
      ctx.total_size = 2;
      break;
   default: break;
   }

   print_operand(ctx, bfe(ctx, 9, 8) | vgpr, 1);

   switch ((aco_opcode)ctx.op) {
   case aco_opcode::v_cndmask_b16:
   case aco_opcode::v_cndmask_b32:
   case aco_opcode::v_addc_co_u32:
   case aco_opcode::v_subb_co_u32:
   case aco_opcode::v_subbrev_co_u32: print_operand(ctx, vcc.reg(), 2); break;
   case aco_opcode::v_madak_f16:
   case aco_opcode::v_fmaak_f16:
   case aco_opcode::v_madak_f32:
   case aco_opcode::v_fmaak_f32:
      fprintf(ctx.disasm->output, ", 0x%x", ctx.dwords[1]);
      ctx.total_size = 2;
      break;
   default: break;
   }

   if (ctx.has_sdwa) {
      print_flag(ctx, " clamp", 45);
      print_omod(ctx, bfe(ctx, 46, 2));
      print_sdwa_sel(ctx, "dst_sel", bfe(ctx, 40, 3));
      print_sdwa_unused(ctx, bfe(ctx, 43, 2));
      print_sdwa_sel(ctx, "src0_sel", bfe(ctx, 48, 3));
      print_sdwa_sel(ctx, "src1_sel", bfe(ctx, 56, 3));
   }

   print_dpp(ctx);
}

static void
print_attr(instr_context& ctx, uint32_t attr, uint32_t channel)
{
   fprintf(ctx.disasm->output, ", attr%u", attr);
   switch (channel) {
   case 0: fprintf(ctx.disasm->output, ".x"); break;
   case 1: fprintf(ctx.disasm->output, ".y"); break;
   case 2: fprintf(ctx.disasm->output, ".z"); break;
   case 3: fprintf(ctx.disasm->output, ".w"); break;
   default: break;
   }
}

void
disasm_vop3(instr_context& ctx)
{
   if (ctx.disasm->program->gfx_level >= GFX11) {
      ctx.has_dpp8 = bfe(ctx, 32, 9) == 233;
      ctx.has_dpp8_fi = bfe(ctx, 32, 9) == 234;
      ctx.has_dpp16 = bfe(ctx, 32, 9) == 250;
   }

   uint16_t opcode = ctx.disasm->vop3_opcodes.at(
      ctx.disasm->program->gfx_level > GFX7 ? bfe(ctx, 16, 10) : bfe(ctx, 17, 9));
   Format format = instr_info.format[opcode];
   opcode = ctx.disasm->opcode_encodings[opcode];
   print_opcode(ctx, format, opcode);

   ctx.encoded_format = Format::VOP3;

   /* VOP3B */
   bool has_sdst = false;
   switch ((aco_opcode)ctx.op) {
   case aco_opcode::v_add_co_u32:
   case aco_opcode::v_sub_co_u32:
   case aco_opcode::v_subrev_co_u32:
   case aco_opcode::v_add_co_u32_e64:
   case aco_opcode::v_sub_co_u32_e64:
   case aco_opcode::v_subrev_co_u32_e64:
   case aco_opcode::v_addc_co_u32:
   case aco_opcode::v_subb_co_u32:
   case aco_opcode::v_subbrev_co_u32:
   case aco_opcode::v_div_scale_f32:
   case aco_opcode::v_div_scale_f64:
   case aco_opcode::v_mad_u64_u32:
   case aco_opcode::v_mad_i64_i32: has_sdst = true; break;
   default: break;
   }

   bool force_sdst = format == Format::VOPC;
   switch ((aco_opcode)ctx.op) {
   case aco_opcode::v_readlane_b32:
   case aco_opcode::v_readlane_b32_e64: force_sdst = true; break;
   default: break;
   }

   bool cmpx = format == Format::VOPC && is_cmpx((aco_opcode)ctx.op) &&
               ctx.disasm->program->gfx_level > GFX9;
   if (cmpx) {
      fprintf(ctx.disasm->output, " ");
   } else {
      if (force_sdst)
         print_definition(ctx, bfe(ctx, 0, 8));
      else
         print_definition(ctx, bfe(ctx, 0, 8) | vgpr);
   }

   bool is_vinterp = false;
   switch ((aco_opcode)ctx.op) {
   case aco_opcode::v_interp_p1ll_f16:
   case aco_opcode::v_interp_p1lv_f16:
   case aco_opcode::v_interp_p2_legacy_f16:
   case aco_opcode::v_interp_p2_f16:
   case aco_opcode::v_interp_p2_hi_f16: is_vinterp = true; break;
   default: break;
   }

   if (has_sdst) {
      print_operand(ctx, bfe(ctx, 8, 7), operand_index_def | 1,
                    additional_operand_info{
                       .count = ctx.disasm->program->wave_size / 32,
                    });
      print_operand(ctx, bfe(ctx, 32, 9), 0, additional_operand_info{.neg = !!bfe(ctx, 61, 1)});
      print_operand(ctx, bfe(ctx, 41, 9), 1, additional_operand_info{.neg = !!bfe(ctx, 62, 1)});
      print_operand(ctx, bfe(ctx, 50, 9), 2, additional_operand_info{.neg = !!bfe(ctx, 63, 1)});
   } else {
      Instruction instr = {
         .opcode = (aco_opcode)ctx.op,
         .format = ctx.format,
      };

      bool has_opsel = false;
      if (get_definition_info(ctx.disasm->program->gfx_level, &instr, 0).bytes() == 2)
         has_opsel = true;

      for (uint32_t i = 0; i < 3; i++) {
         if (get_operand_info(ctx.disasm->program->gfx_level, &instr, i).bytes() == 2)
            has_opsel = true;
      }

      if ((aco_opcode)ctx.op == aco_opcode::v_ashrrev_i16_e64 ||
          (aco_opcode)ctx.op == aco_opcode::v_lshlrev_b16_e64)
         has_opsel = false;

      std::vector<uint32_t> opsel;

      /* vinterp instructions use SRC0 to specify the attribute. */
      if (print_operand(ctx, bfe(ctx, is_vinterp ? 41 : 32, 9), 0,
                        additional_operand_info{
                           .skip_comma = cmpx,
                           .neg = !!bfe(ctx, 61, 1),
                           .abs = !!bfe(ctx, 8, 1),
                        })) {
         if (has_opsel)
            opsel.push_back(bfe(ctx, 11, 1));
      }

      if (is_vinterp)
         print_attr(ctx, bfe(ctx, 32, 6), bfe(ctx, 38, 2));

      if (print_operand(ctx, bfe(ctx, 41, 9), 1,
                        additional_operand_info{
                           .neg = !!bfe(ctx, 62, 1),
                           .abs = !!bfe(ctx, 9, 1),
                        })) {
         if (has_opsel)
            opsel.push_back(bfe(ctx, 12, 1));
      }

      if ((aco_opcode)ctx.op != aco_opcode::v_writelane_b32_e64) {
         if (print_operand(ctx, bfe(ctx, 50, 9), 2,
                           additional_operand_info{
                              .neg = !!bfe(ctx, 63, 1),
                              .abs = !!bfe(ctx, 10, 1),
                           })) {
            if (has_opsel)
               opsel.push_back(bfe(ctx, 13, 1));
         }
      }

      if (is_vinterp)
         print_flag(ctx, " high", 40);

      if (has_opsel)
         opsel.push_back(bfe(ctx, 14, 1));

      switch ((aco_opcode)ctx.op) {
      case aco_opcode::v_permlane16_b32:
      case aco_opcode::v_permlanex16_b32:
         opsel.push_back(bfe(ctx, 11, 1));
         opsel.push_back(bfe(ctx, 12, 1));
         break;
      default: break;
      }

      print_integer_array(ctx, "op_sel", opsel.data(), opsel.size(), 0);
   }

   print_flag(ctx, " clamp", ctx.disasm->program->gfx_level > GFX7 ? 15 : 11);

   print_omod(ctx, bfe(ctx, 59, 2));

   print_dpp(ctx);
}

void
disasm_vop3p(instr_context& ctx)
{
   if (ctx.disasm->program->gfx_level >= GFX11) {
      ctx.has_dpp8 = bfe(ctx, 32, 9) == 233;
      ctx.has_dpp8_fi = bfe(ctx, 32, 9) == 234;
      ctx.has_dpp16 = bfe(ctx, 32, 9) == 250;
   }

   print_opcode(ctx, Format::VOP3P, bfe(ctx, 16, 7));

   bool has_opsel;
   bool fma_mix = false;
   switch ((aco_opcode)ctx.op) {
   case aco_opcode::v_fma_mix_f32:
   case aco_opcode::v_fma_mixlo_f16:
   case aco_opcode::v_fma_mixhi_f16: fma_mix = true; FALLTHROUGH;
   case aco_opcode::v_dot4_u32_u8:
   case aco_opcode::v_dot4_i32_i8:
   case aco_opcode::v_dot8_u32_u4:
   case aco_opcode::v_dot8_i32_i4: has_opsel = false; break;
   default: has_opsel = true; break;
   }

   print_definition(ctx, bfe(ctx, 0, 8) | vgpr);

   uint32_t opsel[3] = {bfe(ctx, 11, 1), bfe(ctx, 12, 1), bfe(ctx, 13, 1)};
   uint32_t opsel_hi[3] = {bfe(ctx, 59, 1), bfe(ctx, 60, 1), bfe(ctx, 14, 1)};
   uint32_t neg[3] = {bfe(ctx, 61, 1), bfe(ctx, 62, 1), bfe(ctx, 63, 1)};
   uint32_t neg_hi[3] = {bfe(ctx, 8, 1), bfe(ctx, 9, 1), bfe(ctx, 10, 1)};

   additional_operand_info operand_infos[3] = {};
   if (fma_mix) {
      for (uint32_t i = 0; i < 3; i++) {
         /* LLVM BUG: if (opsel_hi[2]) */
         operand_infos[i].opsel =
            operand_opsel_lo; /* opsel[i] ? operand_opsel_hi : operand_opsel_lo; */
         if (neg[i])
            operand_infos[i].neg = true;
         if (neg_hi[i])
            operand_infos[i].abs = true;
      }
   }

   uint32_t operand_count = 0;
   operand_count += !!print_operand(ctx, bfe(ctx, 32, 9), 0, operand_infos[0]);
   operand_count += !!print_operand(ctx, bfe(ctx, 41, 9), 1, operand_infos[1]);
   operand_count += !!print_operand(ctx, bfe(ctx, 50, 9), 2, operand_infos[2]);

   if (has_opsel || fma_mix) {
      print_integer_array(ctx, "op_sel", opsel, operand_count, 0);
      print_integer_array(ctx, "op_sel_hi", opsel_hi, operand_count, fma_mix ? 0 : 1);

      if (has_opsel) {
         print_integer_array(ctx, "neg_lo", neg, operand_count, 0);
         print_integer_array(ctx, "neg_hi", neg_hi, operand_count, 0);
      }
   }

   print_flag(ctx, " clamp", 15);

   print_dpp(ctx);
}

void
disasm_vopc(instr_context& ctx)
{
   ctx.has_sdwa = bfe(ctx, 0, 9) == 249;
   if (ctx.disasm->program->gfx_level >= GFX11) {
      ctx.has_dpp8 = bfe(ctx, 32, 9) == 233;
      ctx.has_dpp8_fi = bfe(ctx, 32, 9) == 234;
      ctx.has_dpp16 = bfe(ctx, 32, 9) == 250;
   }

   print_opcode(ctx, Format::VOPC, bfe(ctx, 17, 8));

   bool has_vcc = !is_cmpx((aco_opcode)ctx.op) || ctx.disasm->program->gfx_level <= GFX9;
   if (has_vcc)
      print_definition(ctx, vcc);
   else
      fprintf(ctx.disasm->output, " ");

   print_operand(ctx, bfe(ctx, 0, 9), 0, additional_operand_info{.skip_comma = !has_vcc});
   print_operand(ctx, bfe(ctx, 9, 8) | vgpr, 1);

   if (ctx.has_sdwa) {
      print_sdwa_sel(ctx, "src0_sel", bfe(ctx, 48, 3));
      print_sdwa_sel(ctx, "src1_sel", bfe(ctx, 56, 3));
   }

   print_dpp(ctx);
}

void
disasm_vopd(instr_context& ctx)
{
   print_opcode(ctx, Format::VOPD, bfe(ctx, 22, 4));
   print_definition(ctx, bfe(ctx, 56, 8) | vgpr);
   print_operand(ctx, bfe(ctx, 0, 9), 0);

   if ((aco_opcode)ctx.op == aco_opcode::v_dual_fmamk_f32) {
      fprintf(ctx.disasm->output, ", 0x%x", ctx.dwords[2]);
      ctx.total_size = 3;
      ctx.has_literal = true;
   }

   print_operand(ctx, bfe(ctx, 9, 8) | vgpr, 1);

   if ((aco_opcode)ctx.op == aco_opcode::v_dual_fmaak_f32) {
      fprintf(ctx.disasm->output, ", 0x%x", ctx.dwords[2]);
      ctx.total_size = 3;
      ctx.has_literal = true;
   }

   fprintf(ctx.disasm->output, " :: ");

   print_opcode(ctx, Format::VOPD, bfe(ctx, 17, 5));
   print_definition(ctx, (bfe(ctx, 49, 7) << 1) | (bfe(ctx, 56, 1) ? 0 : 1) | vgpr);
   print_operand(ctx, bfe(ctx, 32, 9), 0);

   if ((aco_opcode)ctx.op == aco_opcode::v_dual_fmamk_f32) {
      fprintf(ctx.disasm->output, ", 0x%x", ctx.dwords[2]);
      ctx.total_size = 3;
      ctx.has_literal = true;
   }

   print_operand(ctx, bfe(ctx, 41, 8) | vgpr, 1);

   if ((aco_opcode)ctx.op == aco_opcode::v_dual_fmaak_f32) {
      fprintf(ctx.disasm->output, ", 0x%x", ctx.dwords[2]);
      ctx.total_size = 3;
      ctx.has_literal = true;
   }
}

const char* data_formats[] = {
   "BUF_DATA_FORMAT_INVALID",     "BUF_DATA_FORMAT_8",        "BUF_DATA_FORMAT_16",
   "BUF_DATA_FORMAT_8_8",         "BUF_DATA_FORMAT_32",       "BUF_DATA_FORMAT_16_16",
   "BUF_DATA_FORMAT_10_11_11",    "BUF_DATA_FORMAT_11_11_10", "BUF_DATA_FORMAT_10_10_10_2",
   "BUF_DATA_FORMAT_2_10_10_10",  "BUF_DATA_FORMAT_8_8_8_8",  "BUF_DATA_FORMAT_32_32",
   "BUF_DATA_FORMAT_16_16_16_16", "BUF_DATA_FORMAT_32_32_32", "BUF_DATA_FORMAT_32_32_32_32",
   "BUF_DATA_FORMAT_RESERVED_15",
};

const char* number_formats_gfx9[] = {
   "BUF_NUM_FORMAT_UNORM",      "BUF_NUM_FORMAT_SNORM", "BUF_NUM_FORMAT_USCALED",
   "BUF_NUM_FORMAT_SSCALED",    "BUF_NUM_FORMAT_UINT",  "BUF_NUM_FORMAT_SINT",
   "BUF_NUM_FORMAT_RESERVED_6", "BUF_NUM_FORMAT_FLOAT",
};

static const char* formats_gfx10[] = {
   "BUF_FMT_INVALID",

   "BUF_FMT_8_UNORM",
   "BUF_FMT_8_SNORM",
   "BUF_FMT_8_USCALED",
   "BUF_FMT_8_SSCALED",
   "BUF_FMT_8_UINT",
   "BUF_FMT_8_SINT",

   "BUF_FMT_16_UNORM",
   "BUF_FMT_16_SNORM",
   "BUF_FMT_16_USCALED",
   "BUF_FMT_16_SSCALED",
   "BUF_FMT_16_UINT",
   "BUF_FMT_16_SINT",
   "BUF_FMT_16_FLOAT",

   "BUF_FMT_8_8_UNORM",
   "BUF_FMT_8_8_SNORM",
   "BUF_FMT_8_8_USCALED",
   "BUF_FMT_8_8_SSCALED",
   "BUF_FMT_8_8_UINT",
   "BUF_FMT_8_8_SINT",

   "BUF_FMT_32_UINT",
   "BUF_FMT_32_SINT",
   "BUF_FMT_32_FLOAT",

   "BUF_FMT_16_16_UNORM",
   "BUF_FMT_16_16_SNORM",
   "BUF_FMT_16_16_USCALED",
   "BUF_FMT_16_16_SSCALED",
   "BUF_FMT_16_16_UINT",
   "BUF_FMT_16_16_SINT",
   "BUF_FMT_16_16_FLOAT",

   "BUF_FMT_10_11_11_UNORM",
   "BUF_FMT_10_11_11_SNORM",
   "BUF_FMT_10_11_11_USCALED",
   "BUF_FMT_10_11_11_SSCALED",
   "BUF_FMT_10_11_11_UINT",
   "BUF_FMT_10_11_11_SINT",
   "BUF_FMT_10_11_11_FLOAT",

   "BUF_FMT_11_11_10_UNORM",
   "BUF_FMT_11_11_10_SNORM",
   "BUF_FMT_11_11_10_USCALED",
   "BUF_FMT_11_11_10_SSCALED",
   "BUF_FMT_11_11_10_UINT",
   "BUF_FMT_11_11_10_SINT",
   "BUF_FMT_11_11_10_FLOAT",

   "BUF_FMT_10_10_10_2_UNORM",
   "BUF_FMT_10_10_10_2_SNORM",
   "BUF_FMT_10_10_10_2_USCALED",
   "BUF_FMT_10_10_10_2_SSCALED",
   "BUF_FMT_10_10_10_2_UINT",
   "BUF_FMT_10_10_10_2_SINT",

   "BUF_FMT_2_10_10_10_UNORM",
   "BUF_FMT_2_10_10_10_SNORM",
   "BUF_FMT_2_10_10_10_USCALED",
   "BUF_FMT_2_10_10_10_SSCALED",
   "BUF_FMT_2_10_10_10_UINT",
   "BUF_FMT_2_10_10_10_SINT",

   "BUF_FMT_8_8_8_8_UNORM",
   "BUF_FMT_8_8_8_8_SNORM",
   "BUF_FMT_8_8_8_8_USCALED",
   "BUF_FMT_8_8_8_8_SSCALED",
   "BUF_FMT_8_8_8_8_UINT",
   "BUF_FMT_8_8_8_8_SINT",

   "BUF_FMT_32_32_UINT",
   "BUF_FMT_32_32_SINT",
   "BUF_FMT_32_32_FLOAT",

   "BUF_FMT_16_16_16_16_UNORM",
   "BUF_FMT_16_16_16_16_SNORM",
   "BUF_FMT_16_16_16_16_USCALED",
   "BUF_FMT_16_16_16_16_SSCALED",
   "BUF_FMT_16_16_16_16_UINT",
   "BUF_FMT_16_16_16_16_SINT",
   "BUF_FMT_16_16_16_16_FLOAT",

   "BUF_FMT_32_32_32_UINT",
   "BUF_FMT_32_32_32_SINT",
   "BUF_FMT_32_32_32_FLOAT",
   "BUF_FMT_32_32_32_32_UINT",
   "BUF_FMT_32_32_32_32_SINT",
   "BUF_FMT_32_32_32_32_FLOAT",
};

static const char* formats_gfx11[] = {
   "BUF_FMT_INVALID",

   "BUF_FMT_8_UNORM",
   "BUF_FMT_8_SNORM",
   "BUF_FMT_8_USCALED",
   "BUF_FMT_8_SSCALED",
   "BUF_FMT_8_UINT",
   "BUF_FMT_8_SINT",

   "BUF_FMT_16_UNORM",
   "BUF_FMT_16_SNORM",
   "BUF_FMT_16_USCALED",
   "BUF_FMT_16_SSCALED",
   "BUF_FMT_16_UINT",
   "BUF_FMT_16_SINT",
   "BUF_FMT_16_FLOAT",

   "BUF_FMT_8_8_UNORM",
   "BUF_FMT_8_8_SNORM",
   "BUF_FMT_8_8_USCALED",
   "BUF_FMT_8_8_SSCALED",
   "BUF_FMT_8_8_UINT",
   "BUF_FMT_8_8_SINT",

   "BUF_FMT_32_UINT",
   "BUF_FMT_32_SINT",
   "BUF_FMT_32_FLOAT",

   "BUF_FMT_16_16_UNORM",
   "BUF_FMT_16_16_SNORM",
   "BUF_FMT_16_16_USCALED",
   "BUF_FMT_16_16_SSCALED",
   "BUF_FMT_16_16_UINT",
   "BUF_FMT_16_16_SINT",
   "BUF_FMT_16_16_FLOAT",

   "BUF_FMT_10_11_11_FLOAT",

   "BUF_FMT_11_11_10_FLOAT",

   "BUF_FMT_10_10_10_2_UNORM",
   "BUF_FMT_10_10_10_2_SNORM",
   "BUF_FMT_10_10_10_2_UINT",
   "BUF_FMT_10_10_10_2_SINT",

   "BUF_FMT_2_10_10_10_UNORM",
   "BUF_FMT_2_10_10_10_SNORM",
   "BUF_FMT_2_10_10_10_USCALED",
   "BUF_FMT_2_10_10_10_SSCALED",
   "BUF_FMT_2_10_10_10_UINT",
   "BUF_FMT_2_10_10_10_SINT",

   "BUF_FMT_8_8_8_8_UNORM",
   "BUF_FMT_8_8_8_8_SNORM",
   "BUF_FMT_8_8_8_8_USCALED",
   "BUF_FMT_8_8_8_8_SSCALED",
   "BUF_FMT_8_8_8_8_UINT",
   "BUF_FMT_8_8_8_8_SINT",

   "BUF_FMT_32_32_UINT",
   "BUF_FMT_32_32_SINT",
   "BUF_FMT_32_32_FLOAT",

   "BUF_FMT_16_16_16_16_UNORM",
   "BUF_FMT_16_16_16_16_SNORM",
   "BUF_FMT_16_16_16_16_USCALED",
   "BUF_FMT_16_16_16_16_SSCALED",
   "BUF_FMT_16_16_16_16_UINT",
   "BUF_FMT_16_16_16_16_SINT",
   "BUF_FMT_16_16_16_16_FLOAT",

   "BUF_FMT_32_32_32_UINT",
   "BUF_FMT_32_32_32_SINT",
   "BUF_FMT_32_32_32_FLOAT",
   "BUF_FMT_32_32_32_32_UINT",
   "BUF_FMT_32_32_32_32_SINT",
   "BUF_FMT_32_32_32_32_FLOAT",
};

void
disasm_mtbuf(instr_context& ctx)
{
   print_opcode(ctx, Format::MTBUF, bfe(ctx, 15, 4));

   print_operand(ctx, bfe(ctx, 40, 8) | vgpr, ctx.has_def ? operand_index_def : 3,
                 additional_operand_info{.tfe = !!bfe(ctx, 55, 1)});

   if (bfe(ctx, 12, 2)) {
      print_operand(ctx, bfe(ctx, 32, 8) | vgpr, 1,
                    additional_operand_info{.count = util_bitcount(bfe(ctx, 12, 2))});
   } else {
      fprintf(ctx.disasm->output, ", off");
   }

   print_operand(ctx, bfe(ctx, 48, 5) << 2, 0, additional_operand_info{.count = 4});
   print_operand(ctx, bfe(ctx, 56, 8), 2);

   uint32_t dfmt = bfe(ctx, 19, 4);
   uint32_t nfmt = bfe(ctx, 23, 3);

   if (dfmt != 1 || nfmt) {
      fprintf(ctx.disasm->output, " format:[");
      if (dfmt != 1)
         fprintf(ctx.disasm->output, "%s", data_formats[dfmt]);
      if (nfmt) {
         if (dfmt != 1)
            fprintf(ctx.disasm->output, ",");
         fprintf(ctx.disasm->output, "%s", number_formats_gfx9[nfmt]);
      }
      fprintf(ctx.disasm->output, "]");
   }

   print_flag(ctx, " idxen", 13);
   print_flag(ctx, " offen", 12);

   if (bfe(ctx, 0, 12))
      fprintf(ctx.disasm->output, " offset:%u", bfe(ctx, 0, 12));

   print_flag(ctx, " glc", 14);
   print_flag(ctx, " slc", 54);
   print_flag(ctx, " tfe", 55);
}

void
disasm_mtbuf_gfx10(instr_context& ctx)
{
   print_opcode(ctx, Format::MTBUF, bfe(ctx, 16, 3) | (bfe(ctx, 53, 1) << 3));

   print_operand(ctx, bfe(ctx, 40, 8) | vgpr, ctx.has_def ? operand_index_def : 3,
                 additional_operand_info{.tfe = !!bfe(ctx, 55, 1)});

   if (bfe(ctx, 12, 2)) {
      print_operand(ctx, bfe(ctx, 32, 8) | vgpr, 1,
                    additional_operand_info{.count = util_bitcount(bfe(ctx, 12, 2))});
   } else {
      fprintf(ctx.disasm->output, ", off");
   }

   print_operand(ctx, bfe(ctx, 48, 5) << 2, 0, additional_operand_info{.count = 4});
   print_operand(ctx, bfe(ctx, 56, 8), 2);

   if (bfe(ctx, 19, 7) != 1)
      fprintf(ctx.disasm->output, " format:[%s]", formats_gfx10[bfe(ctx, 19, 7)]);

   print_flag(ctx, " idxen", 13);
   print_flag(ctx, " offen", 12);

   if (bfe(ctx, 0, 12))
      fprintf(ctx.disasm->output, " offset:%u", bfe(ctx, 0, 12));

   print_flag(ctx, " glc", 14);
   print_flag(ctx, " dlc", 15);
   print_flag(ctx, " slc", 54);
   print_flag(ctx, " tfe", 55);
}

void
disasm_mtbuf_gfx11(instr_context& ctx)
{
   print_opcode(ctx, Format::MTBUF, bfe(ctx, 15, 4));

   print_operand(ctx, bfe(ctx, 40, 8) | vgpr, ctx.has_def ? operand_index_def : 3,
                 additional_operand_info{.tfe = !!bfe(ctx, 53, 1)});

   if (bfe(ctx, 54, 2)) {
      print_operand(ctx, bfe(ctx, 32, 8) | vgpr, 1,
                    additional_operand_info{.count = util_bitcount(bfe(ctx, 54, 2))});
   } else {
      fprintf(ctx.disasm->output, ", off");
   }

   print_operand(ctx, bfe(ctx, 48, 5) << 2, 0, additional_operand_info{.count = 4});
   print_operand(ctx, bfe(ctx, 56, 8), 2);

   if (bfe(ctx, 19, 7) != 1)
      fprintf(ctx.disasm->output, " format:[%s]", formats_gfx11[bfe(ctx, 19, 7)]);

   print_flag(ctx, " idxen", 55);
   print_flag(ctx, " offen", 54);

   if (bfe(ctx, 0, 12))
      fprintf(ctx.disasm->output, " offset:%u", bfe(ctx, 0, 12));

   print_flag(ctx, " glc", 14);
   print_flag(ctx, " dlc", 13);
   print_flag(ctx, " slc", 12);
   print_flag(ctx, " tfe", 53);
}

void
disasm_mubuf(instr_context& ctx)
{
   print_opcode(ctx, Format::MUBUF,
                ctx.disasm->program->gfx_level >= GFX10 ? bfe(ctx, 18, 8) : bfe(ctx, 18, 7));

   print_operand(ctx, bfe(ctx, 40, 8) | vgpr, ctx.has_def ? operand_index_def : 3,
                 additional_operand_info{.tfe = !!bfe(ctx, 55, 1)});

   if (bfe(ctx, 12, 2)) {
      print_operand(ctx, bfe(ctx, 32, 8) | vgpr, 1,
                    additional_operand_info{.count = util_bitcount(bfe(ctx, 12, 2))});
   } else {
      fprintf(ctx.disasm->output, ", off");
   }

   print_operand(ctx, bfe(ctx, 48, 5) << 2, 0, additional_operand_info{.count = 4});

   print_operand(ctx, bfe(ctx, 56, 8), 2);

   print_flag(ctx, " idxen", 13);
   print_flag(ctx, " offen", 12);

   if (bfe(ctx, 0, 12))
      fprintf(ctx.disasm->output, " offset:%u", bfe(ctx, 0, 12));

   print_flag(ctx, " glc", 14);

   if (ctx.disasm->program->gfx_level > GFX9)
      print_flag(ctx, " dlc", 15);

   if (ctx.disasm->program->gfx_level <= GFX7)
      print_flag(ctx, " addr64", 15);

   print_flag(ctx, " slc", ctx.disasm->program->gfx_level > GFX9 ? 54 : 17);
   print_flag(ctx, " lds", 16);
   print_flag(ctx, " tfe", 55);
}

void
disasm_mubuf_gfx11(instr_context& ctx)
{
   print_opcode(ctx, Format::MUBUF, bfe(ctx, 18, 8));

   print_operand(ctx, bfe(ctx, 40, 8) | vgpr, ctx.has_def ? operand_index_def : 3,
                 additional_operand_info{.tfe = !!bfe(ctx, 53, 1)});

   if (bfe(ctx, 54, 2)) {
      print_operand(ctx, bfe(ctx, 32, 8) | vgpr, 1,
                    additional_operand_info{.count = util_bitcount(bfe(ctx, 54, 2))});
   } else {
      if (ctx.printed_operand)
         fprintf(ctx.disasm->output, ",");
      fprintf(ctx.disasm->output, " off");
      ctx.printed_operand = true;
   }

   print_operand(ctx, bfe(ctx, 48, 5) << 2, 0, additional_operand_info{.count = 4});

   print_operand(ctx, bfe(ctx, 56, 8), 2);

   print_flag(ctx, " idxen", 55);
   print_flag(ctx, " offen", 54);

   if (bfe(ctx, 0, 12))
      fprintf(ctx.disasm->output, " offset:%u", bfe(ctx, 0, 12));

   print_flag(ctx, " glc", 14);
   print_flag(ctx, " dlc", 13);
   print_flag(ctx, " slc", 12);
   print_flag(ctx, " lds", 16);
   print_flag(ctx, " tfe", 53);
}

void
print_mimg_dim(instr_context& ctx, ac_image_dim dim)
{
   switch (dim) {
   case ac_image_1d: fprintf(ctx.disasm->output, " dim:SQ_RSRC_IMG_1D"); break;
   case ac_image_2d: fprintf(ctx.disasm->output, " dim:SQ_RSRC_IMG_2D"); break;
   case ac_image_3d: fprintf(ctx.disasm->output, " dim:SQ_RSRC_IMG_3D"); break;
   case ac_image_cube: fprintf(ctx.disasm->output, " dim:SQ_RSRC_IMG_CUBE"); break;
   case ac_image_1darray: fprintf(ctx.disasm->output, " dim:SQ_RSRC_IMG_1D_ARRAY"); break;
   case ac_image_2darray: fprintf(ctx.disasm->output, " dim:SQ_RSRC_IMG_2D_ARRAY"); break;
   case ac_image_2dmsaa: fprintf(ctx.disasm->output, " dim:SQ_RSRC_IMG_2D_MSAA"); break;
   case ac_image_2darraymsaa: fprintf(ctx.disasm->output, " dim:SQ_RSRC_IMG_2D_MSAA_ARRAY"); break;
   }
}

uint32_t
get_mimg_coord_components(instr_context& ctx, aco_mimg_op_info info, ac_image_dim dim, bool a16)
{
   aco_mimg_op_info mimg_op = aco_mimg_op_info_get_op(info);

   if (a16) {
      if (mimg_op == aco_mimg_op_info::bvh)
         return 8;
      if (mimg_op == aco_mimg_op_info::bvh64)
         return 9;
   } else {
      if (mimg_op == aco_mimg_op_info::bvh)
         return 11;
      if (mimg_op == aco_mimg_op_info::bvh64)
         return 12;
   }

   uint32_t comps = 0;
   switch (dim) {
   case ac_image_1d: comps = 1; break;
   case ac_image_2d: comps = 2; break;
   case ac_image_3d: comps = 3; break;
   case ac_image_cube: comps = 3; break;
   case ac_image_1darray: comps = 2; break;
   case ac_image_2darray: comps = 3; break;
   case ac_image_2dmsaa: comps = 3; break;
   case ac_image_2darraymsaa: comps = 4; break;
   }

   if (info & aco_mimg_op_info::flag_lod)
      comps++;

   if (info & aco_mimg_op_info::flag_lod_bias)
      comps++;

   if (info & aco_mimg_op_info::flag_lod_clamp)
      comps++;

   if (info & aco_mimg_op_info::flag_derivative) {
      uint32_t derivative_components = 0;
      switch (dim) {
      case ac_image_1d: derivative_components = 2; break;
      case ac_image_2d: derivative_components = 4; break;
      case ac_image_3d: derivative_components = 6; break;
      case ac_image_cube: derivative_components = 6; break;
      case ac_image_1darray: derivative_components = 2; break;
      case ac_image_2darray: derivative_components = 4; break;
      case ac_image_2dmsaa: derivative_components = 4; break;
      case ac_image_2darraymsaa: derivative_components = 4; break;
      }

      if ((info & aco_mimg_op_info::flag_g16) || ctx.disasm->program->gfx_level <= GFX9)
         derivative_components /= 2;

      if (a16)
         derivative_components *= 2;

      comps += derivative_components;
   }

   if (info & aco_mimg_op_info::flag_compare)
      comps += a16 ? 2 : 1;

   if (info & aco_mimg_op_info::flag_offset)
      comps += a16 ? 2 : 1;

   return DIV_ROUND_UP(comps, a16 ? 2 : 1);
}

#define require_eq(ctx, field, value, expected)                                                    \
   if (value != expected)                                                                          \
   fprintf(ctx.disasm->output, " (invalid " #field " value %u, expected %u)", value, expected)

void
disasm_mimg(instr_context& ctx)
{
   print_opcode(ctx, Format::MIMG, bfe(ctx, 18, 7) | (bfe(ctx, 0, 1) << 7));

   aco_mimg_op_info info = (aco_mimg_op_info)instr_info.mimg_infos.at((aco_opcode)ctx.op);
   aco_mimg_op_info mimg_op = aco_mimg_op_info_get_op(info);

   uint32_t dmask = bfe(ctx, 8, 4);
   uint32_t data_components;
   switch (mimg_op) {
   case aco_mimg_op_info::msaa_load:
   case aco_mimg_op_info::gather4: data_components = 4; break;
   case aco_mimg_op_info::atomic: data_components = 1; break;
   default: data_components = util_bitcount(dmask); break;
   }
   if (bfe(ctx, 16, 1))
      data_components++;

   bool d16 = !!bfe(ctx, 63, 1);
   print_operand(ctx, bfe(ctx, 40, 8) | vgpr, 2,
                 additional_operand_info{.count = DIV_ROUND_UP(data_components, d16 ? 2 : 1)});

   uint32_t coord_components = get_mimg_coord_components(ctx, info, ac_image_1d, !!bfe(ctx, 62, 1));
   print_operand(ctx, bfe(ctx, 32, 8) | vgpr, 0,
                 additional_operand_info{.count = coord_components});

   print_operand(ctx, bfe(ctx, 48, 5) << 2, 0,
                 additional_operand_info{.count = /* bfe(ctx, 15, 1) ? 4u : 8u */ 8});

   if (mimg_op == aco_mimg_op_info::get_lod || mimg_op == aco_mimg_op_info::sample ||
       mimg_op == aco_mimg_op_info::gather4)
      print_operand(ctx, bfe(ctx, 53, 5) << 2, 1, additional_operand_info{.count = 4});

   fprintf(ctx.disasm->output, " dmask:0x%x", dmask);

   print_flag(ctx, " lwe", 17);
   print_flag(ctx, " unorm", 12);
   print_flag(ctx, " glc", 13);
   print_flag(ctx, " slc", 25);
   print_flag(ctx, " a16", 15);
   print_flag(ctx, " da", 14);
   print_flag(ctx, " d16", 63);
   print_flag(ctx, " tfe", 16);
}

void
disasm_mimg_gfx10(instr_context& ctx)
{
   print_opcode(ctx, Format::MIMG, bfe(ctx, 18, 7) | (bfe(ctx, 0, 1) << 7));

   aco_mimg_op_info info = (aco_mimg_op_info)instr_info.mimg_infos.at((aco_opcode)ctx.op);
   aco_mimg_op_info mimg_op = aco_mimg_op_info_get_op(info);

   uint32_t dmask = bfe(ctx, 8, 4);
   uint32_t data_components;
   switch (mimg_op) {
   case aco_mimg_op_info::msaa_load:
   case aco_mimg_op_info::gather4: data_components = 4; break;
   case aco_mimg_op_info::atomic: data_components = 1; break;
   default: data_components = util_bitcount(dmask); break;
   }
   if (bfe(ctx, 16, 1))
      data_components++;

   bool d16 = !!bfe(ctx, 63, 1);
   print_operand(ctx, bfe(ctx, 40, 8) | vgpr, 2,
                 additional_operand_info{.count = DIV_ROUND_UP(data_components, d16 ? 2 : 1)});

   uint32_t nsa = bfe(ctx, 1, 2);
   uint32_t coord_components =
      get_mimg_coord_components(ctx, info, (ac_image_dim)bfe(ctx, 3, 3), !!bfe(ctx, 62, 1));
   if (nsa > 0) {
      fprintf(ctx.disasm->output, ", [");
      print_operand(ctx, bfe(ctx, 32, 8) | vgpr, 0, additional_operand_info{.skip_comma = true});
      for (uint32_t i = 0; i < MIN2(nsa * 4, coord_components - 1); i++) {
         print_operand(ctx, bfe(ctx, 64 + i * 8, 8) | vgpr, 0);
      }
      fprintf(ctx.disasm->output, "]");
   } else {
      print_operand(ctx, bfe(ctx, 32, 8) | vgpr, 0,
                    additional_operand_info{.count = coord_components});
   }
   ctx.total_size += nsa;

   print_operand(ctx, bfe(ctx, 48, 5) << 2, 0,
                 additional_operand_info{.count = bfe(ctx, 15, 1) ? 4u : 8u});

   if (mimg_op == aco_mimg_op_info::get_lod || mimg_op == aco_mimg_op_info::sample ||
       mimg_op == aco_mimg_op_info::gather4)
      print_operand(ctx, bfe(ctx, 53, 5) << 2, 1, additional_operand_info{.count = 4});

   bool is_bvh = mimg_op == aco_mimg_op_info::bvh || mimg_op == aco_mimg_op_info::bvh64;

   if (!is_bvh) {
      fprintf(ctx.disasm->output, " dmask:0x%x", dmask);
      print_mimg_dim(ctx, (ac_image_dim)bfe(ctx, 3, 3));

      print_flag(ctx, " lwe", 17);
      print_flag(ctx, " unorm", 12);
   }

   print_flag(ctx, " dlc", 7);
   print_flag(ctx, " glc", 13);
   print_flag(ctx, " slc", 25);
   print_flag(ctx, " a16", 62);

   if (!is_bvh) {
      print_flag(ctx, " d16", 63);
      print_flag(ctx, " tfe", 16);
   }

   if (is_bvh) {
      require_eq(ctx, dmask, dmask, 0xf);
      require_eq(ctx, d16, bfe(ctx, 63, 1), 0);
      require_eq(ctx, r128, bfe(ctx, 15, 1), 1);
      require_eq(ctx, unorm, bfe(ctx, 12, 1), 1);
      require_eq(ctx, dim, bfe(ctx, 3, 3), 0);
      require_eq(ctx, lwe, bfe(ctx, 17, 1), 0);
      require_eq(ctx, tfe, bfe(ctx, 16, 1), 0);
      require_eq(ctx, ssamp, bfe(ctx, 53, 5), 0);
   }
}

void
disasm_mimg_gfx11(instr_context& ctx)
{
   print_opcode(ctx, Format::MIMG, bfe(ctx, 18, 8));

   aco_mimg_op_info info = (aco_mimg_op_info)instr_info.mimg_infos.at((aco_opcode)ctx.op);
   aco_mimg_op_info mimg_op = aco_mimg_op_info_get_op(info);

   uint32_t dmask = bfe(ctx, 8, 4);
   uint32_t data_components;
   switch (mimg_op) {
   case aco_mimg_op_info::msaa_load:
   case aco_mimg_op_info::gather4: data_components = 4; break;
   case aco_mimg_op_info::atomic: data_components = 1; break;
   default: data_components = util_bitcount(dmask); break;
   }
   if (bfe(ctx, 53, 1))
      data_components++;

   bool d16 = !!bfe(ctx, 17, 1);
   print_operand(ctx, bfe(ctx, 40, 8) | vgpr, 2,
                 additional_operand_info{.count = DIV_ROUND_UP(data_components, d16 ? 2 : 1)});

   bool nsa = !!bfe(ctx, 0, 1);
   bool a16 = !!bfe(ctx, 16, 1);
   uint32_t coord_components =
      get_mimg_coord_components(ctx, info, (ac_image_dim)bfe(ctx, 2, 3), a16);
   if (nsa) {
      fprintf(ctx.disasm->output, ", [");
      print_operand(ctx, bfe(ctx, 32, 8) | vgpr, 0,
                    additional_operand_info{
                       .skip_comma = true,
                       .count = (mimg_op == aco_mimg_op_info::bvh64 ? 2u : 1u),
                    });
      if (mimg_op == aco_mimg_op_info::bvh || mimg_op == aco_mimg_op_info::bvh64) {
         for (uint32_t i = 0; i < (a16 ? 3 : 4); i++) {
            uint32_t count = i > 0 ? 3 : 1;
            print_operand(ctx, bfe(ctx, 64 + i * 8, 8) | vgpr, 0,
                          additional_operand_info{.count = count});
         }
      } else {
         for (uint32_t i = 0; i < MIN2(4, coord_components - 1); i++) {
            print_operand(
               ctx, bfe(ctx, 64 + i * 8, 8) | vgpr, 0,
               additional_operand_info{.count = (i == 3) ? coord_components - 1 - i : 1});
         }
      }
      fprintf(ctx.disasm->output, "]");
   } else {
      print_operand(ctx, bfe(ctx, 32, 8) | vgpr, 0,
                    additional_operand_info{.count = coord_components});
   }
   ctx.total_size += nsa;

   print_operand(ctx, bfe(ctx, 48, 5) << 2, 0,
                 additional_operand_info{.count = bfe(ctx, 15, 1) ? 4u : 8u});

   if (mimg_op == aco_mimg_op_info::get_lod || mimg_op == aco_mimg_op_info::sample ||
       mimg_op == aco_mimg_op_info::gather4)
      print_operand(ctx, bfe(ctx, 58, 5) << 2, 1, additional_operand_info{.count = 4});

   bool is_bvh = mimg_op == aco_mimg_op_info::bvh || mimg_op == aco_mimg_op_info::bvh64;

   if (!is_bvh) {
      fprintf(ctx.disasm->output, " dmask:0x%x", dmask);
      print_mimg_dim(ctx, (ac_image_dim)bfe(ctx, 2, 3));

      print_flag(ctx, " lwe", 54);
      print_flag(ctx, " unorm", 7);
   }

   print_flag(ctx, " dlc", 13);
   print_flag(ctx, " glc", 14);
   print_flag(ctx, " slc", 12);
   print_flag(ctx, " a16", 16);

   if (!is_bvh) {
      print_flag(ctx, " d16", 17);
      print_flag(ctx, " tfe", 53);
   }

   if (is_bvh) {
      require_eq(ctx, dmask, dmask, 0xf);
      require_eq(ctx, d16, bfe(ctx, 17, 1), 0);
      require_eq(ctx, r128, bfe(ctx, 15, 1), 1);
      require_eq(ctx, unorm, bfe(ctx, 7, 1), 1);
      require_eq(ctx, dim, bfe(ctx, 2, 3), 0);
      require_eq(ctx, lwe, bfe(ctx, 54, 1), 0);
      require_eq(ctx, tfe, bfe(ctx, 53, 1), 0);
      require_eq(ctx, ssamp, bfe(ctx, 58, 5), 0);
   }
}

static const std::unordered_map<aco_opcode, uint32_t> flat_data_sizes = {
   {aco_opcode::flat_load_dwordx2, 2},     {aco_opcode::flat_load_dwordx3, 3},
   {aco_opcode::flat_load_dwordx4, 4},     {aco_opcode::flat_store_dwordx2, 2},
   {aco_opcode::flat_store_dwordx3, 3},    {aco_opcode::flat_store_dwordx4, 4},
   {aco_opcode::flat_atomic_cmpswap, 2},   {aco_opcode::flat_atomic_fcmpswap, 2},
   {aco_opcode::flat_atomic_swap_x2, 2},   {aco_opcode::flat_atomic_cmpswap_x2, 4},
   {aco_opcode::flat_atomic_add_x2, 2},    {aco_opcode::flat_atomic_sub_x2, 2},
   {aco_opcode::flat_atomic_smin_x2, 2},   {aco_opcode::flat_atomic_umin_x2, 2},
   {aco_opcode::flat_atomic_smax_x2, 2},   {aco_opcode::flat_atomic_umax_x2, 2},
   {aco_opcode::flat_atomic_and_x2, 2},    {aco_opcode::flat_atomic_or_x2, 2},
   {aco_opcode::flat_atomic_xor_x2, 2},    {aco_opcode::flat_atomic_inc_x2, 2},
   {aco_opcode::flat_atomic_dec_x2, 2},    {aco_opcode::flat_atomic_fcmpswap_x2, 4},
   {aco_opcode::flat_atomic_fmin_x2, 2},   {aco_opcode::flat_atomic_fmax_x2, 2},
   {aco_opcode::global_load_dwordx2, 2},   {aco_opcode::global_load_dwordx3, 3},
   {aco_opcode::global_load_dwordx4, 4},   {aco_opcode::global_store_dwordx2, 2},
   {aco_opcode::global_store_dwordx3, 3},  {aco_opcode::global_store_dwordx4, 4},
   {aco_opcode::global_atomic_cmpswap, 2}, {aco_opcode::global_atomic_fcmpswap, 2},
   {aco_opcode::global_atomic_swap_x2, 2}, {aco_opcode::global_atomic_cmpswap_x2, 4},
   {aco_opcode::global_atomic_add_x2, 2},  {aco_opcode::global_atomic_sub_x2, 2},
   {aco_opcode::global_atomic_smin_x2, 2}, {aco_opcode::global_atomic_umin_x2, 2},
   {aco_opcode::global_atomic_smax_x2, 2}, {aco_opcode::global_atomic_umax_x2, 2},
   {aco_opcode::global_atomic_and_x2, 2},  {aco_opcode::global_atomic_or_x2, 2},
   {aco_opcode::global_atomic_xor_x2, 2},  {aco_opcode::global_atomic_inc_x2, 2},
   {aco_opcode::global_atomic_dec_x2, 2},  {aco_opcode::global_atomic_fcmpswap_x2, 4},
   {aco_opcode::global_atomic_fmin_x2, 2}, {aco_opcode::global_atomic_fmax_x2, 2},
   {aco_opcode::scratch_load_dwordx2, 2},  {aco_opcode::scratch_load_dwordx3, 3},
   {aco_opcode::scratch_load_dwordx4, 4},  {aco_opcode::scratch_store_dwordx2, 2},
   {aco_opcode::scratch_store_dwordx3, 3}, {aco_opcode::scratch_store_dwordx4, 4},
};

static const std::unordered_set<aco_opcode> flat_loads = {
   aco_opcode::flat_load_ubyte,        aco_opcode::flat_load_sbyte,
   aco_opcode::flat_load_ushort,       aco_opcode::flat_load_sshort,
   aco_opcode::flat_load_dword,        aco_opcode::flat_load_dwordx2,
   aco_opcode::flat_load_dwordx3,      aco_opcode::flat_load_dwordx4,
   aco_opcode::flat_load_ubyte_d16,    aco_opcode::flat_load_ubyte_d16_hi,
   aco_opcode::flat_load_sbyte_d16,    aco_opcode::flat_load_sbyte_d16_hi,
   aco_opcode::flat_load_short_d16,    aco_opcode::flat_load_short_d16_hi,
   aco_opcode::global_load_ubyte,      aco_opcode::global_load_sbyte,
   aco_opcode::global_load_ushort,     aco_opcode::global_load_sshort,
   aco_opcode::global_load_dword,      aco_opcode::global_load_dwordx2,
   aco_opcode::global_load_dwordx3,    aco_opcode::global_load_dwordx4,
   aco_opcode::global_load_ubyte_d16,  aco_opcode::global_load_ubyte_d16_hi,
   aco_opcode::global_load_sbyte_d16,  aco_opcode::global_load_sbyte_d16_hi,
   aco_opcode::global_load_short_d16,  aco_opcode::global_load_short_d16_hi,
   aco_opcode::scratch_load_ubyte,     aco_opcode::scratch_load_sbyte,
   aco_opcode::scratch_load_ushort,    aco_opcode::scratch_load_sshort,
   aco_opcode::scratch_load_dword,     aco_opcode::scratch_load_dwordx2,
   aco_opcode::scratch_load_dwordx3,   aco_opcode::scratch_load_dwordx4,
   aco_opcode::scratch_load_ubyte_d16, aco_opcode::scratch_load_ubyte_d16_hi,
   aco_opcode::scratch_load_sbyte_d16, aco_opcode::scratch_load_sbyte_d16_hi,
   aco_opcode::scratch_load_short_d16, aco_opcode::scratch_load_short_d16_hi,
};

static const std::unordered_set<aco_opcode> flat_atomics = {
   aco_opcode::flat_atomic_swap,
   aco_opcode::flat_atomic_cmpswap,
   aco_opcode::flat_atomic_add,
   aco_opcode::flat_atomic_sub,
   aco_opcode::flat_atomic_smin,
   aco_opcode::flat_atomic_umin,
   aco_opcode::flat_atomic_smax,
   aco_opcode::flat_atomic_umax,
   aco_opcode::flat_atomic_and,
   aco_opcode::flat_atomic_or,
   aco_opcode::flat_atomic_xor,
   aco_opcode::flat_atomic_inc,
   aco_opcode::flat_atomic_dec,
   aco_opcode::flat_atomic_fcmpswap,
   aco_opcode::flat_atomic_fmin,
   aco_opcode::flat_atomic_fmax,
   aco_opcode::flat_atomic_swap_x2,
   aco_opcode::flat_atomic_cmpswap_x2,
   aco_opcode::flat_atomic_add_x2,
   aco_opcode::flat_atomic_sub_x2,
   aco_opcode::flat_atomic_smin_x2,
   aco_opcode::flat_atomic_umin_x2,
   aco_opcode::flat_atomic_smax_x2,
   aco_opcode::flat_atomic_umax_x2,
   aco_opcode::flat_atomic_and_x2,
   aco_opcode::flat_atomic_or_x2,
   aco_opcode::flat_atomic_xor_x2,
   aco_opcode::flat_atomic_inc_x2,
   aco_opcode::flat_atomic_dec_x2,
   aco_opcode::flat_atomic_fcmpswap_x2,
   aco_opcode::flat_atomic_fmin_x2,
   aco_opcode::flat_atomic_fmax_x2,
   aco_opcode::flat_atomic_add_f32,
   aco_opcode::flat_atomic_csub_u32,
   aco_opcode::flat_atomic_cond_sub_u32,
   aco_opcode::flat_atomic_pk_add_f16,
   aco_opcode::flat_atomic_pk_add_bf16,
   aco_opcode::global_atomic_swap,
   aco_opcode::global_atomic_cmpswap,
   aco_opcode::global_atomic_add,
   aco_opcode::global_atomic_sub,
   aco_opcode::global_atomic_smin,
   aco_opcode::global_atomic_umin,
   aco_opcode::global_atomic_smax,
   aco_opcode::global_atomic_umax,
   aco_opcode::global_atomic_and,
   aco_opcode::global_atomic_or,
   aco_opcode::global_atomic_xor,
   aco_opcode::global_atomic_inc,
   aco_opcode::global_atomic_dec,
   aco_opcode::global_atomic_fcmpswap,
   aco_opcode::global_atomic_fmin,
   aco_opcode::global_atomic_fmax,
   aco_opcode::global_atomic_swap_x2,
   aco_opcode::global_atomic_cmpswap_x2,
   aco_opcode::global_atomic_add_x2,
   aco_opcode::global_atomic_sub_x2,
   aco_opcode::global_atomic_smin_x2,
   aco_opcode::global_atomic_umin_x2,
   aco_opcode::global_atomic_smax_x2,
   aco_opcode::global_atomic_umax_x2,
   aco_opcode::global_atomic_and_x2,
   aco_opcode::global_atomic_or_x2,
   aco_opcode::global_atomic_xor_x2,
   aco_opcode::global_atomic_inc_x2,
   aco_opcode::global_atomic_dec_x2,
   aco_opcode::global_atomic_fcmpswap_x2,
   aco_opcode::global_atomic_fmin_x2,
   aco_opcode::global_atomic_fmax_x2,
   aco_opcode::global_atomic_add_f32,
   aco_opcode::global_atomic_cond_sub_u32,
   aco_opcode::global_atomic_pk_add_f16,
   aco_opcode::global_atomic_pk_add_bf16,
};

void
disasm_flatlike(instr_context& ctx)
{
   uint32_t seg = bfe(ctx, 14, 2);
   Format format = Format::FLAT;
   if (seg == 1)
      format = Format::SCRATCH;
   else if (seg == 2)
      format = Format::GLOBAL;

   print_opcode(ctx, format, bfe(ctx, 18, 8));

   uint32_t data_size = 1;
   if (flat_data_sizes.find((aco_opcode)ctx.op) != flat_data_sizes.end())
      data_size = flat_data_sizes.at((aco_opcode)ctx.op);

   if (flat_loads.find((aco_opcode)ctx.op) != flat_loads.end() ||
       (flat_atomics.find((aco_opcode)ctx.op) != flat_atomics.end() && bfe(ctx, 16, 1))) {
      print_operand(ctx, bfe(ctx, 56, 8) | vgpr, operand_index_def,
                    additional_operand_info{.min_count = data_size});
   }

   uint32_t saddr = bfe(ctx, 48, 7);
   bool use_saddr =
      saddr != 0x7F && parse_reg_src(ctx, saddr) != sgpr_null && format != Format::FLAT;
   if (format == Format::SCRATCH && use_saddr) {
      if (ctx.printed_operand)
         fprintf(ctx.disasm->output, ",");
      fprintf(ctx.disasm->output, " off");
      ctx.printed_operand = true;
   } else {
      print_operand(
         ctx, bfe(ctx, 32, 8) | vgpr, 0,
         additional_operand_info{.count = (format == Format::SCRATCH || use_saddr) ? 1u : 2u});
   }

   if (flat_loads.find((aco_opcode)ctx.op) == flat_loads.end())
      print_operand(ctx, bfe(ctx, 40, 8) | vgpr, 1, additional_operand_info{.count = data_size});

   if (use_saddr) {
      print_operand(ctx, saddr, 2,
                    additional_operand_info{.count = (format == Format::SCRATCH) ? 1u : 2u});
   } else if (format != Format::FLAT) {
      fprintf(ctx.disasm->output, ", off");
   }

   if (bfe(ctx, 0, 12)) {
      if (format == Format::FLAT)
         fprintf(ctx.disasm->output, " offset:%u", bfe(ctx, 0, 11));
      else
         fprintf(ctx.disasm->output, " offset:%i", u2i(bfe(ctx, 0, 12), 12));
   }

   print_flag(ctx, " glc", 16);
   print_flag(ctx, " dlc", 12);
   print_flag(ctx, " slc", 17);
   print_flag(ctx, " lds", 13);
}

void
disasm_flatlike_gfx11(instr_context& ctx)
{
   uint32_t seg = bfe(ctx, 16, 2);
   Format format = Format::FLAT;
   if (seg == 1)
      format = Format::SCRATCH;
   else if (seg == 2)
      format = Format::GLOBAL;

   print_opcode(ctx, format, bfe(ctx, 18, 7));

   uint32_t data_size = 1;
   if (flat_data_sizes.find((aco_opcode)ctx.op) != flat_data_sizes.end())
      data_size = flat_data_sizes.at((aco_opcode)ctx.op);

   if (flat_loads.find((aco_opcode)ctx.op) != flat_loads.end() ||
       (flat_atomics.find((aco_opcode)ctx.op) != flat_atomics.end() && bfe(ctx, 14, 1))) {
      print_operand(ctx, bfe(ctx, 56, 8) | vgpr, operand_index_def,
                    additional_operand_info{.min_count = data_size});
   }

   uint32_t saddr = bfe(ctx, 48, 7);
   bool use_saddr =
      saddr != 0x7F && parse_reg_src(ctx, saddr) != sgpr_null && format != Format::FLAT;
   if (format == Format::SCRATCH && !bfe(ctx, 55, 1)) {
      if (ctx.printed_operand)
         fprintf(ctx.disasm->output, ",");
      fprintf(ctx.disasm->output, " off");
      ctx.printed_operand = true;
   } else {
      print_operand(
         ctx, bfe(ctx, 32, 8) | vgpr, 0,
         additional_operand_info{.count = (format == Format::SCRATCH || use_saddr) ? 1u : 2u});
   }

   if (flat_loads.find((aco_opcode)ctx.op) == flat_loads.end())
      print_operand(ctx, bfe(ctx, 40, 8) | vgpr, 1, additional_operand_info{.count = data_size});

   if (use_saddr) {
      print_operand(ctx, saddr, 2,
                    additional_operand_info{.count = (format == Format::SCRATCH) ? 1u : 2u});
   } else if (format != Format::FLAT) {
      fprintf(ctx.disasm->output, ", off");
   }

   if (bfe(ctx, 0, 13)) {
      if (format == Format::FLAT)
         fprintf(ctx.disasm->output, " offset:%u", bfe(ctx, 0, 12));
      else
         fprintf(ctx.disasm->output, " offset:%i", u2i(bfe(ctx, 0, 13), 13));
   }

   print_flag(ctx, " glc", 14);
   print_flag(ctx, " dlc", 13);
   print_flag(ctx, " slc", 15);
}

void
disasm_vintrp(instr_context& ctx)
{
   print_opcode(ctx, Format::VINTRP, bfe(ctx, 16, 2));

   print_definition(ctx, bfe(ctx, 18, 8) | vgpr);

   if ((aco_opcode)ctx.op == aco_opcode::v_interp_mov_f32) {
      switch (bfe(ctx, 0, 8)) {
      case 0: fprintf(ctx.disasm->output, ", p10"); break;
      case 1: fprintf(ctx.disasm->output, ", p20"); break;
      case 2: fprintf(ctx.disasm->output, ", p0"); break;
      default: fprintf(ctx.disasm->output, ", (invalid S1)"); break;
      }
   } else {
      print_operand(ctx, bfe(ctx, 0, 8) | vgpr, 0);
   }

   print_attr(ctx, bfe(ctx, 10, 6), bfe(ctx, 8, 2));
}

void
disasm_vinterp(instr_context& ctx)
{
   print_opcode(ctx, Format::VINTERP_INREG, bfe(ctx, 16, 7));

   Instruction instr = {
      .opcode = (aco_opcode)ctx.op,
      .format = ctx.format,
   };

   print_definition(ctx, bfe(ctx, 0, 8) | vgpr);

   print_operand(ctx, bfe(ctx, 32, 9), 0, additional_operand_info{.neg = !!bfe(ctx, 61, 1)});

   print_operand(ctx, bfe(ctx, 41, 9), 1, additional_operand_info{.neg = !!bfe(ctx, 62, 1)});

   print_operand(ctx, bfe(ctx, 50, 9), 2, additional_operand_info{.neg = !!bfe(ctx, 63, 1)});

   bool has_opsel = false;
   if (get_definition_info(ctx.disasm->program->gfx_level, &instr, 0).bytes() == 2)
      has_opsel = true;

   for (uint32_t i = 0; i < 3; i++) {
      if (get_operand_info(ctx.disasm->program->gfx_level, &instr, i).bytes() == 2)
         has_opsel = true;
   }

   if (has_opsel) {
      uint32_t opsel[4] = {bfe(ctx, 11, 1), bfe(ctx, 12, 1), bfe(ctx, 13, 1), bfe(ctx, 14, 1)};
      print_integer_array(ctx, "op_sel", opsel, 4, 0);
   }

   print_flag(ctx, " clamp", 15);

   fprintf(ctx.disasm->output, " wait_exp:%u", bfe(ctx, 8, 3));
}

void
disasm_ldsdir(instr_context& ctx)
{
   print_opcode(ctx, Format::LDSDIR, bfe(ctx, 20, 2));

   print_operand(ctx, bfe(ctx, 0, 8) | vgpr, operand_index_def,
                 additional_operand_info{.min_count = 1});

   if ((aco_opcode)ctx.op == aco_opcode::lds_param_load) {
      char channels[] = {'x', 'y', 'z', 'w'};
      fprintf(ctx.disasm->output, ", attr%u.%c", bfe(ctx, 10, 6), channels[bfe(ctx, 8, 2)]);
   }

   fprintf(ctx.disasm->output, " wait_vdst:%u", bfe(ctx, 16, 4));
}

void
disasm_ds(instr_context& ctx)
{
   print_opcode(ctx, Format::DS, bfe(ctx, ctx.disasm->program->gfx_level > GFX9 ? 18 : 17, 8));
   print_definition(ctx, bfe(ctx, 56, 8) | vgpr);
   print_operand(ctx, bfe(ctx, 32, 8) | vgpr, 0);

   bool has_data = true;
   switch ((aco_opcode)ctx.op) {
   case aco_opcode::ds_read_b32:
   case aco_opcode::ds_read_i8:
   case aco_opcode::ds_read_u8:
   case aco_opcode::ds_read_i16:
   case aco_opcode::ds_read_u16:
   case aco_opcode::ds_read_u8_d16:
   case aco_opcode::ds_read_u8_d16_hi:
   case aco_opcode::ds_read_i8_d16:
   case aco_opcode::ds_read_i8_d16_hi:
   case aco_opcode::ds_read_u16_d16:
   case aco_opcode::ds_read_u16_d16_hi:
   case aco_opcode::ds_read_b64:
   case aco_opcode::ds_read_b96:
   case aco_opcode::ds_read_b128:
   case aco_opcode::ds_read2_b32:
   case aco_opcode::ds_read2st64_b32:
   case aco_opcode::ds_read2_b64:
   case aco_opcode::ds_read2st64_b64:
   case aco_opcode::ds_read_addtid_b32: has_data = false; break;
   default: break;
   }

   if ((aco_opcode)ctx.op != aco_opcode::ds_swizzle_b32 && has_data) {
      uint32_t data_size = 1;
      switch ((aco_opcode)ctx.op) {
      case aco_opcode::ds_rsub_u64:
      case aco_opcode::ds_mskor_b64:
      case aco_opcode::ds_write_b64:
      case aco_opcode::ds_write2_b64:
      case aco_opcode::ds_write2st64_b64:
      case aco_opcode::ds_cmpst_b64:
      case aco_opcode::ds_cmpst_f64:
      case aco_opcode::ds_min_f64:
      case aco_opcode::ds_max_f64:
      case aco_opcode::ds_rsub_rtn_u64:
      case aco_opcode::ds_mskor_rtn_b64:
      case aco_opcode::ds_wrxchg_rtn_b64:
      case aco_opcode::ds_wrxchg2_rtn_b64:
      case aco_opcode::ds_wrxchg2st64_rtn_b64:
      case aco_opcode::ds_cmpst_rtn_b64:
      case aco_opcode::ds_cmpst_rtn_f64:
      case aco_opcode::ds_min_rtn_f64:
      case aco_opcode::ds_max_rtn_f64: data_size = 2; break;
      case aco_opcode::ds_write_b96: data_size = 3; break;
      case aco_opcode::ds_write_b128: data_size = 4; break;
      default: break;
      }

      print_operand(ctx, bfe(ctx, 40, 8) | vgpr, 1, additional_operand_info{.count = data_size});

      switch ((aco_opcode)ctx.op) {
      case aco_opcode::ds_mskor_b32:
      case aco_opcode::ds_mskor_b64:
      case aco_opcode::ds_mskor_rtn_b32:
      case aco_opcode::ds_mskor_rtn_b64:
      case aco_opcode::ds_cmpst_b32:
      case aco_opcode::ds_cmpst_f32:
      case aco_opcode::ds_min_f32:
      case aco_opcode::ds_max_f32:
      case aco_opcode::ds_min_rtn_f32:
      case aco_opcode::ds_max_rtn_f32:
      case aco_opcode::ds_min_f64:
      case aco_opcode::ds_max_f64:
      case aco_opcode::ds_min_rtn_f64:
      case aco_opcode::ds_max_rtn_f64:
      case aco_opcode::ds_cmpst_b64:
      case aco_opcode::ds_cmpst_f64:
      case aco_opcode::ds_cmpst_rtn_b32:
      case aco_opcode::ds_cmpst_rtn_f32:
      case aco_opcode::ds_cmpst_rtn_b64:
      case aco_opcode::ds_write2_b32:
      case aco_opcode::ds_write2st64_b32:
      case aco_opcode::ds_write2_b64:
      case aco_opcode::ds_write2st64_b64:
      case aco_opcode::ds_wrap_rtn_b32:
         print_operand(ctx, bfe(ctx, 48, 8) | vgpr, 2, additional_operand_info{.count = data_size});
         break;
      default: break;
      }
   }

   switch ((aco_opcode)ctx.op) {
   case aco_opcode::ds_write2_b32:
   case aco_opcode::ds_write2st64_b32:
   case aco_opcode::ds_read2_b32:
   case aco_opcode::ds_read2st64_b32:
   case aco_opcode::ds_ordered_count:
   case aco_opcode::ds_write2_b64:
   case aco_opcode::ds_write2st64_b64:
   case aco_opcode::ds_read2_b64:
   case aco_opcode::ds_read2st64_b64:
   case aco_opcode::ds_write_addtid_b32:
   case aco_opcode::ds_read_addtid_b32:
      if (bfe(ctx, 0, 8))
         fprintf(ctx.disasm->output, " offset0:%u", bfe(ctx, 0, 8));
      if (bfe(ctx, 8, 8))
         fprintf(ctx.disasm->output, " offset1:%u", bfe(ctx, 8, 8));
      break;
   default:
      if (bfe(ctx, 0, 16))
         fprintf(ctx.disasm->output, " offset:%u", bfe(ctx, 0, 16));
      break;
   }

   print_flag(ctx, " gds", ctx.disasm->program->gfx_level > GFX9 ? 17 : 16);
}

void
disasm_exp(instr_context& ctx)
{
   ctx.op = (uint16_t)aco_opcode::exp;
   ctx.format = Format::EXP;

   fprintf(ctx.disasm->output, "exp");

   uint32_t target = bfe(ctx, 4, 6);
   if (target < 8) {
      fprintf(ctx.disasm->output, " mrt%u", target);
   } else if (target == 8) {
      fprintf(ctx.disasm->output, " mrtz");
   } else if (target == 9) {
      fprintf(ctx.disasm->output, " null");
   } else if (target >= 12 && target <= 15) {
      fprintf(ctx.disasm->output, " pos%u", target - 12);
   } else if (target == 20) {
      fprintf(ctx.disasm->output, " prim");
   } else if (target >= 32) {
      fprintf(ctx.disasm->output, " param%u", target - 32);
   }

   if (ctx.disasm->program->gfx_level >= GFX11) {
      if (target == 21) {
         fprintf(ctx.disasm->output, " dual_src_blend0");
      } else if (target == 22) {
         fprintf(ctx.disasm->output, " dual_src_blend1");
      }
   }

   uint32_t reg_stride = ctx.disasm->program->gfx_level < GFX11 && bfe(ctx, 10, 1) ? 2 : 1;
   for (uint32_t i = 0; i < 4; i++) {
      if (bfe(ctx, ROUND_DOWN_TO(i, reg_stride), 1)) {
         print_operand(ctx, bfe(ctx, 32 + i / reg_stride * 8, 8) | vgpr, 0);
      } else {
         if (i != 0)
            fprintf(ctx.disasm->output, ",");
         fprintf(ctx.disasm->output, " off");
      }
      ctx.printed_operand = true;
   }

   print_flag(ctx, " done", 11);

   if (ctx.disasm->program->gfx_level >= GFX11) {
      print_flag(ctx, " row_en", 13);
   } else {
      print_flag(ctx, " compr", 10);
      print_flag(ctx, " vm", 12);
   }
}

size_t
disasm_instr(const disasm_context& ctx, uint32_t* dwords, uint32_t instr_offset)
{
   instr_context instr_ctx = {
      .disasm = &ctx,
      .dwords = dwords,
      .instr_offset = instr_offset,
   };

   for (const auto& encoding : ctx.encoding_infos) {
      if ((dwords[0] >> (32 - encoding.encoding_bitsize) != encoding.encoding))
         continue;

      instr_ctx.encoding = &encoding;
      instr_ctx.total_size = instr_ctx.encoding->size;
      instr_ctx.encoding->disasm(instr_ctx);
      return instr_ctx.total_size;
   }

   fprintf(ctx.output, "(invalid instruction)");
   return 1;
}

#define DECLARE_ENCODING_INFO(min_gfx_level, max_gfx_level, encoding, size, cb)                    \
   {min_gfx_level, max_gfx_level, 0b##encoding, (uint32_t)strlen(#encoding), size, cb}

static const struct encoding_info encoding_infos[] = {
   /* scalar ALU */
   DECLARE_ENCODING_INFO(GFX6, NUM_GFX_VERSIONS, 101111101, 1, disasm_sop1),
   DECLARE_ENCODING_INFO(GFX6, NUM_GFX_VERSIONS, 10, 1, disasm_sop2),
   DECLARE_ENCODING_INFO(GFX6, NUM_GFX_VERSIONS, 1011, 1, disasm_sopk),
   DECLARE_ENCODING_INFO(GFX6, NUM_GFX_VERSIONS, 101111110, 1, disasm_sopc),
   DECLARE_ENCODING_INFO(GFX6, NUM_GFX_VERSIONS, 101111111, 1, disasm_sopp),
   /* scalar MEM */
   DECLARE_ENCODING_INFO(GFX6, GFX9, 11000, 1, disasm_smem),
   DECLARE_ENCODING_INFO(GFX6, GFX9, 110000, 2, disasm_smem_gfx8),
   DECLARE_ENCODING_INFO(GFX10, NUM_GFX_VERSIONS, 111101, 2, disasm_smem_gfx10),
   /* vector ALU */
   DECLARE_ENCODING_INFO(GFX6, NUM_GFX_VERSIONS, 0111111, 1, disasm_vop1),
   DECLARE_ENCODING_INFO(GFX6, NUM_GFX_VERSIONS, 0, 1, disasm_vop2),
   DECLARE_ENCODING_INFO(GFX6, GFX9, 110100, 2, disasm_vop3),
   DECLARE_ENCODING_INFO(GFX10, NUM_GFX_VERSIONS, 110101, 2, disasm_vop3),
   DECLARE_ENCODING_INFO(GFX6, GFX9, 11010011, 2, disasm_vop3p),
   DECLARE_ENCODING_INFO(GFX10, NUM_GFX_VERSIONS, 110011, 2, disasm_vop3p),
   DECLARE_ENCODING_INFO(GFX6, NUM_GFX_VERSIONS, 0111110, 1, disasm_vopc),
   DECLARE_ENCODING_INFO(GFX11, NUM_GFX_VERSIONS, 110010, 2, disasm_vopd),
   /* vector MEM */
   DECLARE_ENCODING_INFO(GFX6, GFX9, 111010, 2, disasm_mtbuf),
   DECLARE_ENCODING_INFO(GFX10, GFX10_3, 111010, 2, disasm_mtbuf_gfx10),
   DECLARE_ENCODING_INFO(GFX11, NUM_GFX_VERSIONS, 111010, 2, disasm_mtbuf_gfx11),
   DECLARE_ENCODING_INFO(GFX6, GFX10_3, 111000, 2, disasm_mubuf),
   DECLARE_ENCODING_INFO(GFX11, NUM_GFX_VERSIONS, 111000, 2, disasm_mubuf_gfx11),
   DECLARE_ENCODING_INFO(GFX6, GFX9, 111100, 2, disasm_mimg),
   DECLARE_ENCODING_INFO(GFX10, GFX10_3, 111100, 2, disasm_mimg_gfx10),
   DECLARE_ENCODING_INFO(GFX11, NUM_GFX_VERSIONS, 111100, 2, disasm_mimg_gfx11),
   DECLARE_ENCODING_INFO(GFX6, GFX10_3, 110111, 2, disasm_flatlike),
   DECLARE_ENCODING_INFO(GFX11, NUM_GFX_VERSIONS, 110111, 2, disasm_flatlike_gfx11),
   /* vector parameter interpolation */
   DECLARE_ENCODING_INFO(GFX6, GFX9, 110101, 1, disasm_vintrp),
   DECLARE_ENCODING_INFO(GFX10, GFX10_3, 110010, 1, disasm_vintrp),
   DECLARE_ENCODING_INFO(GFX11, NUM_GFX_VERSIONS, 11001101, 2, disasm_vinterp),
   /* direct parameter access */
   DECLARE_ENCODING_INFO(GFX11, NUM_GFX_VERSIONS, 11001110, 1, disasm_ldsdir),
   /* DS */
   DECLARE_ENCODING_INFO(GFX6, NUM_GFX_VERSIONS, 110110, 2, disasm_ds),
   /* export */
   DECLARE_ENCODING_INFO(GFX6, GFX9, 110001, 2, disasm_exp),
   DECLARE_ENCODING_INFO(GFX10, NUM_GFX_VERSIONS, 111110, 2, disasm_exp),
};

#undef DECLARE_ENCODING_INFO

struct op_rename {
   amd_gfx_level min_gfx_level;
   aco_opcode op;
   const char* name;
};

static const op_rename op_renames[] = {
   {GFX11, aco_opcode::s_andn2_b32, "s_and_not1_b32"},
   {GFX11, aco_opcode::s_andn2_b64, "s_and_not1_b64"},
   {GFX11, aco_opcode::s_orn2_b32, "s_or_not1_b32"},
   {GFX11, aco_opcode::s_orn2_b64, "s_or_not1_b64"},
   {GFX11, aco_opcode::s_ff1_i32_b32, "s_ctz_i32_b32"},
   {GFX11, aco_opcode::s_ff1_i32_b64, "s_ctz_i32_b64"},
   {GFX11, aco_opcode::s_flbit_i32_b32, "s_clz_i32_u32"},
   {GFX11, aco_opcode::s_flbit_i32_b64, "s_clz_i32_u64"},
   {GFX11, aco_opcode::s_flbit_i32, "s_cls_i32"},
   {GFX11, aco_opcode::s_flbit_i32_i64, "s_cls_i32_i64"},
   {GFX11, aco_opcode::s_andn2_saveexec_b64, "s_and_not1_saveexec_b64"},
   {GFX11, aco_opcode::s_orn2_saveexec_b64, "s_or_not1_saveexec_b64"},
   {GFX11, aco_opcode::s_andn1_saveexec_b64, "s_and_not0_savexec_b64"},
   {GFX11, aco_opcode::s_orn1_saveexec_b64, "s_or_not0_savexec_b64"},
   {GFX11, aco_opcode::s_andn1_wrexec_b64, "s_and_not0_wrexec_b64"},
   {GFX11, aco_opcode::s_andn2_wrexec_b64, "s_and_not1_wrexec_b64"},
   {GFX11, aco_opcode::s_andn2_saveexec_b32, "s_and_not1_saveexec_b32"},
   {GFX11, aco_opcode::s_orn2_saveexec_b32, "s_or_not1_saveexec_b32"},
   {GFX11, aco_opcode::s_andn1_saveexec_b32, "s_and_not0_savexec_b32"},
   {GFX11, aco_opcode::s_orn1_saveexec_b32, "s_or_not0_savexec_b32"},
   {GFX11, aco_opcode::s_andn1_wrexec_b32, "s_and_not0_wrexec_b32"},
   {GFX11, aco_opcode::s_andn2_wrexec_b32, "s_and_not1_wrexec_b32"},
   {GFX11, aco_opcode::s_inst_prefetch, "s_set_inst_prefetch_distance"},
   {GFX11, aco_opcode::s_load_dword, "s_load_b32"},
   {GFX11, aco_opcode::s_load_dwordx2, "s_load_b64"},
   {GFX11, aco_opcode::s_load_dwordx4, "s_load_b128"},
   {GFX11, aco_opcode::s_load_dwordx8, "s_load_b256"},
   {GFX11, aco_opcode::s_load_dwordx16, "s_load_b512"},
   {GFX11, aco_opcode::s_buffer_load_dword, "s_buffer_load_b32"},
   {GFX11, aco_opcode::s_buffer_load_dwordx2, "s_buffer_load_b64"},
   {GFX11, aco_opcode::s_buffer_load_dwordx4, "s_buffer_load_b128"},
   {GFX11, aco_opcode::s_buffer_load_dwordx8, "s_buffer_load_b256"},
   {GFX11, aco_opcode::s_buffer_load_dwordx16, "s_buffer_load_b512"},
   {GFX10_3, aco_opcode::v_mac_legacy_f32, "v_fmac_legacy_f32"},
   {GFX11, aco_opcode::v_fmac_legacy_f32, "v_fmac_dx9_zero_f32"},
   {GFX11, aco_opcode::v_mul_legacy_f32, "v_mul_dx9_zero_f32"},
   {GFX6, aco_opcode::v_addc_co_u32, "v_addc_u32"},
   {GFX6, aco_opcode::v_subb_co_u32, "v_subb_u32"},
   {GFX6, aco_opcode::v_subbrev_co_u32, "v_subbrev_u32"},
   {GFX10, aco_opcode::v_addc_co_u32, "v_add_co_ci_u32"},
   {GFX10, aco_opcode::v_subb_co_u32, "v_sub_co_ci_u32"},
   {GFX10, aco_opcode::v_subbrev_co_u32, "v_subrev_co_ci_u32"},
   {GFX10, aco_opcode::v_add_u16_e64, "v_add_nc_u16"},
   {GFX10, aco_opcode::v_sub_u16_e64, "v_sub_nc_u16"},
   {GFX10, aco_opcode::v_mul_lo_u16_e64, "v_mul_lo_u16"},
   {GFX10, aco_opcode::v_max_u16_e64, "v_max_u16"},
   {GFX10, aco_opcode::v_max_i16_e64, "v_max_i16"},
   {GFX10, aco_opcode::v_min_u16_e64, "v_min_u16"},
   {GFX10, aco_opcode::v_min_i16_e64, "v_min_i16"},
   {GFX10, aco_opcode::v_lshrrev_b16_e64, "v_lshrrev_b16"},
   {GFX10, aco_opcode::v_ashrrev_i16_e64, "v_ashrrev_i16"},
   {GFX10, aco_opcode::v_lshlrev_b16_e64, "v_lshlrev_b16"},
   {GFX11, aco_opcode::v_cvt_pkrtz_f16_f32, "v_cvt_pk_rtz_f16_f32"},
   {GFX11, aco_opcode::v_cvt_pknorm_i16_f16, "v_cvt_pk_norm_i16_f16"},
   {GFX11, aco_opcode::v_cvt_pknorm_u16_f16, "v_cvt_pk_norm_u16_f16"},
   {GFX11, aco_opcode::v_cvt_pknorm_i16_f32, "v_cvt_pk_norm_i16_f32"},
   {GFX11, aco_opcode::v_cvt_pknorm_u16_f32, "v_cvt_pk_norm_u16_f32"},
   {GFX6, aco_opcode::v_add_co_u32, "v_add_u32"},
   {GFX6, aco_opcode::v_sub_co_u32, "v_sub_u32"},
   {GFX9, aco_opcode::v_add_co_u32, "v_add_co_u32"},
   {GFX9, aco_opcode::v_sub_co_u32, "v_sub_co_u32"},
   {GFX10, aco_opcode::v_add_u32, "v_add_nc_u32"},
   {GFX10, aco_opcode::v_sub_u32, "v_sub_nc_u32"},
   {GFX10, aco_opcode::v_subrev_u32, "v_subrev_nc_u32"},
   {GFX11, aco_opcode::v_dot2c_f32_f16, "v_dot2acc_f32_f16"},
   {GFX11, aco_opcode::v_cvt_rpi_i32_f32, "v_cvt_nearest_i32_f32"},
   {GFX11, aco_opcode::v_cvt_flr_i32_f32, "v_cvt_floor_i32_f32"},
   {GFX11, aco_opcode::v_ffbh_u32, "v_clz_i32_u32"},
   {GFX11, aco_opcode::v_ffbl_b32, "v_ctz_i32_b32"},
   {GFX11, aco_opcode::v_ffbh_i32, "v_cls_i32"},
   {GFX9, aco_opcode::v_fma_mix_f32, "v_mad_mix_f32"},
   {GFX9, aco_opcode::v_fma_mixlo_f16, "v_mad_mixlo_f16"},
   {GFX9, aco_opcode::v_fma_mixhi_f16, "v_mad_mixhi_f16"},
   {GFX10, aco_opcode::v_fma_mix_f32, "v_fma_mix_f32"},
   {GFX10, aco_opcode::v_fma_mixlo_f16, "v_fma_mixlo_f16"},
   {GFX10, aco_opcode::v_fma_mixhi_f16, "v_fma_mixhi_f16"},
   {GFX8, aco_opcode::v_mad_legacy_f16, "v_mad_f16"},
   {GFX8, aco_opcode::v_mad_legacy_u16, "v_mad_u16"},
   {GFX8, aco_opcode::v_mad_legacy_i16, "v_mad_i16"},
   {GFX9, aco_opcode::v_mad_legacy_f16, "v_mad_legacy_f16"},
   {GFX9, aco_opcode::v_mad_legacy_u16, "v_mad_legacy_u16"},
   {GFX9, aco_opcode::v_mad_legacy_i16, "v_mad_legacy_i16"},
   {GFX10_3, aco_opcode::v_mad_legacy_f32, "v_fma_legacy_f32"},
   {GFX11, aco_opcode::v_fma_legacy_f32, "v_fma_dx9_zero_f32"},
   {GFX8, aco_opcode::v_mbcnt_hi_u32_b32_e64, "v_mbcnt_hi_u32_b32"},
   {GFX8, aco_opcode::v_lshlrev_b64_e64, "v_lshlrev_b64"},
   {GFX8, aco_opcode::v_cvt_pkrtz_f16_f32_e64, "v_cvt_pkrtz_f16_f32"},
   {GFX6, aco_opcode::v_subrev_co_u32, "v_subrev_u32"},
   {GFX10, aco_opcode::v_add_co_u32_e64, "v_add_co_u32"},
   {GFX10, aco_opcode::v_sub_co_u32_e64, "v_sub_co_u32"},
   {GFX10, aco_opcode::v_subrev_co_u32_e64, "v_subrev_co_u32"},
   {GFX8, aco_opcode::v_readlane_b32_e64, "v_readlane_b32"},
   {GFX8, aco_opcode::v_writelane_b32_e64, "v_writelane_b32"},
   {GFX6, aco_opcode::v_cmp_lg_u16, "v_cmp_ne_u16"},
   {GFX6, aco_opcode::v_cmp_lg_i16, "v_cmp_ne_i16"},
   {GFX6, aco_opcode::v_cmpx_lg_u16, "v_cmpx_ne_u16"},
   {GFX6, aco_opcode::v_cmpx_lg_i16, "v_cmpx_ne_i16"},
   {GFX6, aco_opcode::v_cmp_lg_u32, "v_cmp_ne_u32"},
   {GFX6, aco_opcode::v_cmp_lg_i32, "v_cmp_ne_i32"},
   {GFX6, aco_opcode::v_cmpx_lg_u32, "v_cmpx_ne_u32"},
   {GFX6, aco_opcode::v_cmpx_lg_i32, "v_cmpx_ne_i32"},
   {GFX6, aco_opcode::v_cmp_lg_u64, "v_cmp_ne_u64"},
   {GFX6, aco_opcode::v_cmp_lg_i64, "v_cmp_ne_i64"},
   {GFX6, aco_opcode::v_cmpx_lg_u64, "v_cmpx_ne_u64"},
   {GFX6, aco_opcode::v_cmpx_lg_i64, "v_cmpx_ne_i64"},
   {GFX6, aco_opcode::v_add_f64_e64, "v_add_f64"},
   {GFX6, aco_opcode::v_mul_f64_e64, "v_mul_f64"},
   {GFX6, aco_opcode::v_min_f64_e64, "v_min_f64"},
   {GFX6, aco_opcode::v_max_f64_e64, "v_max_f64"},
   {GFX11, aco_opcode::ds_write_b32, "ds_store_b32"},
   {GFX11, aco_opcode::ds_write2_b32, "ds_store_2addr_b32"},
   {GFX11, aco_opcode::ds_write2st64_b32, "ds_store_2addr_stride64_b32"},
   {GFX11, aco_opcode::ds_cmpst_b32, "ds_cmpstore_b32"},
   {GFX11, aco_opcode::ds_cmpst_f32, "ds_cmpstore_f32"},
   {GFX11, aco_opcode::ds_write_addtid_b32, "ds_store_addtid_b32"},
   {GFX11, aco_opcode::ds_write_b8, "ds_store_b8"},
   {GFX11, aco_opcode::ds_write_b16, "ds_store_b16"},
   {GFX11, aco_opcode::ds_wrxchg_rtn_b32, "ds_storexchg_rtn_b32"},
   {GFX11, aco_opcode::ds_wrxchg2_rtn_b32, "ds_storexchg_2addr_rtn_b32"},
   {GFX11, aco_opcode::ds_wrxchg2st64_rtn_b32, "ds_storexchg_2addr_stride64_rtn_b32"},
   {GFX11, aco_opcode::ds_cmpst_rtn_b32, "ds_cmpstore_rtn_b32"},
   {GFX11, aco_opcode::ds_cmpst_rtn_f32, "ds_cmpstore_rtn_f32"},
   {GFX11, aco_opcode::ds_read_b32, "ds_load_b32"},
   {GFX11, aco_opcode::ds_read2_b32, "ds_load_2addr_b32"},
   {GFX11, aco_opcode::ds_read2st64_b32, "ds_load_2addr_stride64_b32"},
   {GFX11, aco_opcode::ds_read_i8, "ds_load_i8"},
   {GFX11, aco_opcode::ds_read_u8, "ds_load_u8"},
   {GFX11, aco_opcode::ds_read_i16, "ds_load_i16"},
   {GFX11, aco_opcode::ds_read_u16, "ds_load_u16"},
   {GFX11, aco_opcode::ds_write_b64, "ds_store_b64"},
   {GFX11, aco_opcode::ds_write2_b64, "ds_store_2addr_b64"},
   {GFX11, aco_opcode::ds_write2st64_b64, "ds_store_2addr_stride64_b64"},
   {GFX11, aco_opcode::ds_cmpst_b64, "ds_cmpstore_b64"},
   {GFX11, aco_opcode::ds_cmpst_f64, "ds_cmpstore_f64"},
   {GFX11, aco_opcode::ds_write_b8_d16_hi, "ds_store_b8_d16_hi"},
   {GFX11, aco_opcode::ds_write_b16_d16_hi, "ds_store_b16_d16_hi"},
   {GFX11, aco_opcode::ds_read_u8_d16, "ds_load_u8_d16"},
   {GFX11, aco_opcode::ds_read_u8_d16_hi, "ds_load_u8_d16_hi"},
   {GFX11, aco_opcode::ds_read_i8_d16, "ds_load_i8_d16"},
   {GFX11, aco_opcode::ds_read_i8_d16_hi, "ds_load_i8_d16_hi"},
   {GFX11, aco_opcode::ds_read_u16_d16, "ds_load_u16_d16"},
   {GFX11, aco_opcode::ds_read_u16_d16_hi, "ds_load_u16_d16_hi"},
   {GFX11, aco_opcode::ds_wrxchg_rtn_b64, "ds_storexchg_rtn_b64"},
   {GFX11, aco_opcode::ds_wrxchg2_rtn_b64, "ds_storexchg_2addr_rtn_b64"},
   {GFX11, aco_opcode::ds_wrxchg2st64_rtn_b64, "ds_storexchg_2addr_stride64_rtn_b64"},
   {GFX11, aco_opcode::ds_cmpst_rtn_b64, "ds_cmpstore_rtn_b64"},
   {GFX11, aco_opcode::ds_cmpst_rtn_f64, "ds_cmpstore_rtn_f64"},
   {GFX11, aco_opcode::ds_read_b64, "ds_load_b64"},
   {GFX11, aco_opcode::ds_read2_b64, "ds_load_2addr_b64"},
   {GFX11, aco_opcode::ds_read2st64_b64, "ds_load_2addr_stride64_b64"},
   {GFX11, aco_opcode::ds_read_addtid_b32, "ds_load_addtid_b32"},
   {GFX11, aco_opcode::ds_write_b96, "ds_store_b96"},
   {GFX11, aco_opcode::ds_write_b128, "ds_store_b128"},
   {GFX11, aco_opcode::ds_read_b96, "ds_load_b96"},
   {GFX11, aco_opcode::ds_read_b128, "ds_load_b128"},
   {GFX11, aco_opcode::buffer_atomic_csub, "buffer_atomic_csub_u32"},
   {GFX11, aco_opcode::buffer_load_format_d16_x, "buffer_load_d16_format_x"},
   {GFX11, aco_opcode::buffer_load_format_d16_xy, "buffer_load_d16_format_xy"},
   {GFX11, aco_opcode::buffer_load_format_d16_xyz, "buffer_load_d16_format_xyz"},
   {GFX11, aco_opcode::buffer_load_format_d16_xyzw, "buffer_load_d16_format_xyzw"},
   {GFX11, aco_opcode::buffer_store_format_d16_x, "buffer_store_d16_format_x"},
   {GFX11, aco_opcode::buffer_store_format_d16_xy, "buffer_store_d16_format_xy"},
   {GFX11, aco_opcode::buffer_store_format_d16_xyz, "buffer_store_d16_format_xyz"},
   {GFX11, aco_opcode::buffer_store_format_d16_xyzw, "buffer_store_d16_format_xyzw"},
   {GFX11, aco_opcode::buffer_store_byte, "buffer_store_b8"},
   {GFX11, aco_opcode::buffer_store_byte_d16_hi, "buffer_store_d16_hi_b8"},
   {GFX11, aco_opcode::buffer_store_short, "buffer_store_b16"},
   {GFX11, aco_opcode::buffer_store_short_d16_hi, "buffer_store_d16_hi_b16"},
   {GFX11, aco_opcode::buffer_store_dword, "buffer_store_b32"},
   {GFX11, aco_opcode::buffer_store_dwordx2, "buffer_store_b64"},
   {GFX11, aco_opcode::buffer_store_dwordx3, "buffer_store_b96"},
   {GFX11, aco_opcode::buffer_store_dwordx4, "buffer_store_b128"},
   {GFX11, aco_opcode::buffer_load_ubyte, "buffer_load_u8"},
   {GFX11, aco_opcode::buffer_load_sbyte, "buffer_load_i8"},
   {GFX11, aco_opcode::buffer_load_ushort, "buffer_load_u16"},
   {GFX11, aco_opcode::buffer_load_sshort, "buffer_load_i16"},
   {GFX11, aco_opcode::buffer_load_dword, "buffer_load_b32"},
   {GFX11, aco_opcode::buffer_load_dwordx2, "buffer_load_b64"},
   {GFX11, aco_opcode::buffer_load_dwordx3, "buffer_load_b96"},
   {GFX11, aco_opcode::buffer_load_dwordx4, "buffer_load_b128"},
   {GFX11, aco_opcode::buffer_load_ubyte_d16, "buffer_load_d16_u8"},
   {GFX11, aco_opcode::buffer_load_ubyte_d16_hi, "buffer_load_d16_hi_u8"},
   {GFX11, aco_opcode::buffer_load_sbyte_d16, "buffer_load_d16_i8"},
   {GFX11, aco_opcode::buffer_load_sbyte_d16_hi, "buffer_load_d16_hi_i8"},
   {GFX11, aco_opcode::buffer_load_short_d16, "buffer_load_d16_b16"},
   {GFX11, aco_opcode::buffer_load_short_d16_hi, "buffer_load_d16_hi_b16"},
   {GFX11, aco_opcode::buffer_load_format_d16_hi_x, "buffer_load_d16_hi_format_x"},
   {GFX11, aco_opcode::buffer_store_format_d16_hi_x, "buffer_store_d16_hi_format_x"},
   {GFX11, aco_opcode::buffer_atomic_swap, "buffer_atomic_swap_b32"},
   {GFX11, aco_opcode::buffer_atomic_cmpswap, "buffer_atomic_cmpswap_b32"},
   {GFX11, aco_opcode::buffer_atomic_add, "buffer_atomic_add_u32"},
   {GFX11, aco_opcode::buffer_atomic_sub, "buffer_atomic_sub_u32"},
   {GFX11, aco_opcode::buffer_atomic_smin, "buffer_atomic_min_i32"},
   {GFX11, aco_opcode::buffer_atomic_umin, "buffer_atomic_min_u32"},
   {GFX11, aco_opcode::buffer_atomic_smax, "buffer_atomic_max_i32"},
   {GFX11, aco_opcode::buffer_atomic_umax, "buffer_atomic_max_u32"},
   {GFX11, aco_opcode::buffer_atomic_and, "buffer_atomic_and_b32"},
   {GFX11, aco_opcode::buffer_atomic_or, "buffer_atomic_or_b32"},
   {GFX11, aco_opcode::buffer_atomic_xor, "buffer_atomic_xor_b32"},
   {GFX11, aco_opcode::buffer_atomic_inc, "buffer_atomic_inc_u32"},
   {GFX11, aco_opcode::buffer_atomic_dec, "buffer_atomic_dec_b32"},
   {GFX11, aco_opcode::buffer_atomic_fcmpswap, "buffer_atomic_cmpswap_f32"},
   {GFX11, aco_opcode::buffer_atomic_fmin, "buffer_atomic_min_f32"},
   {GFX11, aco_opcode::buffer_atomic_fmax, "buffer_atomic_max_f32"},
   {GFX11, aco_opcode::buffer_atomic_swap_x2, "buffer_atomic_swap_b64"},
   {GFX11, aco_opcode::buffer_atomic_cmpswap_x2, "buffer_atomic_cmpswap_b64"},
   {GFX11, aco_opcode::buffer_atomic_add_x2, "buffer_atomic_add_u64"},
   {GFX11, aco_opcode::buffer_atomic_sub_x2, "buffer_atomic_sub_u64"},
   {GFX11, aco_opcode::buffer_atomic_smin_x2, "buffer_atomic_min_i64"},
   {GFX11, aco_opcode::buffer_atomic_umin_x2, "buffer_atomic_min_u64"},
   {GFX11, aco_opcode::buffer_atomic_smax_x2, "buffer_atomic_max_i64"},
   {GFX11, aco_opcode::buffer_atomic_umax_x2, "buffer_atomic_max_u64"},
   {GFX11, aco_opcode::buffer_atomic_and_x2, "buffer_atomic_and_b64"},
   {GFX11, aco_opcode::buffer_atomic_or_x2, "buffer_atomic_or_b64"},
   {GFX11, aco_opcode::buffer_atomic_xor_x2, "buffer_atomic_xor_b64"},
   {GFX11, aco_opcode::buffer_atomic_inc_x2, "buffer_atomic_inc_u64"},
   {GFX11, aco_opcode::buffer_atomic_dec_x2, "buffer_atomic_dec_u64"},
   {GFX11, aco_opcode::global_load_ubyte, "global_load_u8"},
   {GFX11, aco_opcode::global_load_sbyte, "global_load_i8"},
   {GFX11, aco_opcode::global_load_ushort, "global_load_u16"},
   {GFX11, aco_opcode::global_load_sshort, "global_load_i16"},
   {GFX11, aco_opcode::global_load_dword, "global_load_b32"},
   {GFX11, aco_opcode::global_load_dwordx2, "global_load_b64"},
   {GFX11, aco_opcode::global_load_dwordx3, "global_load_b96"},
   {GFX11, aco_opcode::global_load_dwordx4, "global_load_b128"},
   {GFX11, aco_opcode::global_store_byte, "global_store_u8"},
   {GFX11, aco_opcode::global_store_byte_d16_hi, "global_store_d16_hi_u8"},
   {GFX11, aco_opcode::global_store_short, "global_store_b16"},
   {GFX11, aco_opcode::global_store_short_d16_hi, "global_store_d16_hi_b16"},
   {GFX11, aco_opcode::global_store_dword, "global_store_b32"},
   {GFX11, aco_opcode::global_store_dwordx2, "global_store_b64"},
   {GFX11, aco_opcode::global_store_dwordx3, "global_store_b96"},
   {GFX11, aco_opcode::global_store_dwordx4, "global_store_b128"},
   {GFX11, aco_opcode::global_load_ubyte_d16, "global_load_d16_u8"},
   {GFX11, aco_opcode::global_load_ubyte_d16_hi, "global_load_d16_hi_u8"},
   {GFX11, aco_opcode::global_load_sbyte_d16, "global_load_d16_i8"},
   {GFX11, aco_opcode::global_load_sbyte_d16_hi, "global_load_d16_hi_i8"},
   {GFX11, aco_opcode::global_load_short_d16, "global_load_d16_b16"},
   {GFX11, aco_opcode::global_load_short_d16_hi, "global_load_d16_hi_b16"},
   {GFX11, aco_opcode::global_atomic_swap, "global_atomic_swap_b32"},
   {GFX11, aco_opcode::global_atomic_cmpswap, "global_atomic_cmpswap_b32"},
   {GFX11, aco_opcode::global_atomic_add, "global_atomic_add_u32"},
   {GFX11, aco_opcode::global_atomic_sub, "global_atomic_sub_u32"},
   {GFX11, aco_opcode::global_atomic_smin, "global_atomic_min_i32"},
   {GFX11, aco_opcode::global_atomic_umin, "global_atomic_min_u32"},
   {GFX11, aco_opcode::global_atomic_smax, "global_atomic_max_i32"},
   {GFX11, aco_opcode::global_atomic_umax, "global_atomic_max_u32"},
   {GFX11, aco_opcode::global_atomic_and, "global_atomic_and_b32"},
   {GFX11, aco_opcode::global_atomic_or, "global_atomic_or_b32"},
   {GFX11, aco_opcode::global_atomic_xor, "global_atomic_xor_b32"},
   {GFX11, aco_opcode::global_atomic_inc, "global_atomic_inc_u32"},
   {GFX11, aco_opcode::global_atomic_dec, "global_atomic_dec_b32"},
   {GFX11, aco_opcode::global_atomic_fcmpswap, "global_atomic_cmpswap_f32"},
   {GFX11, aco_opcode::global_atomic_fmin, "global_atomic_min_f32"},
   {GFX11, aco_opcode::global_atomic_fmax, "global_atomic_max_f32"},
   {GFX11, aco_opcode::global_atomic_swap_x2, "global_atomic_swap_b64"},
   {GFX11, aco_opcode::global_atomic_cmpswap_x2, "global_atomic_cmpswap_b64"},
   {GFX11, aco_opcode::global_atomic_add_x2, "global_atomic_add_u64"},
   {GFX11, aco_opcode::global_atomic_sub_x2, "global_atomic_sub_u64"},
   {GFX11, aco_opcode::global_atomic_smin_x2, "global_atomic_min_i64"},
   {GFX11, aco_opcode::global_atomic_umin_x2, "global_atomic_min_u64"},
   {GFX11, aco_opcode::global_atomic_smax_x2, "global_atomic_max_i64"},
   {GFX11, aco_opcode::global_atomic_umax_x2, "global_atomic_max_u64"},
   {GFX11, aco_opcode::global_atomic_and_x2, "global_atomic_and_b64"},
   {GFX11, aco_opcode::global_atomic_or_x2, "global_atomic_or_b64"},
   {GFX11, aco_opcode::global_atomic_xor_x2, "global_atomic_xor_b64"},
   {GFX11, aco_opcode::global_atomic_inc_x2, "global_atomic_inc_u64"},
   {GFX11, aco_opcode::global_atomic_dec_x2, "global_atomic_dec_u64"},
   {GFX11, aco_opcode::flat_load_ubyte, "flat_load_u8"},
   {GFX11, aco_opcode::flat_load_sbyte, "flat_load_i8"},
   {GFX11, aco_opcode::flat_load_ushort, "flat_load_u16"},
   {GFX11, aco_opcode::flat_load_sshort, "flat_load_i16"},
   {GFX11, aco_opcode::flat_load_dword, "flat_load_b32"},
   {GFX11, aco_opcode::flat_load_dwordx2, "flat_load_b64"},
   {GFX11, aco_opcode::flat_load_dwordx3, "flat_load_b96"},
   {GFX11, aco_opcode::flat_load_dwordx4, "flat_load_b128"},
   {GFX11, aco_opcode::flat_store_byte, "flat_store_u8"},
   {GFX11, aco_opcode::flat_store_byte_d16_hi, "flat_store_d16_hi_u8"},
   {GFX11, aco_opcode::flat_store_short, "flat_store_b16"},
   {GFX11, aco_opcode::flat_store_short_d16_hi, "flat_store_d16_hi_b16"},
   {GFX11, aco_opcode::flat_store_dword, "flat_store_b32"},
   {GFX11, aco_opcode::flat_store_dwordx2, "flat_store_b64"},
   {GFX11, aco_opcode::flat_store_dwordx3, "flat_store_b96"},
   {GFX11, aco_opcode::flat_store_dwordx4, "flat_store_b128"},
   {GFX11, aco_opcode::flat_load_ubyte_d16, "flat_load_d16_u8"},
   {GFX11, aco_opcode::flat_load_ubyte_d16_hi, "flat_load_d16_hi_u8"},
   {GFX11, aco_opcode::flat_load_sbyte_d16, "flat_load_d16_i8"},
   {GFX11, aco_opcode::flat_load_sbyte_d16_hi, "flat_load_d16_hi_i8"},
   {GFX11, aco_opcode::flat_load_short_d16, "flat_load_d16_b16"},
   {GFX11, aco_opcode::flat_load_short_d16_hi, "flat_load_d16_hi_b16"},
   {GFX11, aco_opcode::flat_atomic_swap, "flat_atomic_swap_b32"},
   {GFX11, aco_opcode::flat_atomic_cmpswap, "flat_atomic_cmpswap_b32"},
   {GFX11, aco_opcode::flat_atomic_add, "flat_atomic_add_u32"},
   {GFX11, aco_opcode::flat_atomic_sub, "flat_atomic_sub_u32"},
   {GFX11, aco_opcode::flat_atomic_smin, "flat_atomic_min_i32"},
   {GFX11, aco_opcode::flat_atomic_umin, "flat_atomic_min_u32"},
   {GFX11, aco_opcode::flat_atomic_smax, "flat_atomic_max_i32"},
   {GFX11, aco_opcode::flat_atomic_umax, "flat_atomic_max_u32"},
   {GFX11, aco_opcode::flat_atomic_and, "flat_atomic_and_b32"},
   {GFX11, aco_opcode::flat_atomic_or, "flat_atomic_or_b32"},
   {GFX11, aco_opcode::flat_atomic_xor, "flat_atomic_xor_b32"},
   {GFX11, aco_opcode::flat_atomic_inc, "flat_atomic_inc_u32"},
   {GFX11, aco_opcode::flat_atomic_dec, "flat_atomic_dec_b32"},
   {GFX11, aco_opcode::flat_atomic_fcmpswap, "flat_atomic_cmpswap_f32"},
   {GFX11, aco_opcode::flat_atomic_fmin, "flat_atomic_min_f32"},
   {GFX11, aco_opcode::flat_atomic_fmax, "flat_atomic_max_f32"},
   {GFX11, aco_opcode::flat_atomic_swap_x2, "flat_atomic_swap_b64"},
   {GFX11, aco_opcode::flat_atomic_cmpswap_x2, "flat_atomic_cmpswap_b64"},
   {GFX11, aco_opcode::flat_atomic_add_x2, "flat_atomic_add_u64"},
   {GFX11, aco_opcode::flat_atomic_sub_x2, "flat_atomic_sub_u64"},
   {GFX11, aco_opcode::flat_atomic_smin_x2, "flat_atomic_min_i64"},
   {GFX11, aco_opcode::flat_atomic_umin_x2, "flat_atomic_min_u64"},
   {GFX11, aco_opcode::flat_atomic_smax_x2, "flat_atomic_max_i64"},
   {GFX11, aco_opcode::flat_atomic_umax_x2, "flat_atomic_max_u64"},
   {GFX11, aco_opcode::flat_atomic_and_x2, "flat_atomic_and_b64"},
   {GFX11, aco_opcode::flat_atomic_or_x2, "flat_atomic_or_b64"},
   {GFX11, aco_opcode::flat_atomic_xor_x2, "flat_atomic_xor_b64"},
   {GFX11, aco_opcode::flat_atomic_inc_x2, "flat_atomic_inc_u64"},
   {GFX11, aco_opcode::flat_atomic_dec_x2, "flat_atomic_dec_u64"},
   {GFX11, aco_opcode::scratch_load_ubyte, "scratch_load_u8"},
   {GFX11, aco_opcode::scratch_load_sbyte, "scratch_load_i8"},
   {GFX11, aco_opcode::scratch_load_ushort, "scratch_load_u16"},
   {GFX11, aco_opcode::scratch_load_sshort, "scratch_load_i16"},
   {GFX11, aco_opcode::scratch_load_dword, "scratch_load_b32"},
   {GFX11, aco_opcode::scratch_load_dwordx2, "scratch_load_b64"},
   {GFX11, aco_opcode::scratch_load_dwordx3, "scratch_load_b96"},
   {GFX11, aco_opcode::scratch_load_dwordx4, "scratch_load_b128"},
   {GFX11, aco_opcode::scratch_store_byte, "scratch_store_u8"},
   {GFX11, aco_opcode::scratch_store_byte_d16_hi, "scratch_store_d16_hi_u8"},
   {GFX11, aco_opcode::scratch_store_short, "scratch_store_b16"},
   {GFX11, aco_opcode::scratch_store_short_d16_hi, "scratch_store_d16_hi_b16"},
   {GFX11, aco_opcode::scratch_store_dword, "scratch_store_b32"},
   {GFX11, aco_opcode::scratch_store_dwordx2, "scratch_store_b64"},
   {GFX11, aco_opcode::scratch_store_dwordx3, "scratch_store_b96"},
   {GFX11, aco_opcode::scratch_store_dwordx4, "scratch_store_b128"},
   {GFX11, aco_opcode::scratch_load_ubyte_d16, "scratch_load_d16_u8"},
   {GFX11, aco_opcode::scratch_load_ubyte_d16_hi, "scratch_load_d16_hi_u8"},
   {GFX11, aco_opcode::scratch_load_sbyte_d16, "scratch_load_d16_i8"},
   {GFX11, aco_opcode::scratch_load_sbyte_d16_hi, "scratch_load_d16_hi_i8"},
   {GFX11, aco_opcode::scratch_load_short_d16, "scratch_load_d16_b16"},
   {GFX11, aco_opcode::scratch_load_short_d16_hi, "scratch_load_d16_hi_b16"},
   {GFX11, aco_opcode::v_interp_p10_f32_inreg, "v_interp_p10_f32"},
   {GFX11, aco_opcode::v_interp_p2_f32_inreg, "v_interp_p2_f32"},
   {GFX11, aco_opcode::v_interp_p10_f16_f32_inreg, "v_interp_p10_f16_f32"},
   {GFX11, aco_opcode::v_interp_p2_f16_f32_inreg, "v_interp_p2_f16_f32"},
   {GFX11, aco_opcode::v_interp_p10_rtz_f16_f32_inreg, "v_interp_p10_rtz_f16_f32"},
   {GFX11, aco_opcode::v_interp_p2_rtz_f16_f32_inreg, "v_interp_p2_rtz_f16_f32"},
   {GFX9, aco_opcode::v_interp_p2_hi_f16, "v_interp_p2_f16"},
   {GFX8, aco_opcode::v_interp_p2_legacy_f16, "v_interp_p2_f16"},
   {GFX9, aco_opcode::v_interp_p2_legacy_f16, "v_interp_p2_legacy_f16"},
};

void
disasm_context_init(disasm_context& ctx, Program* program, FILE* output)
{
   ctx.program = program;
   ctx.output = output;

   ctx.referenced_blocks.resize(program->blocks.size());
   ctx.referenced_blocks[0] = true;
   for (Block& block : program->blocks) {
      for (uint32_t succ : block.linear_succs)
         ctx.referenced_blocks[succ] = true;
   }

   if (program->gfx_level <= GFX7)
      ctx.opcode_encodings = &instr_info.opcode_gfx7[0];
   else if (program->gfx_level <= GFX9)
      ctx.opcode_encodings = &instr_info.opcode_gfx9[0];
   else if (program->gfx_level <= GFX10_3)
      ctx.opcode_encodings = &instr_info.opcode_gfx10[0];
   else if (program->gfx_level >= GFX11)
      ctx.opcode_encodings = &instr_info.opcode_gfx11[0];

   uint16_t vop1_as_vop3_offset =
      (program->gfx_level == GFX8 || program->gfx_level == GFX9) ? 0x140 : 0x180;

   for (uint16_t i = 0; i < (uint16_t)aco_opcode::num_opcodes; i++) {
      Format format = instr_info.format[i];
      if (ctx.opcodes.find(format) == ctx.opcodes.end())
         ctx.opcodes[format] = {};

      ctx.opcodes.at(format)[ctx.opcode_encodings[i]] = i;

      switch (format) {
      case Format::VOP1: ctx.vop3_opcodes[ctx.opcode_encodings[i] + vop1_as_vop3_offset] = i; break;
      case Format::VOP2: ctx.vop3_opcodes[ctx.opcode_encodings[i] + 0x100] = i; break;
      case Format::VOP3: ctx.vop3_opcodes[ctx.opcode_encodings[i]] = i; break;
      case Format::VOPC: ctx.vop3_opcodes[ctx.opcode_encodings[i]] = i; break;
      case Format::VINTRP: ctx.vop3_opcodes[ctx.opcode_encodings[i] + 0x270] = i; break;
      default: break;
      }
   }

   std::unordered_map<aco_opcode, op_rename> renames;
   for (uint32_t i = 0; i < ARRAY_SIZE(op_renames); i++) {
      if (op_renames[i].min_gfx_level > program->gfx_level)
         continue;

      if (renames.find(op_renames[i].op) != renames.end())
         if (op_renames[i].min_gfx_level < renames.at(op_renames[i].op).min_gfx_level)
            continue;

      renames[op_renames[i].op] = op_renames[i];
   }
   for (const auto& rename : renames)
      ctx.opcode_renames[rename.first] = rename.second.name;

   for (uint32_t i = 0; i < ARRAY_SIZE(encoding_infos); i++) {
      if (encoding_infos[i].min_gfx_level <= program->gfx_level &&
          encoding_infos[i].max_gfx_level >= program->gfx_level)
         ctx.encoding_infos.push_back(encoding_infos[i]);
   }
   auto compare = [](const encoding_info& a, const encoding_info& b)
   { return a.encoding_bitsize > b.encoding_bitsize; };
   std::sort(ctx.encoding_infos.begin(), ctx.encoding_infos.end(), compare);

   for (uint32_t i = 0; i < program->blocks.size(); i++) {
      if (ctx.referenced_blocks[i] &&
          ctx.block_offsets.find(program->blocks[i].offset) == ctx.block_offsets.end())
         ctx.block_offsets[program->blocks[i].offset] = i;
   }
}

/* Returns true on failure */
bool
disasm_program(Program* program, std::vector<uint32_t>& binary, uint32_t exec_size, char** string)
{
   size_t disasm_size = 0;
   struct u_memstream mem;
   if (!u_memstream_open(&mem, string, &disasm_size))
      return true;

   disasm_context ctx;
   disasm_context_init(ctx, program, u_memstream_get(&mem));

   size_t pos = 0;
   bool invalid = false;
   uint32_t next_block = 0;

   uint32_t prev_size = 0;
   uint32_t prev_pos = 0;
   uint32_t repeat_count = 0;
   while (pos <= exec_size) {
      bool new_block =
         next_block < program->blocks.size() && pos == program->blocks[next_block].offset;
      if (pos + prev_size <= exec_size && prev_pos != pos && !new_block &&
          memcmp(&binary[prev_pos], &binary[pos], prev_size * 4) == 0) {
         repeat_count++;
         pos += prev_size;
         continue;
      } else {
         if (repeat_count)
            fprintf(ctx.output, "\t(then repeated %u times)\n", repeat_count);
         repeat_count = 0;
      }

      print_block_markers(ctx, &next_block, pos);

      /* For empty last block, only print block marker. */
      if (pos == exec_size)
         break;

      fprintf(ctx.output, "\t");

      long start = ftell(ctx.output);
      size_t length = disasm_instr(ctx, binary.data() + pos, pos);
      long end = ftell(ctx.output);

      fprintf(ctx.output, " ");
      for (long i = end + 1; i < start + 60; i++)
         fprintf(ctx.output, " ");

      fprintf(ctx.output, ";");

      for (uint32_t i = 0; i < length; i++)
         fprintf(ctx.output, " %.8x", binary[pos + i]);
      fputc('\n', ctx.output);

      invalid |= !length;

      prev_size = length;
      prev_pos = pos;
      pos += length;
   }
   assert(next_block == program->blocks.size());

   print_constant_data(ctx);

   fputc(0, ctx.output);
   u_memstream_close(&mem);

   return invalid;
}

} // namespace aco
