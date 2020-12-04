/*
 * Copyright © 2020 Valve Corporation
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
 */

#include "helpers.h"

using namespace aco;

BEGIN_TEST(optimizer_postRA.vcmp)
    //>> v1: %a, s2: %x:exec = p_startpgm
    ASSERTED bool setup_ok = setup_cs("v1", GFX6);
    assert(setup_ok);

    Temp v_in = inputs[0];

    {
        /* Recognize when the result of VOPC goes to VCC, and use that for the branching then. */

        //! s2: %b:vcc = v_cmp_eq_u32 0, %a
        //! s2: %e = p_cbranch_z %b:vcc
        //! p_unit_test 0, %e
        auto vcmp = bld.vopc(aco_opcode::v_cmp_eq_u32, bld.vcc(bld.def(bld.lm)), Operand(0u), v_in);
        auto sand = bld.sop2(Builder::s_and, bld.def(bld.lm), bld.def(s1, scc), bld.vcc(vcmp), bld.exec(exec_input));
        auto br = bld.branch(aco_opcode::p_cbranch_z, bld.hint_vcc(bld.def(s2)), bld.scc(sand.def(1).getTemp()));
        writeout(0, br);
    }

    //; del b, e

    {
        /* When the result of VOPC goes to an SGPR pair other than VCC, don't optimize */

        //! s2: %b = v_cmp_eq_u32 0, %a
        //! s2: %c, s1: %d:scc = s_and_b64 %b, %x:exec
        //! s2: %e = p_cbranch_z %d:scc
        //! p_unit_test 1, %e
        auto vcmp = bld.vopc(aco_opcode::v_cmp_eq_u32, bld.def(bld.lm), Operand(0u), v_in);
        auto sand = bld.sop2(Builder::s_and, bld.def(bld.lm), bld.def(s1, scc), vcmp, bld.exec(exec_input));
        auto br = bld.branch(aco_opcode::p_cbranch_z, bld.hint_vcc(bld.def(s2)), bld.scc(sand.def(1).getTemp()));
        writeout(1, br);
    }

    finish_optimizer_postRA_test();
END_TEST
