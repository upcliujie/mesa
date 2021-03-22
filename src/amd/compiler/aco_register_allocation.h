/*
 * Copyright © 2021 Valve Corporation
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
 */

#ifndef ACO_REGISTER_ALLOCATION_H
#define ACO_REGISTER_ALLOCATION_H

#include "aco_ir.h"

namespace aco {
namespace{

/* minimum_stride, bytes_written */
std::pair<unsigned, unsigned> get_subdword_definition_info(Program *program, const aco_ptr<Instruction>& instr, RegClass rc)
{
   chip_class chip = program->chip_class;

   if (instr->isPseudo() && chip >= GFX8)
      return std::make_pair(rc.bytes() % 2 == 0 ? 2 : 1, rc.bytes());
   else if (instr->isPseudo())
      return std::make_pair(4, rc.size() * 4u);

   unsigned bytes_written = chip >= GFX10 ? rc.bytes() : 4u;
   switch (instr->opcode) {
   case aco_opcode::v_mad_f16:
   case aco_opcode::v_mad_u16:
   case aco_opcode::v_mad_i16:
   case aco_opcode::v_fma_f16:
   case aco_opcode::v_div_fixup_f16:
   case aco_opcode::v_interp_p2_f16:
      bytes_written = chip >= GFX9 ? rc.bytes() : 4u;
      break;
   default:
      break;
   }
   bytes_written = bytes_written > 4 ? align(bytes_written, 4) : bytes_written;
   bytes_written = MAX2(bytes_written, instr_info.definition_size[(int)instr->opcode] / 8u);

   if (can_use_SDWA(chip, instr)) {
      return std::make_pair(rc.bytes(), rc.bytes());
   } else if (rc.bytes() == 2 && can_use_opsel(chip, instr->opcode, -1, 1)) {
      return std::make_pair(2u, bytes_written);
   }

   switch (instr->opcode) {
   case aco_opcode::buffer_load_ubyte_d16:
   case aco_opcode::buffer_load_short_d16:
   case aco_opcode::flat_load_ubyte_d16:
   case aco_opcode::flat_load_short_d16:
   case aco_opcode::scratch_load_ubyte_d16:
   case aco_opcode::scratch_load_short_d16:
   case aco_opcode::global_load_ubyte_d16:
   case aco_opcode::global_load_short_d16:
   case aco_opcode::ds_read_u8_d16:
   case aco_opcode::ds_read_u16_d16:
      if (chip >= GFX9 && !program->dev.sram_ecc_enabled)
         return std::make_pair(2u, 2u);
      else
         return std::make_pair(2u, 4u);
   case aco_opcode::v_fma_mixlo_f16:
      return std::make_pair(2u, 2u);
   default:
      break;
   }

   return std::make_pair(4u, bytes_written);
}

unsigned get_subdword_operand_stride(chip_class chip, const aco_ptr<Instruction>& instr, unsigned idx, RegClass rc)
{
   /* v_readfirstlane_b32 cannot use SDWA */
   if (instr->opcode == aco_opcode::p_as_uniform)
      return 4;
   if (instr->isPseudo() && chip >= GFX8)
      return rc.bytes() % 2 == 0 ? 2 : 1;

   if (instr->opcode == aco_opcode::v_cvt_f32_ubyte0) {
      return 1;
   } else if (can_use_SDWA(chip, instr)) {
      return rc.bytes() % 2 == 0 ? 2 : 1;
   } else if (rc.bytes() == 2 && can_use_opsel(chip, instr->opcode, idx, 1)) {
      return 2;
   } else if (instr->isVOP3P()) {
      return 2;
   }

   switch (instr->opcode) {
   case aco_opcode::ds_write_b8:
   case aco_opcode::ds_write_b16:
      return chip >= GFX8 ? 2 : 4;
   case aco_opcode::buffer_store_byte:
   case aco_opcode::buffer_store_short:
   case aco_opcode::flat_store_byte:
   case aco_opcode::flat_store_short:
   case aco_opcode::scratch_store_byte:
   case aco_opcode::scratch_store_short:
   case aco_opcode::global_store_byte:
   case aco_opcode::global_store_short:
      return chip >= GFX9 ? 2 : 4;
   default:
      break;
   }

   return 4;
}
} /* end namespace */

/* Iterator type for making PhysRegInterval compatible with range-based for */
struct PhysRegIterator {
   using difference_type = int;
   using value_type = unsigned;
   using reference = const unsigned&;
   using pointer = const unsigned*;
   using iterator_category = std::bidirectional_iterator_tag;

   PhysReg reg;

   PhysReg operator*() const {
      return reg;
   }

   PhysRegIterator& operator++() {
      reg.reg_b += 4;
      return *this;
   }

   PhysRegIterator& operator--() {
      reg.reg_b -= 4;
      return *this;
   }

   bool operator==(PhysRegIterator oth) const {
      return reg == oth.reg;
   }

   bool operator!=(PhysRegIterator oth) const {
      return reg != oth.reg;
   }

   bool operator<(PhysRegIterator oth) const {
      return reg < oth.reg;
   }
};

/* Half-open register interval used in "sliding window"-style for-loops */
struct PhysRegInterval {
   PhysReg lo_;
   unsigned size;

   /* Inclusive lower bound */
   PhysReg lo() const {
      return lo_;
   }

   /* Exclusive upper bound */
   PhysReg hi() const {
      return PhysReg { lo() + size };
   }

   PhysRegInterval& operator+=(uint32_t stride) {
      lo_ = PhysReg { lo_.reg() + stride };
      return *this;
   }

   bool operator!=(const PhysRegInterval& oth) const {
      return lo_ != oth.lo_ || size != oth.size;
   }

   /* Construct a half-open interval, excluding the end register */
   static PhysRegInterval from_until(PhysReg first, PhysReg end) {
      return { first, end - first };
   }

   bool contains(PhysReg reg) const {
       return lo() <= reg && reg < hi();
   }

   bool contains(const PhysRegInterval& needle) const {
       return needle.lo() >= lo() && needle.hi() <= hi();
   }

   PhysRegIterator begin() const {
      return { lo_ };
   }

   PhysRegIterator end() const {
      return { PhysReg { lo_ + size } };
   }
};

bool intersects(const PhysRegInterval& a, const PhysRegInterval& b) {
   return ((a.lo() >= b.lo() && a.lo() < b.hi()) ||
           (a.hi() > b.lo() && a.hi() <= b.hi()));
}


/* Gets the stride for full (non-subdword) registers */
uint32_t get_stride(RegClass rc) {
    if (rc.type() == RegType::vgpr) {
        return 1;
    } else {
        uint32_t size = rc.size();
        if (size == 2) {
            return 2;
        } else if (size >= 4) {
            return 4;
        } else {
            return 1;
        }
    }
}

PhysRegInterval get_reg_bounds(Program* program, RegType type) {
   if (type == RegType::vgpr) {
      return { PhysReg { 256 }, (unsigned)program->max_reg_demand.vgpr };
   } else {
      return { PhysReg { 0 }, (unsigned)program->max_reg_demand.sgpr };
   }
}

struct assignment {
   PhysReg reg;
   RegClass rc;
   uint8_t assigned = 0;
   assignment() = default;
   assignment(PhysReg reg_, RegClass rc_) : reg(reg_), rc(rc_), assigned(-1) {}
};

struct phi_info {
   Instruction* phi;
   unsigned block_idx;
   std::set<Instruction*> uses;
};

struct ra_ctx {
   std::bitset<512> war_hint;
   Program* program;
   std::vector<assignment> assignments;
   std::vector<std::unordered_map<unsigned, Temp>> renames;
   std::vector<std::vector<Instruction*>> incomplete_phis;
   std::vector<bool> filled;
   std::vector<bool> sealed;
   std::unordered_map<unsigned, Temp> orig_names;
   std::unordered_map<unsigned, phi_info> phi_map;
   std::unordered_map<unsigned, unsigned> affinities;
   std::unordered_map<unsigned, Instruction*> vectors;
   std::unordered_map<unsigned, Instruction*> split_vectors;
   aco_ptr<Instruction> pseudo_dummy;
   uint16_t max_used_sgpr = 0;
   uint16_t max_used_vgpr = 0;
   uint16_t sgpr_limit;
   uint16_t vgpr_limit;
   std::bitset<64> defs_done; /* see MAX_ARGS in aco_instruction_selection_setup.cpp */

   ra_test_policy policy;

   ra_ctx(Program* program_, ra_test_policy policy_)
      : program(program_),
        assignments(program->peekAllocationId()),
        renames(program->blocks.size()),
        incomplete_phis(program->blocks.size()),
        filled(program->blocks.size()),
        sealed(program->blocks.size()),
        policy(policy_)
   {
      pseudo_dummy.reset(create_instruction<Instruction>(aco_opcode::p_parallelcopy, Format::PSEUDO, 0, 0));
      sgpr_limit = get_addr_sgpr_from_waves(program, program->min_waves);
      vgpr_limit = get_addr_sgpr_from_waves(program, program->min_waves);
   }
};

struct DefInfo {
   PhysRegInterval bounds;
   uint8_t size;
   uint8_t stride;
   RegClass rc;

   DefInfo(ra_ctx& ctx, aco_ptr<Instruction>& instr, RegClass rc_, int operand) : rc(rc_) {
      size = rc.size();
      stride = get_stride(rc);

      bounds = get_reg_bounds(ctx.program, rc.type());

      if (rc.is_subdword() && operand >= 0) {
         /* stride in bytes */
         stride = get_subdword_operand_stride(ctx.program->chip_class, instr, operand, rc);
      } else if (rc.is_subdword()) {
         std::pair<unsigned, unsigned> info = get_subdword_definition_info(ctx.program, instr, rc);
         stride = info.first;
         if (info.second > rc.bytes()) {
            rc = RegClass::get(rc.type(), info.second);
            size = rc.size();
            /* we might still be able to put the definition in the high half,
             * but that's only useful for affinities and this information isn't
             * used for them */
            stride = align(stride, info.second);
            if (!rc.is_subdword())
               stride = DIV_ROUND_UP(stride, 4);
         }
         assert(stride > 0);
      }
   }
};

class RegisterFile {
public:
   RegisterFile() {regs.fill(0);}

   std::array<uint32_t, 512> regs;
   std::map<uint32_t, std::array<uint32_t, 4>> subdword_regs;

   const uint32_t& operator [] (PhysReg index) const {
      return regs[index];
   }

   uint32_t& operator [] (PhysReg index) {
      return regs[index];
   }

   unsigned count_zero(PhysRegInterval reg_interval) {
      unsigned res = 0;
      for (PhysReg reg : reg_interval)
         res += !regs[reg];
      return res;
   }

   /* Returns true if any of the bytes in the given range are allocated or blocked */
   bool test(PhysReg start, unsigned num_bytes) {
      for (PhysReg i = start; i.reg_b < start.reg_b + num_bytes; i = PhysReg(i + 1)) {
         if (regs[i] & 0x0FFFFFFF)
            return true;
         if (regs[i] == 0xF0000000) {
            assert(subdword_regs.find(i) != subdword_regs.end());
            for (unsigned j = i.byte(); i * 4 + j < start.reg_b + num_bytes && j < 4; j++) {
               if (subdword_regs[i][j])
                  return true;
            }
         }
      }
      return false;
   }

   void block(PhysReg start, RegClass rc) {
      if (rc.is_subdword())
         fill_subdword(start, rc.bytes(), 0xFFFFFFFF);
      else
         fill(start, rc.size(), 0xFFFFFFFF);
   }

   bool is_blocked(PhysReg start) {
      if (regs[start] == 0xFFFFFFFF)
         return true;
      if (regs[start] == 0xF0000000) {
         for (unsigned i = start.byte(); i < 4; i++)
            if (subdword_regs[start][i] == 0xFFFFFFFF)
               return true;
      }
      return false;
   }

   bool is_empty_or_blocked(PhysReg start) {
      /* Empty is 0, blocked is 0xFFFFFFFF, so to check both we compare the
       * incremented value to 1 */
      if (regs[start] == 0xF0000000) {
         return subdword_regs[start][start.byte()] + 1 <= 1;
      }
      return regs[start] + 1 <= 1;
   }

   void clear(PhysReg start, RegClass rc) {
      if (rc.is_subdword())
         fill_subdword(start, rc.bytes(), 0);
      else
         fill(start, rc.size(), 0);
   }

   void fill(Operand op) {
      if (op.regClass().is_subdword())
         fill_subdword(op.physReg(), op.bytes(), op.tempId());
      else
         fill(op.physReg(), op.size(), op.tempId());
   }

   void clear(Operand op) {
      clear(op.physReg(), op.regClass());
   }

   void fill(Definition def) {
      if (def.regClass().is_subdword())
         fill_subdword(def.physReg(), def.bytes(), def.tempId());
      else
         fill(def.physReg(), def.size(), def.tempId());
   }

   void clear(Definition def) {
      clear(def.physReg(), def.regClass());
   }

   unsigned get_id(PhysReg reg) {
      return regs[reg] == 0xF0000000 ? subdword_regs[reg][reg.byte()] : regs[reg];
   }

private:
   void fill(PhysReg start, unsigned size, uint32_t val) {
      for (unsigned i = 0; i < size; i++)
         regs[start + i] = val;
   }

   void fill_subdword(PhysReg start, unsigned num_bytes, uint32_t val) {
      fill(start, DIV_ROUND_UP(num_bytes, 4), 0xF0000000);
      for (PhysReg i = start; i.reg_b < start.reg_b + num_bytes; i = PhysReg(i + 1)) {
         /* emplace or get */
         std::array<uint32_t, 4>& sub = subdword_regs.emplace(i, std::array<uint32_t, 4>{0, 0, 0, 0}).first->second;
         for (unsigned j = i.byte(); i * 4 + j < start.reg_b + num_bytes && j < 4; j++)
            sub[j] = val;

         if (sub == std::array<uint32_t, 4>{0, 0, 0, 0}) {
            subdword_regs.erase(i);
            regs[i] = 0;
         }
      }
   }
};

/* helper function for debugging */
void print_regs(ra_ctx& ctx, bool vgprs, RegisterFile& reg_file)
{
   unsigned max = vgprs ? ctx.program->max_reg_demand.vgpr : ctx.program->max_reg_demand.sgpr;
   PhysRegInterval regs { vgprs ? PhysReg{256} : PhysReg{0}, max };
   char reg_char = vgprs ? 'v' : 's';

   /* print markers */
   printf("       ");
   for (unsigned i = 0; i < regs.size; i += 3) {
      printf("%.2u ", i);
   }
   printf("\n");

   /* print usage */
   printf("%cgprs: ", reg_char);
   unsigned free_regs = 0;
   unsigned prev = 0;
   bool char_select = false;
   for (auto reg : regs) {
      if (reg_file[reg] == 0xFFFFFFFF) {
         printf("~");
      } else if (reg_file[reg]) {
         if (reg_file[reg] != prev) {
            prev = reg_file[reg];
            char_select = !char_select;
         }
         printf(char_select ? "#" : "@");
      } else {
         free_regs++;
         printf(".");
      }
   }
   printf("\n");

   printf("%u/%u used, %u/%u free\n", max - free_regs, max, free_regs, max);

   /* print assignments */
   prev = 0;
   unsigned size = 0;
   for (auto i : regs) {
      if (reg_file[i] != prev) {
         if (prev && size > 1)
            printf("-%d]\n", i - regs.lo() - 1);
         else if (prev)
            printf("]\n");
         prev = reg_file[i];
         if (prev && prev != 0xFFFFFFFF) {
            if (ctx.orig_names.count(reg_file[i]) && ctx.orig_names[reg_file[i]].id() != reg_file[i])
               printf("%%%u (was %%%d) = %c[%d", reg_file[i], ctx.orig_names[reg_file[i]].id(), reg_char, i - regs.lo());
            else
               printf("%%%u = %c[%d", reg_file[i], reg_char, i - regs.lo());
         }
         size = 1;
      } else {
         size++;
      }
   }
   if (prev && size > 1)
      printf("-%d]\n", regs.size - 1);
   else if (prev)
      printf("]\n");
}

void adjust_max_used_regs(ra_ctx& ctx, RegClass rc, unsigned reg)
{
   uint16_t max_addressible_sgpr = ctx.sgpr_limit;
   unsigned size = rc.size();
   if (rc.type() == RegType::vgpr) {
      assert(reg >= 256);
      uint16_t hi = reg - 256 + size - 1;
      ctx.max_used_vgpr = std::max(ctx.max_used_vgpr, hi);
   } else if (reg + rc.size() <= max_addressible_sgpr) {
      uint16_t hi = reg + size - 1;
      ctx.max_used_sgpr = std::max(ctx.max_used_sgpr, std::min(hi, max_addressible_sgpr));
   }
}

bool operand_can_use_reg(chip_class chip, aco_ptr<Instruction>& instr, unsigned idx, PhysReg reg, RegClass rc)
{
   if (instr->operands[idx].isFixed())
      return instr->operands[idx].physReg() == reg;

   bool is_writelane = instr->opcode == aco_opcode::v_writelane_b32 ||
                       instr->opcode == aco_opcode::v_writelane_b32_e64;
   if (chip <= GFX9 && is_writelane && idx <= 1) {
      /* v_writelane_b32 can take two sgprs but only if one is m0. */
      bool is_other_sgpr = instr->operands[!idx].isTemp() &&
                           (!instr->operands[!idx].isFixed() ||
                            instr->operands[!idx].physReg() != m0);
      if (is_other_sgpr && instr->operands[!idx].tempId() != instr->operands[idx].tempId()) {
         instr->operands[idx].setFixed(m0);
         return reg == m0;
      }
   }

   if (reg.byte()) {
      unsigned stride = get_subdword_operand_stride(chip, instr, idx, rc);
      if (reg.byte() % stride)
         return false;
   }

   switch (instr->format) {
   case Format::SMEM:
      return reg != scc &&
             reg != exec &&
             (reg != m0 || idx == 1 || idx == 3) && /* offset can be m0 */
             (reg != vcc || (instr->definitions.empty() && idx == 2) || chip >= GFX10); /* sdata can be vcc */
   default:
      // TODO: there are more instructions with restrictions on registers
      return true;
   }
}

bool get_reg_specified(ra_ctx& ctx,
                       RegisterFile& reg_file,
                       RegClass rc,
                       aco_ptr<Instruction>& instr,
                       PhysReg reg)
{
   std::pair<unsigned, unsigned> sdw_def_info;
   if (rc.is_subdword())
      sdw_def_info = get_subdword_definition_info(ctx.program, instr, rc);

   if (rc.is_subdword() && reg.byte() % sdw_def_info.first)
      return false;
   if (!rc.is_subdword() && reg.byte())
      return false;

   if (rc.type() == RegType::sgpr && reg % get_stride(rc) != 0)
         return false;

   PhysRegInterval reg_win = { reg, rc.size() };
   PhysRegInterval bounds = get_reg_bounds(ctx.program, rc.type());
   PhysRegInterval vcc_win = { vcc, 2 };
   /* VCC is outside the bounds */
   bool is_vcc = rc.type() == RegType::sgpr && vcc_win.contains(reg_win);
   if (!bounds.contains(reg_win) && !is_vcc)
      return false;

   if (rc.is_subdword()) {
      PhysReg test_reg;
      test_reg.reg_b = reg.reg_b & ~(sdw_def_info.second - 1);
      if (reg_file.test(test_reg, sdw_def_info.second))
         return false;
   } else {
      if (reg_file.test(reg, rc.bytes()))
         return false;
   }

   adjust_max_used_regs(ctx, rc, reg_win.lo());
   return true;
}
}
#endif /* ACO_REGISTER_ALLOCATION_H */
