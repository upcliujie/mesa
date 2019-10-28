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
t2 = 't2'
zero = 'zero'

B = 'B'
UB = 'UB'
W = 'W'
UW = 'UW'
D = 'D'
UD = 'UD'
F = 'F'
HF = 'HF'
result_type = 'VF'

gen12_md = [
    (('b2f32', a), Instruction('MOV', r, neg(retype(a, D)))),
    (('b2i32', a), Instruction('MOV', r, neg(retype(a, D)))),
    (('b2f16', a), Instruction('MOV', r, neg(retype(a, D)))),
    (('b2i16', a), Instruction('MOV', r, neg(retype(a, D)))),
    (('b2i8', a),  Instruction('MOV', r, neg(retype(a, D)))),

    (('f2f32', a), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(0, D)),
                    Instruction('MOV', r, a)],
     'nir_has_any_rounding_mode_rtne(execution_mode)'
    ),
    (('f2f32', a), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(3, D)),
                    Instruction('MOV', r, a)],
     'nir_has_any_rounding_mode_rtz(execution_mode)'
    ),
    (('f2f32', a), Instruction('MOV', r, a)),
    (('f2i32', a), Instruction('MOV', r, a)),
    (('f2u32', a), Instruction('MOV', r, a)),
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
    (('f2i16', a), Instruction('MOV', r, a)),
    (('f2u16', a), Instruction('MOV', r, a)),
    (('f2i8',  a), Instruction('MOV', r, a)),
    (('f2u8',  a), Instruction('MOV', r, a)),

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

    (('i2f16', a), Instruction('MOV', r, a)),
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
    (('u2f16', a), Instruction('MOV', r, a)),
    (('u2u16', a), Instruction('MOV', r, a)),
    (('u2u8', a),  Instruction('MOV', r, a)),

    (('i2b32', 'a@32'), Instruction('CMP', r, retype(a, D), imm(0, D)).cmod('NZ')),
    (('i2b32', 'a@16'), Instruction('CMP', r, retype(a, W), imm(0, W)).cmod('NZ')),
    (('f2b32', 'a@32'), Instruction('CMP', retype(r, F), retype(a, F), imm(0.0, F)).cmod('NZ')),
    (('f2b32', 'a@16'), Instruction('CMP', retype(r, HF), retype(a, HF), imm(0.0, HF)).cmod('NZ')),

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
    (('fadd', a, b), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(0, D)),
                      Instruction('ADD', r, a, b)],
     'nir_has_any_rounding_mode_rtne(execution_mode)'
    ),
    (('fadd', a, b), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(3, D)),
                      Instruction('ADD', r, a, b)],
     'nir_has_any_rounding_mode_rtz(execution_mode)'
    ),
    (('fadd', a, b), Instruction('ADD', r, a, b)),
    (('iadd', a, b), Instruction('ADD', r, a, b)),
    (('iadd_sat', a, b), Instruction('ADD', r, a, b).saturate()),
    (('uadd_sat', a, b), Instruction('ADD', r, a, b).saturate()),
    (('isub_sat', a, b), Instruction('SHADER_OPCODE_ISUB_SAT', r, a, b)),
    (('usub_sat', a, b), Instruction('SHADER_OPCODE_USUB_SAT', r, a, b)),
    (('irhadd', a, b), Instruction('AVG', r, a, b)),
    (('urhadd', a, b), Instruction('AVG', r, a, b)),
]

for bits, T in ((8, 'B'), (16, 'W'), (32, 'D')):
    aa = 'a@' + str(bits)
    bb = 'b@' + str(bits)
    UT = 'U' + T

    # AVG(x, y) - ((x ^ y) & 1)
    gen12_md.extend([
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

gen12_md.extend([
    (('fmul', ('fsign(is_used_once)', 'a@16'), b),
     [Instruction('CMP', null(F), a, imm(0.0, F)).cmod('NZ'),
      Instruction('AND', retype(r, UW), retype(a, UW), imm(0x8000, UW)),
      Instruction('XOR', retype(r, UW), retype(r, UW), retype(b, UW)).predicate()]
    ),
    (('fmul', ('fsign(is_used_once)', 'a@32'), b),
     [Instruction('CMP', null(F), a, imm(0.0, F)).cmod('NZ'),
      Instruction('AND', retype(r, UD), retype(a, UD), imm(0x80000000, UD)),
      Instruction('XOR', retype(r, UD), retype(r, UD), retype(b, UD)).predicate()]
    ),

    (('fmul', a, b), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(0, D)),
                      Instruction('MUL', r, a, b)],
     'nir_has_any_rounding_mode_rtne(execution_mode)'
    ),
    (('fmul', a, b), [Instruction('SHADER_OPCODE_RND_MODE', null(UD), imm(3, D)),
                      Instruction('MUL', r, a, b)],
     'nir_has_any_rounding_mode_rtz(execution_mode)'
    ),
    (('fmul', a, b), Instruction('MUL', r, a, b)),

    (('imul_32x16', a, '#b'), [Instruction('MUL', r, a, retype(b, W))]),
    (('imul_32x16', a, b),    [Instruction('MUL', r, a, subscript(b, W, 0))]),
    (('umul_32x16', a, '#b'), [Instruction('MUL', r, a, retype(b, UW))]),
    (('umul_32x16', a, b),    [Instruction('MUL', r, a, subscript(b, UW, 0))]),

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
])

# Logic operations

inv_logic = {'iand': 'OR', 'ixor': 'XOR', 'ior': 'AND'}
for bits, T in (8, B), (16, W), (32, D):
    inot = 'inot@{}'.format(bits)

    for op in ('ior', 'iand'):
        gen12_md.extend([
            ((inot, (op, ('inot', a),  ('inot', b))),
             Instruction(inv_logic[op], retype(r, T), retype(    a , T), retype(    b , T))),
            ((inot, (op, ('inot', a),           b)),
             Instruction(inv_logic[op], retype(r, T), retype(    a , T), retype(neg(b), T))),
            ((inot, (op,          a,            b)),
             Instruction(inv_logic[op], retype(r, T), retype(neg(a), T), retype(neg(b), T))),
        ])

    # Only need to invert one of the arguments to XOR.
    gen12_md.extend([
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
    gen12_md.extend([
        ((op, ('inot', a),  ('inot', b)),
         Instruction(op[1:].upper(), r, neg(a), neg(b))),
        ((op, ('inot', a),           b ),
         Instruction(op[1:].upper(), r, neg(a),     b )),
        ((op,           a,           b ),
         Instruction(op[1:].upper(), r,     a ,     b )),
    ])

gen12_md.extend([
    # This has to be last or the optimized inot-of-logic patterns won't
    # trigger.
    (('inot', a), Instruction('NOT', r, a)),
])

# Comparisons
for op, cmod in (('feq32', 'Z'), ('fge32', 'GE'), ('flt32', 'L'), ('fne32', 'NZ')):
    gen12_md.extend([
        ((op, 'a@32', 'b@32'), Instruction('CMP', retype(r, F), a, b).cmod(cmod)),
        ((op, 'a@16', 'b@16'), [TempReg(t0, HF),
                                Instruction('CMP', t0, a, b).cmod(cmod),
                                Instruction('MOV', retype(r, D), retype(t0, W))]
        ),
    ])

for op, cmod in (('ieq32', 'Z'), ('ige32', 'GE'), ('ilt32', 'L'), ('ine32', 'NZ')):
    gen12_md.extend([
        ((op, 'a@32', 'b@32'), Instruction('CMP', retype(r, D), retype(a, D), retype(b, D)).cmod(cmod)),
        ((op, 'a@16', 'b@16'), [TempReg(t0, W),
                                Instruction('CMP', t0, retype(a, W), retype(b, W)).cmod(cmod),
                                Instruction('MOV', retype(r, D), retype(t0, W))]
        ),
        ((op, 'a@8',  'b@8' ), [TempReg(t0, D), TempReg(t1, D),
                                Instruction('MOV', t0, a),
                                Instruction('MOV', t1, b),
                                Instruction('CMP', r, t0, t1).cmod(cmod)]
        ),
    ])

for op, cmod in (('uge32', 'GE'), ('ult32', 'L')):
    gen12_md.extend([
        ((op, 'a@32', 'b@32'), Instruction('CMP', retype(r, UD), retype(a, UD), retype(b, UD)).cmod(cmod)),
        ((op, 'a@16', 'b@16'), [TempReg(t0, UW),
                                Instruction('CMP', t0, retype(a, UW), retype(b, UW)).cmod(cmod),
                                Instruction('MOV', retype(r, D), retype(t0, W))]
        ),
        ((op, 'a@8',  'b@8' ), [TempReg(t0, UD), TempReg(t1, UD),
                                Instruction('MOV', t0, a),
                                Instruction('MOV', t1, b),
                                Instruction('CMP', r, t0, t1).cmod(cmod)]
        ),
    ])

gen12_md.extend([
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

    # Trig / exponent / log / etc.
    (('frcp', a), Instruction('SHADER_OPCODE_RCP', r, a)),
    (('fpow', a, b), Instruction('SHADER_OPCODE_POW', r, a, b)),
    (('fexp2', a), Instruction('SHADER_OPCODE_EXP2', r, a)),
    (('flog2', a), Instruction('SHADER_OPCODE_LOG2', r, a)),
    (('fsin', a), Instruction('SHADER_OPCODE_SIN', r, a)),
    (('fcos', a), Instruction('SHADER_OPCODE_COS', r, a)),
    (('fsqrt', a), Instruction('SHADER_OPCODE_SQRT', r, a)),
    (('frsq', a), Instruction('SHADER_OPCODE_RSQ', r, a)),

    (('fsign', 'a@16'), [Instruction('CMP', null(F), a, imm(0.0, F)).cmod('NZ'),
                         Instruction('AND', retype(r, UW), retype(a, UW), imm(0x8000, UW)),
                         Instruction('OR', retype(r, UW), retype(r, UW), imm(0x3c00, UW)).predicate()]
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
    (('imin', 'a@8', 'b@8'), [TempReg(t0, W), TempReg(t1, W),
                              Instruction('MOV', t0, b),
                              Instruction('SEL', t1, a, t0).cmod('L'),
                              Instruction('MOV', r, t1)]),
    (('imin', a, b), Instruction('SEL', r, a, b).cmod('L')),
    (('umin', 'a@8', 'b@8'), [TempReg(t0, UW), TempReg(t1, UW),
                              Instruction('MOV', t0, b),
                              Instruction('SEL', t1, a, t0).cmod('L'),
                              Instruction('MOV', r, t1)]),
    (('umin', a, b), Instruction('SEL', r, a, b).cmod('L')),
    (('fmax', a, b), Instruction('SEL', r, a, b).cmod('GE')),
    (('imax', 'a@8', 'b@8'), [TempReg(t0, W), TempReg(t1, W),
                              Instruction('MOV', t0, b),
                              Instruction('SEL', t1, a, t0).cmod('GE'),
                              Instruction('MOV', r, t1)]),
    (('imax', a, b), Instruction('SEL', r, a, b).cmod('GE')),
    (('umax', 'a@8', 'b@8'), [TempReg(t0, UW), TempReg(t1, UW),
                              Instruction('MOV', t0, b),
                              Instruction('SEL', t1, a, t0).cmod('GE'),
                              Instruction('MOV', r, t1)]),
    (('umax', a, b), Instruction('SEL', r, a, b).cmod('GE')),

    # Packing / unpacking
    (('pack_64_2x32_split', a, b), Instruction('FS_OPCODE_PACK', r, a, b)),
    (('pack_32_2x16_split', a, b), Instruction('FS_OPCODE_PACK', r, a, b)),
    (('pack_half_2x16_split', a, b), Instruction('FS_OPCODE_PACK_HALF_2x16_SPLIT', r, a, b)),
    (('unpack_64_2x32_split_x', a), Instruction('MOV', r, subscript(a, UD, 0))),
    (('unpack_64_2x32_split_y', a), Instruction('MOV', r, subscript(a, UD, 1))),
    (('unpack_32_2x16_split_x', a), Instruction('MOV', r, subscript(a, UW, 0))),
    (('unpack_32_2x16_split_y', a), Instruction('MOV', r, subscript(a, UW, 1))),
    (('unpack_half_2x16_split_x', a), Instruction('F16TO32', r, subscript(a, UW, 0))),
    (('unpack_half_2x16_split_x_flush_to_zero', a), Instruction('F16TO32', r, subscript(a, UW, 0))),
    (('unpack_half_2x16_split_y', a), Instruction('F16TO32', r, subscript(a, UW, 1))),
    (('unpack_half_2x16_split_y_flush_to_zero', a), Instruction('F16TO32', r, subscript(a, UW, 1))),

    # Bitfields
    (('bitfield_reverse', a), Instruction('BFREV', r, a)),
    (('bit_count', a), Instruction('CBIT', r, a)),

    # LZD counts from the MSB side, while GLSL's ufind_MSB wants the count
    # from the LSB side. Subtract the result from 31 to convert the MSB count
    # into an LSB count.  If no bits are set, LZD will return 32.  31-32 = -1,
    # which is exactly what ufind_msb is supposed to return.
    (('ufind_msb', a), [Instruction('LZD', retype(r, UD), retype(a, UD)),
                        Instruction('ADD', r, neg(retype(r, D)), imm(31, D))]
    ),

    (('uclz', a), Instruction('LZD', retype(r, UD), a)),

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

    # Shifts
    (('ishl', a, b), Instruction('SHL', r, a, b)),
    (('ishr', a, b), Instruction('ASR', r, a, b)),
    (('ushr', a, b), Instruction('SHR', r, a, b)),

    (('urol', a, b), Instruction('ROL', r, a, b)),
    (('uror', a, b), Instruction('ROR', r, a, b)),
])

for i in range(0, 4):
    gen12_md.extend([(('extract_u8', a, i), Instruction('MOV', r, subscript(a, UB, i)))])
    gen12_md.extend([(('extract_i8', a, i), Instruction('MOV', r, subscript(a,  B, i)))])

for i in range(0, 2):
    gen12_md.extend([(('extract_u16', a, i), Instruction('MOV', r, subscript(a, UW, i)))])
    gen12_md.extend([(('extract_i16', a, i), Instruction('MOV', r, subscript(a,  W, i)))])

gen12_md.extend([
    # Select
    (('b32csel', 'a(front_face)', 1.0, -1.0), [TempReg(t0, D),
                                               Instruction('OR', subscript(t0, W, 1), retype(grf(1, 1, 1), W), imm(0x3f80, W)),
                                               Instruction('AND', retype(r, UD), retype(t0, UD), imm(0xbf800000, UD))]
    ),
    (('b32csel', 'a(front_face)', -1.0, 1.0), [TempReg(t0, D),
                                               Instruction('OR', subscript(t0, W, 1), retype(grf(1, 1, 1), W), imm(0x3f80, W)),
                                               Instruction('MOV', t0, neg(t0)),
                                               Instruction('AND', retype(r, UD), retype(t0, UD), imm(0xbf800000, UD))]
    ),
    (('b32csel', a, 'b@8', 'c@8'), [TempReg(t0, W), TempReg(t1, W),
                                    Instruction('CMP', null(D), a, imm(0, D)).cmod('NZ'),
                                    Instruction('MOV', t0, c),
                                    Instruction('SEL', t1, b, t0).predicate(),
                                    Instruction('MOV', r, t1)]
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

gen12_unsupported = [
    (['b2f64',
      'b2i64',
      'f2f64',
      'f2i64',
      'f2u64',
      'i2f64',
      'i2i64',
      'u2f64',
      'u2u64',
      'imul_2x32_64',
      'umul_2x32_64'],
     "Should have lowered 64-bit operations."),
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
    ('flrp', "flrp instruction not supported."),
]

print(gen_gen_codegen.CodeGeneratorGenerator("gen12", gen12_md, gen12_unsupported).render())
