#
# Copyright © 2020 Google, Inc.
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

from mako.template import Template
from isa import ISA, BitSetDerivedField, BitSetAssertField
import sys
import re

# Encoding is driven by the display template that would be used
# to decode any given instruction, essentially working backwards
# from the decode case.  (Or put another way, the decoded bitset
# should contain enough information to re-encode it again.)
#
# In the xml, we can have multiple override cases per bitset,
# which can override display template and/or fields.  Iterating
# all this from within the template is messy, so use helpers
# outside of the template for this.

# Represents a concrete field, ie. a field can be overriden
# by an override, so the exact choice to encode a given field
# in a bitset may be conditional
class FieldCase(object):
    def __init__(self, field, case):
        self.field = field
        self.expr  = None
        if case.expr is not None:
            self.expr = isa.expressions[case.expr]

class AssertCase(object):
    def __init__(self, field, case):
        self.field = field
        self.expr  = None
        if case.expr is not None:
            self.expr = isa.expressions[case.expr]

# Represents a field to be encoded:
class Field(object):
    def __init__(self, bitset, name):
        self.bitset = bitset   # leaf bitset
        self.name = name

    def assert_cases(self, bitset=None):
        if bitset is None:
            bitset = self.bitset
        for case in bitset.cases:
            for name, field in case.fields.items():
                if field.get_c_typename() == 'TYPE_ASSERT':
                    yield AssertCase(field, case)
        if bitset.extends is not None:
            yield from self.assert_cases(isa.bitsets[bitset.extends])

    def field_cases(self, bitset=None):
        if bitset is None:
            bitset = self.bitset
        # resolving the various cases for encoding a given
        # field is similar to resolving the display template
        # string
        for case in bitset.cases:
            if self.name in case.fields:
                field = case.fields[self.name]
                # For bitset fields, the bitset type could reference
                # fields in this (the containing) bitset, in addition
                # to the ones which are directly used to encode the
                # field itself.
                if field.get_c_typename() == 'TYPE_BITSET':
                    # TODO so far we haven't *required* remaps to
                    # allow a bitset field to access fields in the
                    # bitset that contains it.  Possibly we should
                    # require that, since (a) it makes this logic
                    # actually work, and (b) it does make things
                    # a bit more clear
                    for remap in field.remaps:
                        yield from Field(self.bitset, remap[0]).field_cases()
                # For derived fields, we want to consider any other
                # fields that are referenced by the expr
                if isinstance(field, BitSetDerivedField):
                    expr = bitset.isa.expressions[field.expr]
                    for instr in expr.instructions:
                        if instr[0] == 'VAR':
                            yield from Field(self.bitset, instr[1]).field_cases()
                # TODO for assert fields, we probably want to OR in the
                # assert pattern
                elif not isinstance(field, BitSetAssertField):
                    yield FieldCase(field, case)
                # if we've found an unconditional case specifying
                # the named field, we are done
                if case.expr is None:
                    return
        if bitset.extends is not None:
            yield from self.field_cases(isa.bitsets[bitset.extends])

# Represents an if/else case in bitset encoding:
class Case(object):
    def __init__(self, bitset, case):
        self.bitset = bitset   # leaf bitset
        self.case = case
        self.expr = None
        if case.expr is not None:
            self.expr = isa.expressions[case.expr]

    def fields(self):
        fieldnames = re.findall(r"{([a-zA-Z0-9_]+)}", self.case.display)
        for fieldname in fieldnames:
            yield Field(self.bitset, fieldname)

# State and helpers used by the template:
class State(object):
    def __init__(self, isa):
        self.isa = isa

    def bitset_cases(self, bitset, leaf_bitset=None):
        if leaf_bitset is None:
            leaf_bitset = bitset;
        for case in bitset.cases:
            if case.display is None:
                # if this is the last case (ie. case.expr is None)
                # then we need to go up the inheritance chain:
                if case.expr is None and bitset.extends is not None:
                    parent_bitset = isa.bitsets[bitset.extends]
                    yield from self.bitset_cases(parent_bitset, leaf_bitset)
                continue;
            yield Case(leaf_bitset, case)

    def case_name(self, bitset, name):
       return bitset.encode.case_prefix + name.upper().replace('.', '_').replace('-', '_').replace('#', '')

    def encode_roots(self):
       for name, root in self.isa.roots.items():
          if root.encode is None:
             continue
          yield root

    def encode_leafs(self, root):
       for name, leaf in self.isa.leafs.items():
          if leaf.get_root() != root:
             continue
          yield leaf

    # expressions used in a bitset (case or field or recursively parent bitsets)
    def bitset_used_exprs(self, bitset):
       for case in bitset.cases:
          if case.expr:
             yield self.isa.expressions[case.expr]
          # TODO I think we don't need expressions for derived fields, do we?
       if bitset.extends is not None:
          yield from self.bitset_used_exprs(self.isa.bitsets[bitset.extends])

    def extractor(self, bitset, name):
        if bitset.encode is not None:
            if name in bitset.encode.maps:
                return bitset.encode.maps[name]
        if bitset.extends is not None:
            return self.extractor(self.isa.bitsets[bitset.extends], name)
        # Default fallback when no mapping is defined, simply to avoid
        # having to deal with encoding at the same time as r/e new
        # instruction decoding
        return '0 /* XXX */'

    def encode_type(self, bitset):
        if bitset.encode is not None:
            if bitset.encode.type is not None:
                return bitset.encode.type
        if bitset.extends is not None:
            return self.encode_type(self.isa.bitsets[bitset.extends])
        return None

    def expr_name(self, root, expr):
       return root.get_c_name() + '_' + expr.get_c_name()

template = """\
/* Copyright (C) 2020 Google, Inc.
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

#include <stdbool.h>
#include <stdint.h>

<%
isa = s.isa
%>

/**
 * Opaque type from the PoV of generated code, but allows state to be passed
 * thru to the hand written helpers used by the generated code.
 */
struct encode_state;

static uint64_t
pack_field(unsigned low, unsigned high, uint64_t val)
{
   val &= ((1ul << (1 + high - low)) - 1);
   return val << low;
}

/*
 * Forward-declarations (so we don't have to figure out which order to
 * emit various encoders when they have reference each other)
 */

%for root in s.encode_roots():
static uint64_t encode${root.get_c_name()}(struct encode_state *s, ${root.encode.type} src);
%endfor

## TODO before the expr evaluators, we should generate extract_FOO() for
## derived fields.. which probably also need to be in the context of the
## respective root so they take the correct src arg??

/*
 * Expression evaluators:
 */

#define push(v) do { \
            assert(sp < ARRAY_SIZE(stack)); \
            stack[sp] = (v); \
            sp++; \
        } while (0)
#define peek() ({ \
            assert(sp < ARRAY_SIZE(stack)); \
            stack[sp - 1]; \
        })
#define pop() ({ \
            assert(sp > 0); \
            --sp; \
            stack[sp]; \
        })

<%def name="render_expr(leaf, expr)">
static inline int64_t
${s.expr_name(leaf.get_root(), expr)}(struct encode_state *s, ${leaf.get_root().encode.type} src)
{
   int64_t stack[8], tmp;
   int sp = 0;
   (void)tmp;
%for instr in expr.instructions:
%   if instr[0] == 'DUP':
       push(peek());
%   elif instr[0] == 'RET':
       return pop();
%   elif instr[0] == 'RETIF':
       tmp = pop();
       if (tmp)
          return tmp;
%   elif instr[0] == 'NE':
       push(pop() != pop());
%   elif instr[0] == 'EQ':
       push(pop() == pop());
%   elif instr[0] == 'GT':
       push(pop() > pop());
%   elif instr[0] == 'NOT':
       push(!pop());
%   elif instr[0] == 'OR':
       push(pop() | pop());
%   elif instr[0] == 'AND':
       push(pop() & pop());
%   elif instr[0] == 'LSH':
       push(pop() << pop());
%   elif instr[0] == 'RSH':
       push(pop() >> pop());
%   elif instr[0] == 'ADD':
       push(pop() + pop());
%   elif instr[0] == 'LITERAL':
       push(${instr[1]});
%   elif instr[0] == 'VAR':
       push(${s.extractor(leaf, instr[1])});  /* ${instr[1]} */
%   else:
# error ${'unhandled instruction: ' + instr[0]}
%   endif
%endfor
   return pop();
}
</%def>

## note, we can't just iterate all the expressions, but we need to find
## the context in which they are used to know the correct src type

%for root in s.encode_roots():
<%
    rendered_exprs = []
%>
%   for leaf in s.encode_leafs(root):
%      for expr in s.bitset_used_exprs(leaf):
<%
          if expr in rendered_exprs:
             continue
          rendered_exprs.append(expr)
%>
          ${render_expr(leaf, expr)}
%      endfor
%   endfor
%endfor

#undef pop
#undef peek
#undef push


<%def name="case_pre(root, expr)">
%if expr is not None:
    if (${s.expr_name(root, expr)}(s, src)) {
%endif
</%def>

<%def name="case_post(root, expr)">
%if expr is not None:
    }
%endif
</%def>

/*
 * The actual encoder definitions
 */

%for root in s.encode_roots():

static uint64_t
encode${root.get_c_name()}(struct encode_state *s, ${root.encode.type} src)
{
%  if root.encode.case_prefix is not None:
   switch (${root.get_c_name()}_case(s, src)) {
%  endif
%   for leaf in s.encode_leafs(root):
%      if root.encode.case_prefix is not None:
   case ${s.case_name(root, leaf.name)}: {
%      endif
      uint64_t fld, val = ${hex(leaf.get_pattern().match)};
%      for case in s.bitset_cases(leaf):
          ${case_pre(root, case.expr)}
%         for f in case.fields():
%             for fc in f.field_cases():
                 ${case_pre(root, fc.expr)}
%               if fc.field.get_c_typename() == 'TYPE_BITSET':
                   fld = encode${isa.roots[fc.field.type].get_c_name()}(s, ${s.extractor(leaf, fc.field.name)});
%               else:
                   fld = ${s.extractor(leaf, fc.field.name)};
%               endif
                val |= pack_field(${fc.field.low}, ${fc.field.high}, fld);  /* ${fc.field.name} */
                 ${case_post(root, fc.expr)}
%             endfor

%             for fc in f.assert_cases():
                 ${case_pre(root, fc.expr)}
                val |= pack_field(${fc.field.low}, ${fc.field.high}, ${fc.field.val});
                 ${case_post(root, fc.expr)}
%             endfor


%         endfor
          ${case_post(root, case.expr)}
%      endfor
      return val;
%      if root.encode.case_prefix is not None:
    }
%      endif
%   endfor
%   if root.encode.case_prefix is not None:
   default:
      /* Note that we need the default case, because there are
       * instructions which we never expect to be encoded, (ie.
       * meta/macro instructions) as they are removed/replace
       * in earlier stages of the compiler.
       */
      break;
   }
   mesa_loge("Unhandled ${root.name} encode case: 0x%x\\n", ${root.get_c_name()}_case(s, src));
   return 0;
%   endif
}

%endfor

"""

xml = sys.argv[1]
dst = sys.argv[2]

isa = ISA(xml)
s = State(isa)

with open(dst, 'wb') as f:
    f.write(Template(template, output_encoding='utf-8').render(s=s))
