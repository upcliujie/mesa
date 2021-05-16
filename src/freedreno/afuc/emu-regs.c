/*
 * Copyright © 2021 Google, Inc.
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

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "emu.h"
#include "util.h"

/*
 * Emulator Registers:
 *
 * Handles access to GPR, GPU, control, and pipe registers.
 */

uint32_t
emu_get_control_reg(struct emu *emu, unsigned n)
{
   assert(n < ARRAY_SIZE(emu->control_regs.val));
   return emu->control_regs.val[n];
}

void
emu_set_control_reg(struct emu *emu, unsigned n, uint32_t val)
{
   assert(n < ARRAY_SIZE(emu->control_regs.val));
   BITSET_SET(emu->control_regs.written, n);
   emu->control_regs.val[n] = val;

   /* Some control regs have special action on write: */
   if (n == afuc_control_reg("PACKET_TABLE_WRITE")) {
      unsigned packet_table_write = afuc_control_reg("PACKET_TABLE_WRITE_ADDR");
      unsigned write_addr = emu_get_control_reg(emu, packet_table_write);

      assert(write_addr < ARRAY_SIZE(emu->jmptbl));
      emu->jmptbl[write_addr] = val;

      emu_set_control_reg(emu, packet_table_write, write_addr+1);
   } else if (n == afuc_control_reg("REG_WRITE")) {
      unsigned reg_write_addr = afuc_control_reg("REG_WRITE_ADDR");
      uint32_t write_addr = emu_get_control_reg(emu, reg_write_addr);

      /* Upper bits seem like some flags, not part of the actual
       * register offset.. not sure what they mean yet:
       */
      uint32_t flags = write_addr >> 16;
      write_addr &= 0xffff;

      emu_set_gpu_reg(emu, write_addr++, val);
      emu_set_control_reg(emu, reg_write_addr, write_addr | (flags << 16));
   }
}

static uint32_t
emu_get_pipe_reg(struct emu *emu, unsigned n)
{
   assert(n < ARRAY_SIZE(emu->pipe_regs.val));
   return emu->pipe_regs.val[n];
}

static void
emu_set_pipe_reg(struct emu *emu, unsigned n, uint32_t val)
{
   assert(n < ARRAY_SIZE(emu->pipe_regs.val));
   BITSET_SET(emu->pipe_regs.written, n);
   emu->pipe_regs.val[n] = val;

   /* Some pipe regs have special action on write: */
   if (n == afuc_pipe_reg("NRT_DATA")) {
      unsigned nrt_addr = afuc_pipe_reg("NRT_ADDR");

      uintptr_t addr = emu_get_pipe_reg(emu, nrt_addr + 1);
      addr <<= 32;
      addr |= emu_get_pipe_reg(emu, nrt_addr);

      emu_mem_write_dword(emu, addr, val);

      emu_set_pipe_reg(emu, nrt_addr + 1, (addr + 4) >> 32);
      emu_set_pipe_reg(emu, nrt_addr,     (addr + 4));
   }
}

static uint32_t
emu_get_gpu_reg(struct emu *emu, unsigned n)
{
   if (n >= ARRAY_SIZE(emu->gpu_regs.val))
      return 0;
   assert(n < ARRAY_SIZE(emu->gpu_regs.val));
   return emu->gpu_regs.val[n];
}

void
emu_set_gpu_reg(struct emu *emu, unsigned n, uint32_t val)
{
   if (n >= ARRAY_SIZE(emu->gpu_regs.val))
      return;
   assert(n < ARRAY_SIZE(emu->gpu_regs.val));
   BITSET_SET(emu->gpu_regs.written, n);
   emu->gpu_regs.val[n] = val;
}

static bool
is_pipe_reg_addr(unsigned regoff)
{
   return regoff > 0xffff;
}

static unsigned
get_reg_addr(struct emu *emu)
{
   switch (emu->data_mode) {
   case DATA_PIPE:
   case DATA_ADDR:    return REG_ADDR;
   case DATA_USRADDR: return REG_USRADDR;
   default:
      unreachable("bad data_mode");
      return 0;
   }
}

/* Handle reads for special streaming regs: */
static uint32_t
emu_get_fifo_reg(struct emu *emu, unsigned n)
{
   /* TODO the fifo regs are slurping out of a FIFO that the hw is filling
    * in parallel.. we can use `struct emu_queue` to emulate what is actually
    * happening more accurately
    */

   if (n == REG_MEMDATA) {
      /* $addr */
      unsigned mem_read_dwords = afuc_control_reg("MEM_READ_DWORDS");
      unsigned mem_read_addr   = afuc_control_reg("MEM_READ_ADDR");

      unsigned  read_dwords = emu_get_control_reg(emu, mem_read_dwords);
      uintptr_t read_addr   = emu_get_control_reg(emu, mem_read_addr + 1);
      read_addr <<= 32;
      read_addr |= emu_get_control_reg(emu, mem_read_addr);

      if (read_dwords > 0) {
         emu_set_control_reg(emu, mem_read_dwords, read_dwords - 1);
         emu_set_control_reg(emu, mem_read_addr+1, (read_addr + 4) >> 32);
         emu_set_control_reg(emu, mem_read_addr,   (read_addr + 4));

         uint32_t rem = emu_get_gpr_reg(emu, REG_REM);
         if (rem > 0)
            emu_set_gpr_reg(emu, REG_REM, --rem);
      }

      return emu_mem_read_dword(emu, read_addr);
   } else if (n == REG_REGDATA) {
      /* $regdata */
      unsigned reg_read_dwords = afuc_control_reg("REG_READ_DWORDS");
      unsigned reg_read_addr   = afuc_control_reg("REG_READ_ADDR");

      unsigned read_dwords = emu_get_control_reg(emu, reg_read_dwords);
      unsigned read_addr   = emu_get_control_reg(emu, reg_read_addr);

      /* I think if the fw doesn't write REG_READ_DWORDS before
       * REG_READ_ADDR, it just ends up with a single value written
       * into the FIFO that $regdata is consuming from:
       */
      if (read_dwords > 0) {
         emu_set_control_reg(emu, reg_read_dwords, read_dwords - 1);
         emu_set_control_reg(emu, reg_read_addr,   read_addr   + 1);

         uint32_t rem = emu_get_gpr_reg(emu, REG_REM);
         if (rem > 0)
            emu_set_gpr_reg(emu, REG_REM, --rem);
      }

      return emu_get_gpu_reg(emu, read_addr);
   } else if (n == REG_DATA) {
      /* $data */
      do {
         uint32_t rem = emu->gpr_regs.val[REG_REM];
         assert(rem >= 0);

         uint32_t val;
         if (emu_queue_pop(&emu->roq, &val)) {
            emu_set_gpr_reg(emu, REG_REM, --rem);
            return val;
         }

         /* If FIFO is empty, prompt for more input: */
         printf("FIFO empty, input a packet!\n");
         emu->run_mode = false;
         emu_main_prompt(emu);
      } while (true);
   } else {
      unreachable("not a FIFO reg");
      return 0;
   }
}

static void
emu_set_fifo_reg(struct emu *emu, unsigned n, uint32_t val)
{
   if ((n == REG_ADDR) || (n == REG_USRADDR)) {
      emu->data_mode = (n == REG_ADDR) ? DATA_ADDR : DATA_USRADDR;

      /* Treat these as normal register writes so we can see
       * updated values in the output as we step thru the
       * instructions:
       */
      emu->gpr_regs.val[n] = val;
      BITSET_SET(emu->gpr_regs.written, n);

      if (is_pipe_reg_addr(val)) {
         /* "void" pipe regs don't have a value to write, so just
          * treat it as writing zero to the pipe reg:
          */
         if (afuc_pipe_reg_is_void(val >> 24))
            emu_set_pipe_reg(emu, val >> 24, 0);
         emu->data_mode = DATA_PIPE;
      }
   } else if (n == REG_DATA) {
      unsigned reg = get_reg_addr(emu);
      unsigned regoff = emu->gpr_regs.val[reg];
      if (is_pipe_reg_addr(regoff)) {
         /* writes pipe registers: */

         assert(!(regoff & 0xfbffff));

         /* If b18 is set, don't auto-increment dest addr.. and if we
          * do auto-increment, we only increment the high 8b
          *
          * Note that we bypass emu_set_gpr_reg() in this case because
          * auto-incrementing doesn't reset needs_pipe_reg_flush.
          */
         if (!(regoff & 0x40000)) {
            emu->gpr_regs.val[reg] = regoff + 0x01000000;
            BITSET_SET(emu->gpr_regs.written, reg);
         }

         emu_set_pipe_reg(emu, regoff >> 24, val);
      } else {
         /* writes to gpu registers: */
         emu_set_gpr_reg(emu, reg, regoff+1);
         emu_set_gpu_reg(emu, regoff, val);
      }
   }
}

uint32_t
emu_get_gpr_reg(struct emu *emu, unsigned n)
{
   assert(n < ARRAY_SIZE(emu->gpr_regs.val));

   /* Handle special regs: */
   switch (n) {
   case 0x00:
      return 0;
   case REG_MEMDATA:
   case REG_REGDATA:
   case REG_DATA:
      return emu_get_fifo_reg(emu, n);
   default:
      return emu->gpr_regs.val[n];
   }
}

void
emu_set_gpr_reg(struct emu *emu, unsigned n, uint32_t val)
{
   assert(n < ARRAY_SIZE(emu->gpr_regs.val));

   switch (n) {
   case REG_ADDR:
   case REG_USRADDR:
   case REG_DATA:
      emu_set_fifo_reg(emu, n, val);
      break;
   default:
      emu->gpr_regs.val[n] = val;
      BITSET_SET(emu->gpr_regs.written, n);
      break;
   }
}
