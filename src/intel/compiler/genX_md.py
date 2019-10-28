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
Q = 'Q'
UQ = 'UQ'
F = 'F'
DF = 'DF'
HF = 'HF'
result_type = 'VF'

add_with_conversion = [
    # Conversion instructions
    # b2[fi](inot(a)) maps a=0 => 1, a=-1 => 0.  Since a can only be 0 or -1,
    # this is float(1 + a).
    (('b2f32', ('inot', a)), Instruction('ADD', r, retype(a, D), imm(1, D))),
    (('b2i32', ('inot', a)), Instruction('ADD', r, retype(a, D), imm(1, D))),
]

conversions = [
    (('b2f32', a), Instruction('MOV', r, neg(retype(a, D)))),
    (('b2i32', a), Instruction('MOV', r, neg(retype(a, D)))),
    (('b2i16', a), Instruction('MOV', r, neg(retype(a, D)))),
    (('b2i8', a),  Instruction('MOV', r, neg(retype(a, D)))),
    (('f2i32', a), Instruction('MOV', r, a)),
    (('f2u32', a), Instruction('MOV', r, a)),
    (('f2i16', a), Instruction('MOV', r, a)),
    (('f2u16', a), Instruction('MOV', r, a)),
    (('f2i8',  a), Instruction('MOV', r, a)),
    (('f2u8',  a), Instruction('MOV', r, a)),
    # Don't need f2f32 because in the "all platforms" case, there is only f32.

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
]

partial_derivatives = [
    (('fddx', a), Instruction('FS_OPCODE_DDX_FINE', r, a), 'fs_key->high_quality_derivatives'),
    (('fddx', a), Instruction('FS_OPCODE_DDX_COARSE', r, a), '!fs_key->high_quality_derivatives'),
    (('fddx_fine', a), Instruction('FS_OPCODE_DDX_FINE', r, a)),
    (('fddx_coarse', a), Instruction('FS_OPCODE_DDX_COARSE', r, a)),
    (('fddy', a), Instruction('FS_OPCODE_DDY_FINE', r, a), 'fs_key->high_quality_derivatives'),
    (('fddy', a), Instruction('FS_OPCODE_DDY_COARSE', r, a), '!fs_key->high_quality_derivatives'),
    (('fddy_fine', a), Instruction('FS_OPCODE_DDY_FINE', r, a)),
    (('fddy_coarse', a), Instruction('FS_OPCODE_DDY_COARSE', r, a)),
]

fmul_fsign_optimizations = [
    (('fmul', ('fsign(is_used_once)', 'a@32'), b),
     [Instruction('CMP', null(F), a, imm(0.0, F)).cmod('NZ'),
      Instruction('AND', retype(r, UD), retype(a, UD), imm(0x80000000, UD)),
      Instruction('XOR', retype(r, UD), retype(r, UD), retype(b, UD)).predicate()]
    ),
]

arithmetic = [
    (('fadd', a, b), Instruction('ADD', r, a, b)),
    (('iadd', a, b), Instruction('ADD', r, a, b)),
    (('iadd_sat', a, b), Instruction('ADD', r, a, b).saturate()),
    (('uadd_sat', a, b), Instruction('ADD', r, a, b).saturate()),
    (('isub_sat', a, b), Instruction('SHADER_OPCODE_ISUB_SAT', r, a, b)),
    (('usub_sat', a, b), Instruction('SHADER_OPCODE_USUB_SAT', r, a, b)),
    (('irhadd', a, b), Instruction('AVG', r, a, b)),
    (('urhadd', a, b), Instruction('AVG', r, a, b)),

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

f32_comparison = []
for op, cmod in (('feq32', 'Z'), ('fge32', 'GE'), ('flt32', 'L'), ('fne32', 'NZ')):
    f32_comparison.extend([
        ((op, 'a@32', 'b@32'), Instruction('CMP', retype(r, F), a, b).cmod(cmod)),
    ])

integral_comparison = []
for op, cmod in (('ieq32', 'Z'), ('ige32', 'GE'), ('ilt32', 'L'), ('ine32', 'NZ')):
    integral_comparison.extend([
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
    integral_comparison.extend([
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

integral64_comparison = []
for op, cmod in (('ieq32', 'Z'), ('ige32', 'GE'), ('ilt32', 'L'), ('ine32', 'NZ')):
    integral64_comparison.extend([
        ((op, 'a@64', 'b@64'), [TempReg(t0, Q),
                                Instruction('CMP', t0, retype(a, Q), retype(b, Q)).cmod(cmod),
                                Instruction('MOV', r, subscript(t0, UD, 0))]
        ),
    ])

for op, cmod in (('uge32', 'GE'), ('ult32', 'L')):
    integral64_comparison.extend([
        ((op, 'a@64', 'b@64'), [TempReg(t0, UQ),
                                Instruction('CMP', t0, retype(a, UQ), retype(b, UQ)).cmod(cmod),
                                Instruction('MOV', r, subscript(t0, UD, 0))]
        ),
    ])

fsign = [
    (('fsign', 'a@32'), [Instruction('CMP', null(F), a, imm(0.0, F)).cmod('NZ'),
                         Instruction('AND', retype(r, UD), retype(a, UD), imm(0x80000000, UD)),
                         Instruction('OR', retype(r, UD), retype(r, UD), imm(0x3f800000, UD)).predicate()]
    ),
]

rounding = [
    (('ftrunc', a), Instruction('RNDZ', r, a)),
    (('fceil', a), [TempReg(t0, F),
                    Instruction('RNDD', t0, neg(a)),
                    Instruction('MOV', r, neg(t0))]
    ),
    (('ffloor', a), Instruction('RNDD', r, a)),
    (('ffract', a), Instruction('FRC', r, a)),
    (('fround_even', a), Instruction('RNDE', r, a)),
]

pack_unpack = [
    (('pack_32_2x16_split', a, b), Instruction('FS_OPCODE_PACK', r, a, b)),
    (('pack_half_2x16_split', a, b), Instruction('FS_OPCODE_PACK_HALF_2x16_SPLIT', r, a, b)),
    (('unpack_32_2x16_split_x', a), Instruction('MOV', r, subscript(a, UW, 0))),
    (('unpack_32_2x16_split_y', a), Instruction('MOV', r, subscript(a, UW, 1))),
]

misc_bit_manipulation = [
    # LZD counts from the MSB side, while GLSL's ufind_MSB wants the count
    # from the LSB side. Subtract the result from 31 to convert the MSB count
    # into an LSB count.  If no bits are set, LZD will return 32.  31-32 = -1,
    # which is exactly what ufind_msb is supposed to return.
    (('ufind_msb', a), [Instruction('LZD', retype(r, UD), retype(a, UD)),
                        Instruction('ADD', r, neg(retype(r, D)), imm(31, D))]
    ),

    (('uclz', a), Instruction('LZD', retype(r, UD), a)),

    # Shifts
    (('ishl', a, b), Instruction('SHL', r, a, b)),
    (('ishr', a, b), Instruction('ASR', r, a, b)),
    (('ushr', a, b), Instruction('SHR', r, a, b)),
]

extract = []
for i in range(0, 4):
    extract.extend([(('extract_u8', a, i), Instruction('MOV', r, subscript(a, UB, i)))])
    extract.extend([(('extract_i8', a, i), Instruction('MOV', r, subscript(a,  B, i)))])

for i in range(0, 2):
    extract.extend([(('extract_u16', a, i), Instruction('MOV', r, subscript(a, UW, i)))])
    extract.extend([(('extract_i16', a, i), Instruction('MOV', r, subscript(a,  W, i)))])

abs_neg_sat = [
    (('fsat', a), Instruction('MOV', r, a).saturate()),
    (('fneg', a), Instruction('MOV', r, neg(a))),
    (('fabs', a), Instruction('MOV', r, abs(a))),
    (('ineg', a), Instruction('MOV', r, neg(a))),
    (('iabs', a), Instruction('MOV', r, abs(a))),
]
