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
zero = 'zero'

B = 'B'
UB = 'UB'
W = 'W'
UW = 'UW'
D = 'D'
UD = 'UD'
F = 'F'
result_type = 'VF'

gen6_md = [
    # Conversion instructions
    # b2[fi](inot(a)) maps a=0 => 1, a=-1 => 0.  Since a can only be 0 or -1,
    # this is float(1 + a).
    (('b2f32', ('inot', a)), Instruction('ADD', r, retype(a, D), imm(1, D))),
    (('b2i32', ('inot', a)), Instruction('ADD', r, retype(a, D), imm(1, D))),

    (('b2f32', a), Instruction('MOV', r, neg(retype(a, D)))),
    (('b2i32', a), Instruction('MOV', r, neg(retype(a, D)))),
    (('b2i16', a), Instruction('MOV', r, neg(retype(a, D)))),
    (('b2i8', a),  Instruction('MOV', r, neg(retype(a, D)))),

    (('f2f32', a), Instruction('MOV', r, a)),
    (('f2i32', a), Instruction('MOV', r, a)),
    (('f2u32', a), Instruction('MOV', r, a)),
    (('f2i16', a), Instruction('MOV', r, a)),
    (('f2u16', a), Instruction('MOV', r, a)),
    (('f2i8',  a), Instruction('MOV', r, a)),
    (('f2u8',  a), Instruction('MOV', r, a)),

    (('i2i32', a), Instruction('MOV', r, a)),

    (('i2f32', ('extract_i16', a, 0)), Instruction('MOV', r, subscript(a, W, 0))),
    (('i2f32', ('extract_i16', a, 1)), Instruction('MOV', r, subscript(a, W, 1))),
    (('i2f32', ('extract_i8', a, 0)), Instruction('MOV', r, subscript(a, B, 0))),
    (('i2f32', ('extract_i8', a, 1)), Instruction('MOV', r, subscript(a, B, 1))),
    (('i2f32', ('extract_i8', a, 2)), Instruction('MOV', r, subscript(a, B, 2))),
    (('i2f32', ('extract_i8', a, 3)), Instruction('MOV', r, subscript(a, B, 3))),
    (('i2f32', ('extract_u16', a, 0)), Instruction('MOV', r, subscript(a, UW, 0))),
    (('i2f32', ('extract_u16', a, 1)), Instruction('MOV', r, subscript(a, UW, 1))),
    (('i2f32', ('extract_u8', a, 0)), Instruction('MOV', r, subscript(a, UB, 0))),
    (('i2f32', ('extract_u8', a, 1)), Instruction('MOV', r, subscript(a, UB, 1))),
    (('i2f32', ('extract_u8', a, 2)), Instruction('MOV', r, subscript(a, UB, 2))),
    (('i2f32', ('extract_u8', a, 3)), Instruction('MOV', r, subscript(a, UB, 3))),
    (('i2f32', a), Instruction('MOV', r, a)),

    (('i2i16', a), Instruction('MOV', r, a)),
    (('i2i8', a),  Instruction('MOV', r, a)),

    (('u2f32', ('extract_i16', a, 0)), Instruction('MOV', r, subscript(a, W, 0))),
    (('u2f32', ('extract_i16', a, 1)), Instruction('MOV', r, subscript(a, W, 1))),
    (('u2f32', ('extract_i8', a, 0)), Instruction('MOV', r, subscript(a, B, 0))),
    (('u2f32', ('extract_i8', a, 1)), Instruction('MOV', r, subscript(a, B, 1))),
    (('u2f32', ('extract_i8', a, 2)), Instruction('MOV', r, subscript(a, B, 2))),
    (('u2f32', ('extract_i8', a, 3)), Instruction('MOV', r, subscript(a, B, 3))),
    (('u2f32', ('extract_u16', a, 0)), Instruction('MOV', r, subscript(a, UW, 0))),
    (('u2f32', ('extract_u16', a, 1)), Instruction('MOV', r, subscript(a, UW, 1))),
    (('u2f32', ('extract_u8', a, 0)), Instruction('MOV', r, subscript(a, UB, 0))),
    (('u2f32', ('extract_u8', a, 1)), Instruction('MOV', r, subscript(a, UB, 1))),
    (('u2f32', ('extract_u8', a, 2)), Instruction('MOV', r, subscript(a, UB, 2))),
    (('u2f32', ('extract_u8', a, 3)), Instruction('MOV', r, subscript(a, UB, 3))),
    (('u2f32', a), Instruction('MOV', r, a)),

    (('u2u32', a), Instruction('MOV', r, a)),
    (('u2u16', a), Instruction('MOV', r, a)),
    (('u2u8', a),  Instruction('MOV', r, a)),

    (('i2b32', 'a@32'), Instruction('CMP', r, retype(a, D), imm(0, D)).cmod('NZ')),
    (('i2b32', 'a@16'), Instruction('CMP', r, retype(a, W), imm(0, W)).cmod('NZ')),
    (('f2b32', 'a@32'), Instruction('CMP', retype(r, F), retype(a, F), imm(0.0, F)).cmod('NZ')),

    # Partial derivatives
    (('fddx', a), Instruction('FS_OPCODE_DDX_FINE', r, a), 'fs_key->high_quality_derivatives'),
    (('fddx', a), Instruction('FS_OPCODE_DDX_COARSE', r, a), '!fs_key->high_quality_derivatives'),
    (('fddx_fine', a), Instruction('FS_OPCODE_DDX_FINE', r, a)),
    (('fddx_coarse', a), Instruction('FS_OPCODE_DDX_COARSE', r, a)),
    (('fddy', a), Instruction('FS_OPCODE_DDY_FINE', r, a), 'fs_key->high_quality_derivatives'),
    (('fddy', a), Instruction('FS_OPCODE_DDY_COARSE', r, a), '!fs_key->high_quality_derivatives'),
    (('fddy_fine', a), Instruction('FS_OPCODE_DDY_FINE', r, a)),
    (('fddy_coarse', a), Instruction('FS_OPCODE_DDY_COARSE', r, a)),

    # General arithmetic
    (('fadd', a, b), Instruction('ADD', r, a, b)),
    (('iadd', a, b), Instruction('ADD', r, a, b)),
    (('uadd_sat', a, b), Instruction('ADD', r, a, b).saturate()),

    (('fmul', ('fsign(is_used_once)', 'a@32'), b),
     [Instruction('CMP', null(F), a, imm(0.0, F)).cmod('NZ'),
      Instruction('AND', retype(r, UD), retype(a, UD), imm(0x80000000, UD)),
      Instruction('XOR', retype(r, UD), retype(r, UD), retype(b, UD)).predicate()]
    ),

    (('fmul', a, b), Instruction('MUL', r, a, b)),

    (('imul', a, b), Instruction('MUL', r, a, b)),
    (('imul_high', a, b), Instruction('SHADER_OPCODE_MULH', r, a, b)),
    (('umul_high', a, b), Instruction('SHADER_OPCODE_MULH', r, a, b)),
    (('idiv', a, b), Instruction('SHADER_OPCODE_INT_QUOTIENT', r, a, b)),
    (('udiv', a, b), Instruction('SHADER_OPCODE_INT_QUOTIENT', r, a, b)),
    (('irem', a, b), Instruction('SHADER_OPCODE_INT_REMAINDER', r, a, b)),
    (('umod', a, b), Instruction('SHADER_OPCODE_INT_REMAINDER', r, a, b)),

    (('imod', a, b), [TempReg(t0, D),
                      # Get a regular C-style remainder.  If a % b == 0, set the predicate.
                      Instruction('SHADER_OPCODE_INT_REMAINDER', r, a, b),

                      # Math instructions don't support conditional mod
                      Instruction('MOV', null(D), r).cmod('NZ'),

                      # Now, we need to determine if signs of the sources are different.
                      # When we XOR the sources, the top bit is 0 if they are the same and 1
                      # if they are different.  We can then use a conditional modifier to
                      # turn that into a predicate.  This leads us to an XOR.l instruction.
                      #
                      # Technically, according to the PRM, you're not allowed to use .l on a
                      # XOR instruction.  However, emperical experiments and Curro's reading
                      # of the simulator source both indicate that it's safe.
                      Instruction('XOR', t0, a, b).predicate().cmod('L'),

                      # If the result of the initial remainder operation is non-zero and the
                      # two sources have different signs, add in a copy of op[1] to get the
                      # final integer modulus value.
                      Instruction('ADD', r, r, b).predicate()]
    ),
]

# Logic operations

inv_logic = {'iand': 'OR', 'ixor': 'XOR', 'ior': 'AND'}
for bits, T in (8, B), (16, W), (32, D):
    inot = 'inot@{}'.format(bits)

    for op in ('ior', 'iand'):
        gen6_md.extend([
            ((inot, (op, ('inot', 'a'),  ('inot', 'b'))),
             Instruction(inv_logic[op], retype(r, T), retype(    a , T), retype(    b , T))),
        ])

    gen6_md.extend([
        ((inot, ('ixor', ('inot', 'a'), 'b' )),
         Instruction('XOR', retype(r, T), retype(    a , T), retype(b, T))),
    ])

gen6_md.extend([((op, a, b), Instruction(op[1:].upper(), r, a , b)) for op in ('ixor', 'ior', 'iand')])

gen6_md.extend([
    # This has to be last or the optimized inot-of-logic patterns won't
    # trigger.
    (('inot', a), Instruction('NOT', r, a)),
])

# Comparisons
for op, cmod in (('feq32', 'Z'), ('fge32', 'GE'), ('flt32', 'L'), ('fne32', 'NZ')):
    gen6_md.extend([
        ((op, 'a@32', 'b@32'), Instruction('CMP', retype(r, F), a, b).cmod(cmod)),
    ])

for op, cmod in (('ieq32', 'Z'), ('ige32', 'GE'), ('ilt32', 'L'), ('ine32', 'NZ')):
    gen6_md.extend([
        ((op, 'a@32', 'b@32'), Instruction('CMP', retype(r, D), retype(a, D), retype(b, D)).cmod(cmod)),
        ((op, 'a@16', 'b@16'), [TempReg(t0, W),
                                Instruction('CMP', t0, retype(a, W), retype(b, W)).cmod(cmod),
                                Instruction('MOV', retype(r, D), retype(t0, W))]
        ),
        ((op, 'a@8',  'b@8' ), [TempReg(t0, B),
                                Instruction('CMP', t0, retype(a, B), retype(b, B)).cmod(cmod),
                                Instruction('MOV', retype(r, D), retype(t0, B))]
        ),
    ])

for op, cmod in (('uge32', 'GE'), ('ult32', 'L')):
    gen6_md.extend([
        ((op, 'a@32', 'b@32'), Instruction('CMP', retype(r, UD), retype(a, UD), retype(b, UD)).cmod(cmod)),
        ((op, 'a@16', 'b@16'), [TempReg(t0, UW),
                                Instruction('CMP', t0, retype(a, UW), retype(b, UW)).cmod(cmod),
                                Instruction('MOV', retype(r, D), retype(t0, W))]
        ),
        ((op, 'a@8',  'b@8' ), [TempReg(t0, UB),
                                Instruction('CMP', t0, retype(a, UB), retype(b, UB)).cmod(cmod),
                                Instruction('MOV', retype(r, D), retype(t0, B))]
        ),
    ])

gen6_md.extend([
    # 3-source arithmetic
    (('ffma', a, b, c), Instruction('MAD', r, c, b, a)),
    (('flrp', a, b, c), Instruction('LRP', r, c, b, a)),
])

# Trig / exponent / log / etc.
#
# Gen6 and earlier math box instructions cannot have source modifiers.  Emit
# explict resolve instructions here.
for op, gen_op in (('frcp',  'SHADER_OPCODE_RCP'),
                   ('fexp2', 'SHADER_OPCODE_EXP2'),
                   ('flog2', 'SHADER_OPCODE_LOG2'),
                   ('fsin',  'SHADER_OPCODE_SIN'),
                   ('fcos',  'SHADER_OPCODE_COS'),
                   ('fsqrt', 'SHADER_OPCODE_SQRT'),
                   ('frsq',  'SHADER_OPCODE_RSQ')):
    gen6_md.extend([
        ((op, a), Instruction(gen_op, r, a)),
    ])

gen6_md.extend([
    (('fpow', 'a(no_src_mod)',  'b(no_src_mod)'), Instruction('SHADER_OPCODE_POW', r, a, b)),
    (('fpow', 'a(any_src_mod)', 'b(no_src_mod)'),  [TempReg(t0, F),
                                                    Instruction('MOV', t0, a),
                                                    Instruction('SHADER_OPCODE_POW', r, t0, b)]
    ),
    (('fpow', 'a(no_src_mod)',  'b(any_src_mod)'), [TempReg(t0, F),
                                                    Instruction('MOV', t0, b),
                                                    Instruction('SHADER_OPCODE_POW', r, a, t0)]
    ),
    (('fpow', 'a(any_src_mod)', 'b(any_src_mod)'), [TempReg(t0, F),
                                                    TempReg(t1, F),
                                                    Instruction('MOV', t0, a),
                                                    Instruction('MOV', t1, b),
                                                    Instruction('SHADER_OPCODE_POW', r, t0, t1)]
    ),

    (('fsign', 'a@32'), [Instruction('CMP', null(F), a, imm(0.0, F)).cmod('NZ'),
                         Instruction('AND', retype(r, UD), retype(a, UD), imm(0x80000000, UD)),
                         Instruction('OR', retype(r, UD), retype(r, UD), imm(0x3f800000, UD)).predicate()]
    ),

    # Rounding
    (('ftrunc', a), Instruction('RNDZ', r, a)),
    (('fceil', a), [TempReg(t0, F),
                    Instruction('RNDD', t0, neg(a)),
                    Instruction('MOV', r, neg(t0))]
    ),
    (('ffloor', a), Instruction('RNDD', r, a)),
    (('ffract', a), Instruction('FRC', r, a)),
    (('fround_even', a), Instruction('RNDE', r, a)),

    # Min / max
    (('fmin', a, b), Instruction('SEL', r, a, b).cmod('L')),
    (('imin', a, b), Instruction('SEL', r, a, b).cmod('L')),
    (('umin', a, b), Instruction('SEL', r, a, b).cmod('L')),
    (('fmax', a, b), Instruction('SEL', r, a, b).cmod('GE')),
    (('imax', a, b), Instruction('SEL', r, a, b).cmod('GE')),
    (('umax', a, b), Instruction('SEL', r, a, b).cmod('GE')),

    # Packing / unpacking
    (('pack_32_2x16_split', a, b), Instruction('FS_OPCODE_PACK', r, a, b)),
    (('pack_half_2x16_split', a, b), Instruction('FS_OPCODE_PACK_HALF_2x16_SPLIT', r, a, b)),
    (('unpack_32_2x16_split_x', a), Instruction('MOV', r, subscript(a, UW, 0))),
    (('unpack_32_2x16_split_y', a), Instruction('MOV', r, subscript(a, UW, 1))),

    # LZD counts from the MSB side, while GLSL's ufind_MSB wants the count
    # from the LSB side. Subtract the result from 31 to convert the MSB count
    # into an LSB count.  If no bits are set, LZD will return 32.  31-32 = -1,
    # which is exactly what ufind_msb is supposed to return.
    (('ufind_msb', a), [Instruction('LZD', retype(r, UD), retype(a, UD)),
                        Instruction('ADD', r, neg(retype(r, D)), imm(31, D))]
    ),

    # LZD of an absolute value source almost always does the right thing.
    # There are two problem values:
    #
    # * 0x80000000.  Since abs(0x80000000) == 0x80000000, LZD returns 0.
    #   However, findMSB(int(0x80000000)) == 30.
    #
    # * 0xffffffff.  Since abs(0xffffffff) == 1, LZD returns 31.  Section 8.8
    #   (Integer Functions) of the GLSL 4.50 spec says:
    #
    #    For a value of zero or negative one, -1 will be returned.
    #
    # * Negative powers of two.  LZD(abs(-(1<<x))) returns x, but
    #   findMSB(-(1<<x)) should return x-1.
    #
    # For all negative number cases, including 0x80000000 and 0xffffffff, the
    # correct value is obtained from LZD if instead of negating the (already
    # negative) value the logical-not is used.  A conditonal logical-not can
    # be achieved in two instructions.
    #
    # The rest works just like the ufind_msb case.
    (('ifind_msb', a), [TempReg(t0, D),
                        Instruction('ASR', t0, a, imm(31, D)),
                        Instruction('XOR', t0, t0, a),
                        Instruction('LZD', retype(r, UD), retype(t0, UD)),
                        Instruction('ADD', r, neg(retype(r, D)), imm(31, D))]
    ),

    (('find_lsb', a), [TempReg(t0, D),

                       # (x & -x) generates a value that consists of only the
                       # LSB of x.  For all powers of 2, findMSB(y) ==
                       # findLSB(y).
                       Instruction('AND', t0, retype(a, D), neg(retype(a, D))),

                       # The rest works just like the ufind_msb case.
                       Instruction('LZD', retype(r, UD), retype(t0, UD)),
                       Instruction('ADD', r, neg(retype(r, D)), imm(31, D))]
    ),

    # Shifts
    (('ishl', a, b), Instruction('SHL', r, a, b)),
    (('ishr', a, b), Instruction('ASR', r, a, b)),
    (('ushr', a, b), Instruction('SHR', r, a, b)),
])

# Extract

for i in range(0, 4):
    gen6_md.extend([
        (('extract_u8', a, i), Instruction('MOV', r, subscript(a, UB, i))),
        (('extract_i8', a, i), Instruction('MOV', r, subscript(a,  B, i)))
    ])

for i in range(0, 2):
    gen6_md.extend([
        (('extract_u16', a, i), Instruction('MOV', r, subscript(a, UW, i))),
        (('extract_i16', a, i), Instruction('MOV', r, subscript(a,  W, i)))
    ])

gen6_md.extend([
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
    (('b32csel', a, b, c), [Instruction('CMP', null(D), a, imm(0, D)).cmod('NZ'),
                            Instruction('SEL', r, b, c).predicate()]
    ),

    (('fsat', a), Instruction('MOV', r, a).saturate()),
    (('fneg', a), Instruction('MOV', r, neg(a))),
    (('fabs', a), Instruction('MOV', r, abs(a))),
    (('ineg', a), Instruction('MOV', r, neg(a))),
    (('iabs', a), Instruction('MOV', r, abs(a))),
])

gen6_unsupported = [
    (['b2f16',
      'f2f16',
      'f2f16_rtne',
      'f2f16_rtz',
      'i2f16',
      'u2f16',],
     "FP16 not supported."),
    (['b2f64',
      'b2i64',
      'f2i64',
      'f2u64',
      'i2f64',
      'i2i64',
      'u2f64',
      'u2u64',
      'imul_2x32_64',
      'umul_2x32_64',
      'pack_64_2x32_split',
      'unpack_64_2x32_split_x',
      'unpack_64_2x32_split_y'],
     "Should have lowered 64-bit integer operations."),
    (['bitfield_reverse',
      'bit_count',
      'ubfe',
      'ibfe',
      'bfm',
      'bfi',
      'unpack_half_2x16_split_x',
      'unpack_half_2x16_split_y',],
     "Should have been lowered."),
    ('fquantize2f16', "No SPIR-V support."),
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

print(gen_gen_codegen.CodeGeneratorGenerator("gen6", gen6_md, gen6_unsupported).render())
