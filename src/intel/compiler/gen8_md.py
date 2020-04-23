#
# Copyright (C) 2019 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

import gen_gen_codegen
import genX_md
from math import ldexp
from gen_gen_codegen import retype, abs, grf, imm, neg, null, subscript, Instruction, InstructionList, TempReg

# Convenience variables
a = 'a'
b = 'b'
c = 'c'
d = 'd'
r = 'r'

t0 = 't0'
t1 = 't1'
t2 = 't2'
zero = 'zero'

B = 'B'
UB = 'UB'
W = 'W'
UW = 'UW'
D = 'D'
UD = 'UD'
Q = 'Q'
UQ = 'UQ'
F = 'F'
DF = 'DF'
HF = 'HF'
result_type = 'VF'

gen8_md = []

gen8_md.extend(genX_md.add_with_conversion)
gen8_md.extend(genX_md.conversions)

gen8_md.extend([
    (('b2f16', ('inot', a)), Instruction('ADD', r, retype(a, D), imm(1, D))),
    (('b2f16', a), Instruction('MOV', r, neg(retype(a, D)))),

    (('b2f64', a), Instruction('MOV', r, neg(retype(a, D)))),
    (('b2i64', a), Instruction('MOV', r, neg(retype(a, D)))),
])

gen8_md.extend([
    (('f2f64', a), Instruction('MOV', r, a)),
    (('f2i64', a), Instruction('MOV', r, a)),
    (('f2u64', a), Instruction('MOV', r, a)),
    (('f2f32', a), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(0, D)),
                    Instruction('MOV', r, a)],
     'nir_has_any_rounding_mode_rtne(execution_mode)'
    ),
    (('f2f32', a), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(3, D)),
                    Instruction('MOV', r, a)],
     'nir_has_any_rounding_mode_rtz(execution_mode)'
    ),
    (('f2f32', a), Instruction('MOV', r, a)),
    (('f2f16', a), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(0, D)),
                    Instruction('MOV', r, a)],
     'nir_has_any_rounding_mode_rtne(execution_mode)'
    ),
    (('f2f16', a), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(3, D)),
                    Instruction('MOV', r, a)],
     'nir_has_any_rounding_mode_rtz(execution_mode)'
    ),
    (('f2f16', a), Instruction('MOV', r, a)),
    (('f2f16_rtne', a), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(0, D)),
                         Instruction('MOV', r, a)]
    ),
    (('f2f16_rtz', a), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(3, D)),
                        Instruction('MOV', r, a)]
    ),

    # FINISHME: The original hand-coded version of this did (tmp16 there is t0
    # here):
    #
    #    /* The destination stride must be at least as big as the source stride. */
    #    tmp16.type = BRW_REGISTER_TYPE_W;
    #    tmp16.stride = 2;
    #
    # Without this, there's an extra move to zero the upper 16-bits of t0.  I
    # also tried putting retype(t0, W) in place of t0, but that prevents
    # copy-propagation from being able to do its job.
    (('fquantize2f16', a), [TempReg(t0, UD),
                            TempReg(t1, F),
                            TempReg(zero, F),
                            # Check for denormal
                            Instruction('CMP', null(F), abs(a), imm(ldexp(1.0, -14), F)).cmod('L'),
                            # Get the appropriately signed zero.
                            Instruction('AND', retype(zero, UD), retype(a, UD), imm(0x80000000, UD)),
                            # Do the actual F32 -> F16 -> F32 conversion
                            Instruction('F32TO16', t0, a),
                            Instruction('F16TO32', t1, t0),
                            # Select that or zero based on normal status
                            Instruction('SEL', r, zero, t1).predicate()]
    ),

    (('i2f64', a), Instruction('MOV', r, a)),
    (('i2i64', a), Instruction('MOV', r, a)),
    (('i2f16', a), Instruction('MOV', r, a)),

    (('u2f64', a), Instruction('MOV', r, a)),
    (('u2u64', a), Instruction('MOV', r, a)),
    (('u2f16', a), Instruction('MOV', r, a)),

    (('i2b32', 'a@64'), [TempReg(t0, Q),
                         TempReg(zero, Q),
                         Instruction('MOV', zero, imm(0, Q)),
                         Instruction('CMP', t0, a, zero).cmod('NZ'),
                         Instruction('MOV', r, subscript(t0, UD, 0))]
    ),
    (('i2b32', 'a@32'), Instruction('CMP', r, retype(a, D), imm(0, D)).cmod('NZ')),
    (('i2b32', 'a@16'), Instruction('CMP', r, retype(a, W), imm(0, W)).cmod('NZ')),
    (('f2b32', 'a@64'), [TempReg(t0, DF),
                         TempReg(zero, DF),
                         Instruction('MOV', zero, imm(0.0, DF)),
                         Instruction('CMP', t0, a, zero).cmod('NZ'),
                         Instruction('MOV', r, subscript(t0, UD, 0))]
    ),
    (('f2b32', 'a@32'), Instruction('CMP', retype(r, F), retype(a, F), imm(0.0, F)).cmod('NZ')),
    (('f2b32', 'a@16'), Instruction('CMP', retype(r, HF), retype(a, HF), imm(0.0, HF)).cmod('NZ')),
])

gen8_md.extend(genX_md.partial_derivatives)
gen8_md.extend(genX_md.fmul_fsign_optimizations(8))

gen8_md.extend([
    # General arithmetic
    (('fadd', a, b), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(0, D)),
                      Instruction('ADD', r, a, b)],
     'nir_has_any_rounding_mode_rtne(execution_mode)'
    ),
    (('fadd', a, b), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(3, D)),
                      Instruction('ADD', r, a, b)],
     'nir_has_any_rounding_mode_rtz(execution_mode)'
    ),
])

for bits, T in ((8, 'B'), (16, 'W'), (32, 'D')):
    aa = 'a@' + str(bits)
    bb = 'b@' + str(bits)
    UT = 'U' + T

    # AVG(x, y) - ((x ^ y) & 1)
    gen8_md.extend([
        (('ihadd', aa, bb),
         [
          TempReg(t0, T),
          Instruction('XOR', t0, a, b),
          Instruction('AND', t0, t0, imm(1, T)),
          Instruction('AVG', r, a, b),
          Instruction('ADD', r, r, neg(t0))
         ]),

        (('uhadd', aa, bb),
         [TempReg(t0, UT),
          Instruction('XOR', t0, a, b),
          Instruction('AND', t0, t0, imm(1, UT)),
          Instruction('AVG', r, a, b),
          Instruction('ADD', r, r, neg(t0))
         ]),
    ])

gen8_md.extend([
    (('fmul', a, b), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(0, D)),
                      Instruction('MUL', r, a, b)],
     'nir_has_any_rounding_mode_rtne(execution_mode)'
    ),
    (('fmul', a, b), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(3, D)),
                      Instruction('MUL', r, a, b)],
     'nir_has_any_rounding_mode_rtz(execution_mode)'
    ),

    # If b is NaN, this will produce 0.0 instead of NaN.
    (('~fmul', ('b2f(is_used_once)', a), 'b(is_not_const)'), Instruction('CSEL', r, a, b, a).cmod('Z')),

    (('imul_2x32_64', a, b), Instruction('MUL', r, a, b)),
    (('umul_2x32_64', a, b), Instruction('MUL', r, a, b)),
    (('imul_32x16', a, '#b'), [Instruction('MUL', r, a, retype(b, W))]),
    (('imul_32x16', a, b),    [Instruction('MUL', r, a, subscript(b, W, 0))]),
    (('umul_32x16', a, '#b'), [Instruction('MUL', r, a, retype(b, UW))]),
    (('umul_32x16', a, b),    [Instruction('MUL', r, a, subscript(b, UW, 0))]),
])

gen8_md.extend(genX_md.arithmetic)

# Logic operations

inv_logic = {'iand': 'OR', 'ixor': 'XOR', 'ior': 'AND'}
for bits, T in (8, B), (16, W), (32, D), (64, Q):
    inot = 'inot@{}'.format(bits)

    for op in ('ior', 'iand'):
        gen8_md.extend([
            ((inot, (op, ('inot', a),  ('inot', b))),
             Instruction(inv_logic[op], retype(r, T), retype(    a , T), retype(    b , T))),
            ((inot, (op, ('inot', a),           b)),
             Instruction(inv_logic[op], retype(r, T), retype(    a , T), retype(neg(b), T))),
            ((inot, (op,          a,            b)),
             Instruction(inv_logic[op], retype(r, T), retype(neg(a), T), retype(neg(b), T))),
        ])

    # Only need to invert one of the arguments to XOR.
    gen8_md.extend([
        ((inot, ('ixor', ('inot', a), ('inot', b))),
         Instruction('XOR', retype(r, T), retype(neg(a), T), retype(b, T))),
        ((inot, ('ixor', ('inot', a),          b )),
         Instruction('XOR', retype(r, T), retype(    a , T), retype(b, T))),
        ((inot, ('ixor',          a ,          b )),
         Instruction('XOR', retype(r, T), retype(neg(a), T), retype(b, T))),
    ])

for op in ('ixor', 'ior', 'iand'):
    # Gen8+ source modifiers do not mean negation or absolute value on logical
    # operations.  Instead, negation means logical-not.
    gen8_md.extend([
        ((op, ('inot', a),  ('inot', b)),
         Instruction(op[1:].upper(), r, neg(a), neg(b))),
        ((op, ('inot', a),           b ),
         Instruction(op[1:].upper(), r, neg(a),     b )),
        ((op,           a,           b ),
         Instruction(op[1:].upper(), r,     a ,     b )),
    ])

gen8_md.extend([
    # This has to be last or the optimized inot-of-logic patterns won't
    # trigger.
    (('inot', a), Instruction('NOT', r, a)),
])

# Comparisons

gen8_md.extend(genX_md.f32_comparison)

for op, cmod in (('feq32', 'Z'), ('fge32', 'GE'), ('flt32', 'L'), ('fne32', 'NZ')):
    gen8_md.extend([
        ((op, 'a@64', 'b@64'), [TempReg(t0, DF),
                                Instruction('CMP', t0, a, b).cmod(cmod),
                                Instruction('MOV', r, subscript(t0, UD, 0))]
        ),
        ((op, 'a@16', 'b@16'), [TempReg(t0, HF),
                                Instruction('CMP', t0, a, b).cmod(cmod),
                                Instruction('MOV', retype(r, D), retype(t0, W))]
        ),
    ])

gen8_md.extend(genX_md.integral_comparison)
gen8_md.extend(genX_md.integral64_comparison)

gen8_md.extend([
    # 3-source arithmetic
    (('ffma', a, b, c), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(0, D)),
                         Instruction('MAD', r, c, b, a)],
     'nir_has_any_rounding_mode_rtne(execution_mode)'
    ),
    (('ffma', a, b, c), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(3, D)),
                         Instruction('MAD', r, c, b, a)],
     'nir_has_any_rounding_mode_rtz(execution_mode)'
    ),
    (('ffma', a, b, c), Instruction('MAD', r, c, b, a)),
    (('flrp', a, b, c), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(0, D)),
                         Instruction('LRP', r, c, b, a)],
     'nir_has_any_rounding_mode_rtne(execution_mode)'
    ),
    (('flrp', a, b, c), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(3, D)),
                         Instruction('LRP', r, c, b, a)],
     'nir_has_any_rounding_mode_rtz(execution_mode)'
    ),
    (('flrp', a, b, c), Instruction('LRP', r, c, b, a)),

    # Trig / exponent / log / etc.
    (('frcp', a), Instruction('SHADER_OPCODE_RCP', r, a)),
    (('fpow', a, b), Instruction('SHADER_OPCODE_POW', r, a, b)),
    (('fexp2', a), Instruction('SHADER_OPCODE_EXP2', r, a)),
    (('flog2', a), Instruction('SHADER_OPCODE_LOG2', r, a)),
    (('fsin', a), Instruction('SHADER_OPCODE_SIN', r, a)),
    (('fcos', a), Instruction('SHADER_OPCODE_COS', r, a)),
    (('fsqrt', a), Instruction('SHADER_OPCODE_SQRT', r, a)),
    (('frsq', a), Instruction('SHADER_OPCODE_RSQ', r, a)),
])

gen8_md.extend(genX_md.fsign(8))
gen8_md.extend(genX_md.rounding)

gen8_md.extend([
    # Min / max
    (('fmin', a, b), Instruction('SEL', r, a, b).cmod('L')),
    (('imin', a, b), Instruction('SEL', r, a, b).cmod('L')),
    (('umin', a, b), Instruction('SEL', r, a, b).cmod('L')),
    (('fmax', a, b), Instruction('SEL', r, a, b).cmod('GE')),
    (('imax', a, b), Instruction('SEL', r, a, b).cmod('GE')),
    (('umax', a, b), Instruction('SEL', r, a, b).cmod('GE')),
])

gen8_md.extend(genX_md.pack_unpack)

gen8_md.extend([
    # Packing / unpacking
    (('pack_64_2x32_split', a, b), Instruction('FS_OPCODE_PACK', r, a, b)),
    (('unpack_64_2x32_split_x', a), Instruction('MOV', r, subscript(a, UD, 0))),
    (('unpack_64_2x32_split_y', a), Instruction('MOV', r, subscript(a, UD, 1))),
    (('unpack_half_2x16_split_x', a), Instruction('F16TO32', r, subscript(a, UW, 0))),
    (('unpack_half_2x16_split_x_flush_to_zero', a), Instruction('F16TO32', r, subscript(a, UW, 0))),
    (('unpack_half_2x16_split_y', a), Instruction('F16TO32', r, subscript(a, UW, 1))),
    (('unpack_half_2x16_split_y_flush_to_zero', a), Instruction('F16TO32', r, subscript(a, UW, 1))),

    # Bitfields
    (('bitfield_reverse', a), Instruction('BFREV', r, a)),
    (('bit_count', a), Instruction('CBIT', r, a)),

    # FBH counts from the MSB side, while ifind_msb wants the count from the
    # LSB side. If FBH didn't return an error (0xFFFFFFFF), then subtract the
    # result from 31 to convert the MSB count into an LSB count.
    (('ifind_msb', a), [Instruction('FBH', retype(r, UD), a),
                        Instruction('CMP', null(D), r, imm(-1, D)).cmod('NZ'),
                        Instruction('ADD', r, neg(r), imm(31, D)).predicate()]
    ),

    (('find_lsb', a), Instruction('FBL', r, a)),
    (('ubfe', a, b, c), Instruction('BFE', r, c, b, a)),
    (('ibfe', a, b, c), Instruction('BFE', r, c, b, a)),
    (('bfm', a, b), Instruction('BFI1', r, a, b)),
    (('bfi', a, b, c), Instruction('BFI2', r, a, b, c)),
])

gen8_md.extend(genX_md.misc_bit_manipulation)

# Extract
#
# The PRMs say:
#
#    BDW+
#
#    There is no direct conversion from B/UB to Q/UQ or Q/UQ to B/UB.  Use
#    two instructions and a word or DWord intermediate integer type.

for i in range(0, 4):
    gen8_md.extend([
        (('extract_u8@64', a, 2 * i + 0), Instruction('AND', r, subscript(a, UW, i), imm(0x00ff, UW))),
        (('extract_u8@64', a, 2 * i + 1), Instruction('SHR', r, subscript(a, UW, i), imm(8, UW))),
        (('extract_i8@64', a, 2 * i + 0), [TempReg(t0, W),
                                           Instruction('MOV', t0, subscript(a, B, 2 * i + 0)),
                                           Instruction('MOV', r, t0)]),
        (('extract_i8@64', a, 2 * i + 1), [TempReg(t0, W),
                                           Instruction('MOV', t0, subscript(a, B, 2 * i + 1)),
                                           Instruction('MOV', r, t0)]),
    ])

for i in range(0, 8):
    gen8_md.extend([
        (('extract_u8', a, i), Instruction('MOV', r, subscript(a, UB, i))),
        (('extract_i8', a, i), Instruction('MOV', r, subscript(a,  B, i))),
    ])

for i in range(0, 4):
    gen8_md.extend([
        (('extract_u16', a, i), Instruction('MOV', r, subscript(a, UW, i))),
        (('extract_i16', a, i), Instruction('MOV', r, subscript(a,  W, i))),
    ])

gen8_md.extend([
    # Select
    (('b32csel', 'a(front_face)', 1.0, -1.0), [TempReg(t0, D),
                                               Instruction('OR',
                                                           subscript(t0, W, 1),
                                                           retype(grf(0, 0, 1), W),
                                                           imm(0x3f80, W)),
                                               Instruction('AND', retype(r, UD), retype(t0, UD), imm(0xbf800000, UD))]
    ),
    (('b32csel', 'a(front_face)', -1.0, 1.0), [TempReg(t0, D),
                                               Instruction('OR', subscript(t0, W, 1), neg(retype(grf(0, 0, 1), W)), imm(0x3f80, W)),
                                               Instruction('AND', retype(r, UD), retype(t0, UD), imm(0xbf800000, UD))]
    ),

    # a < b ? b : a => b > a ? b : a.  Greater-than-or-equal is similar.  The
    # is_not_const is important because the first instruction source cannot be
    # a constant, and we don't want the builder to swap the sources.
    (('b32csel', ('flt32', 'a@32', 'b(is_not_const)'), b, a), Instruction('SEL', retype(r, F), b, a).cmod('G')),
    (('b32csel', ('fge32', 'a@32', 'b(is_not_const)'), b, a), Instruction('SEL', retype(r, F), b, a).cmod('LE')),

    (('b32csel', ('fne32', 'a@32', 0.0), 'b@32(is_not_const)', 'c@32(is_not_const)'), Instruction('CSEL', retype(r, F), retype(b, F), retype(c, F), retype(a, F)).cmod('NZ')),
    (('b32csel', ('feq32', 'a@32', 0.0), 'b@32(is_not_const)', 'c@32(is_not_const)'), Instruction('CSEL', retype(r, F), retype(b, F), retype(c, F), retype(a, F)).cmod('Z')),
    (('b32csel', ('flt32', 'a@32', 0.0), 'b@32(is_not_const)', 'c@32(is_not_const)'), Instruction('CSEL', retype(r, F), retype(b, F), retype(c, F), retype(a, F)).cmod('L')),
    (('b32csel', ('flt32', 0.0, 'a@32'), 'b@32(is_not_const)', 'c@32(is_not_const)'), Instruction('CSEL', retype(r, F), retype(b, F), retype(c, F), retype(a, F)).cmod('G')),
    (('b32csel', ('fge32', 'a@32', 0.0), 'b@32(is_not_const)', 'c@32(is_not_const)'), Instruction('CSEL', retype(r, F), retype(b, F), retype(c, F), retype(a, F)).cmod('GE')),
    (('b32csel', ('fge32', 0.0, 'a@32'), 'b@32(is_not_const)', 'c@32(is_not_const)'), Instruction('CSEL', retype(r, F), retype(b, F), retype(c, F), retype(a, F)).cmod('LE')),

    # These work because we know that the value being compared is ±0.  They
    # both should also be NaN safe.
    (('b32csel', ('fne32', 'a@32', 0.0), 'b@32(is_not_const)', 0.0), Instruction('CSEL', retype(r, F), retype(b, F), abs(retype(a, F)), retype(a, F)).cmod('NZ')),
    (('b32csel', ('feq32', 'a@32', 0.0), 0.0, 'b@32(is_not_const)'), Instruction('CSEL', retype(r, F), abs(retype(a, F)), retype(b, F), retype(a, F)).cmod('Z')),

    (('b32csel', a, b, c), [Instruction('CMP', null(D), a, imm(0, D)).cmod('NZ'),
                            Instruction('SEL', r, b, c).predicate()]
    ),
])

gen8_md.extend(genX_md.abs_neg_sat);

gen8_unsupported = [
    ('uadd_carry', "Should have been lowered by carry_to_arith()."),
    ('usub_borrow', "Should have been lowered by borrow_to_arith()."),
    (['fdot2',
      'fdot3',
      'fdot4',
      'b32all_fequal2',
      'b32all_iequal2',
      'b32all_fequal3',
      'b32all_iequal3',
      'b32all_fequal4',
      'b32all_iequal4',
      'b32any_fnequal2',
      'b32any_inequal2',
      'b32any_fnequal3',
      'b32any_inequal3',
      'b32any_fnequal4',
      'b32any_inequal4'],
     "Lowered by nir_lower_alu_reductions"),
    ('ldexp', "not reached: should be handled by ldexp_to_arith()"),
    (['pack_snorm_2x16',
      'pack_snorm_4x8',
      'pack_unorm_2x16',
      'pack_unorm_4x8',
      'unpack_snorm_2x16',
      'unpack_snorm_4x8',
      'unpack_unorm_2x16',
      'unpack_unorm_4x8',
      'unpack_half_2x16',
      'pack_half_2x16'],
     "not reached: should be handled by lower_packing_builtins"),
    (['ubitfield_extract',
      'ibitfield_extract'],
     "should have been lowered"),
    ('bitfield_insert', "not reached: should have been lowered"),
    (['urol', 'uror'], "Rotate not supported."),
]

print(gen_gen_codegen.CodeGeneratorGenerator("gen8", gen8_md, gen8_unsupported).render())
