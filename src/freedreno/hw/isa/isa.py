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

from xml.etree import ElementTree
import os

class BitSetPattern(object):
    """Class that encapsulated the pattern matching, ie.
       the match/dontcare/mask bitmasks.  The following
       rules should hold

          (match ^ dontcare) == 0
          (match || dontcare) == mask

       For a leaf node, the mask should be (1 << size) - 1
       (ie. all bits set)
    """
    def __init__(self, bitset):
        self.match      = bitset.match
        self.dontcare   = bitset.dontcare
        self.mask       = bitset.mask
        self.field_mask = bitset.field_mask;

    def merge(self, pattern):
        p = BitSetPattern(pattern)
        p.match      = p.match      | self.match
        p.dontcare   = p.dontcare   | self.dontcare
        p.mask       = p.mask       | self.mask
        p.field_mask = p.field_mask | self.field_mask
        return p

    def defined_bits(self):
        return self.match | self.dontcare | self.mask | self.field_mask

def get_bitrange(field):
    if 'pos' in field.attrib:
        assert('low' not in field.attrib)
        assert('high' not in field.attrib)
        low = int(field.attrib['pos'])
        high = low
    else:
        low = int(field.attrib['low'])
        high = int(field.attrib['high'])
    assert low <= high
    return low, high

def get_c_name(name):
    return name.lower().replace('#', '__').replace('-', '_').replace('.', '_')

class BitSetField(object):
    """Class that encapsulates a field defined in a bitset
    """
    def __init__(self, isa, xml):
        self.isa = isa
        self.low, self.high = get_bitrange(xml)
        self.name = xml.attrib['name']
        self.type = xml.attrib['type']
        self.expr = None
        self.display = None
        if 'display' in xml.attrib:
            self.display = xml.attrib['display'].strip()

    def get_c_typename(self):
        if self.type in self.isa.enums:
            return 'TYPE_ENUM'
        if self.type in self.isa.bitsets:
            return 'TYPE_BITSET'
        return 'TYPE_' + self.type.upper()

class BitSetDerivedField(BitSetField):
    """Similar to BitSetField, but for derived fields
    """
    def __init__(self, isa, xml):
        self.isa = isa
        self.low = 0
        self.high = 0
        self.name = xml.attrib['name']
        self.type = xml.attrib['type']
        self.expr = xml.attrib['expr']
        self.display = None
        if 'display' in xml.attrib:
            self.display = xml.attrib['display'].strip()

class BitSetCase(object):
    """Class that encapsulates a single bitset case
    """
    def __init__(self, bitset, xml, expr=None):
        self.bitset = bitset
        self.name = bitset.name + '#case' + str(len(bitset.cases))
        self.expr = expr
        self.field_mask = 0
        self.fields = {}

        for derived in xml.findall('derived'):
            f = BitSetDerivedField(bitset.isa, derived)
            self.fields[f.name] = f

        for field in xml.findall('field'):
            print("{}.{}".format(self.name, field.attrib['name']))
            f = BitSetField(bitset.isa, field)

            m = ((1 << (1 + f.high - f.low)) - 1) << f.low

            print("field: {}.{} => {:016x}".format(self.name, f.name, m))

            self.field_mask |= m

            self.fields[f.name] = f

        self.display = None
        for d in xml.findall('display'):
            self.display = d.text.strip()
            print("found display: '{}'".format(self.display))

    def get_c_name(self):
        return get_c_name(self.name)

class BitSet(object):
    """Class that encapsulates a single bitset rule
    """
    def __init__(self, isa, xml):
        self.isa = isa
        self.xml = xml
        self.name = xml.attrib['name']
        if 'size' in xml.attrib:
            assert('extends' not in xml.attrib)
            self.size = int(xml.attrib['size'])
            self.extends = None
        else:
            self.size = None
            self.extends = xml.attrib['extends']

        self.expr = None
        if 'expr' in xml.attrib:
            self.expr = xml.attrib['expr']

        # Collect up the match/dontcare/mask bitmasks for
        # this bitset case:
        self.match = 0
        self.dontcare = 0
        self.mask = 0
        self.field_mask = 0

        self.cases = []

        for override in xml.findall('override'):
            c = BitSetCase(self, override, override.attrib['expr'])
            self.field_mask |= c.field_mask
            self.cases.append(c)

        dflt = BitSetCase(self, xml)
        self.field_mask |= dflt.field_mask
        self.cases.append(dflt)

        # Helper to check for redefined bits:
        def is_defined_bits(m):
            return ((self.field_mask | self.mask | self.dontcare | self.match) & m) != 0

        for pattern in xml.findall('pattern'):
            l, h = get_bitrange(pattern)
            m = ((1 << (1 + h - l)) - 1) << l

            patstr = pattern.text

            assert (len(patstr) == (1 + h - l)), "Invalid pattern length in {}: {}..{}".format(self.name, l, h)
            assert not is_defined_bits(m), "Redefined bits in pattern {}: {}..{}".format(self.name, l, h);

            match = 0;
            dontcare = 0

            for n in range(0, len(patstr)):
                match = match << 1
                dontcare = dontcare << 1
                if patstr[n] == '1':
                    match |= 1
                elif patstr[n] == 'x':
                    dontcare |= 1
                elif patstr[n] != '0':
                    assert 0, "Invalid pattern character in {}: {}".format(self.name, patstr[n])

            self.match    |= match << l
            self.dontcare |= dontcare << l
            self.mask     |= m

            print("pattern: {}.{} => {:016x} / {:016x} / {:016x}".format(self.name, patstr, match << l, dontcare << l, m))

    def get_pattern(self):
        if self.extends is not None:
            parent = self.isa.bitsets[self.extends]
            ppat = parent.get_pattern()
            pat  = BitSetPattern(self)

            assert ((ppat.defined_bits() & pat.defined_bits()) == 0), "bitset conflict in {}: {:x}".format(self.name, (ppat.defined_bits() & pat.defined_bits()))

            return pat.merge(ppat)

        return BitSetPattern(self)

    def get_size(self):
        if self.extends is not None:
            parent = self.isa.bitsets[self.extends]
            return parent.get_size()
        return self.size

    def get_c_name(self):
        return get_c_name(self.name)

    def get_root(self):
        if self.extends is not None:
            return self.isa.bitsets[self.extends].get_root()
        return self

class BitSetEnum(object):
    """Class that encapsulates an enum declaration
    """
    def __init__(self, isa, xml):
        self.isa = isa
        self.name = xml.attrib['name']
        # Table mapping value to name
        # TODO currently just mapping to 'display' name, but if we
        # need more attributes then maybe need BitSetEnumValue?
        self.values = {}
        for value in xml.findall('value'):
            self.values[value.attrib['val']] = value.attrib['display']

    def get_c_name(self):
        return 'enum_' + get_c_name(self.name)

class BitSetExpression(object):
    """Class that encapsulates an <expr> declaration
    """
    def __init__(self, isa, xml):
        self.isa = isa
        if 'name' in xml.attrib:
            self.name = xml.attrib['name']
        else:
            self.name = 'anon_' + isa.anon_expression_count
            isa.anon_expression_count = isa.anon_expression_count + 1
        self.instructions = []
        for child in xml:
            if child.tag == 'literal':
                self.instructions.append(['LITERAL', child.attrib['val']])
            elif child.tag == 'var':
                self.instructions.append(['VAR', child.attrib['name']])
            elif child.tag == 'dup':
                self.instructions.append(['DUP'])
            elif child.tag == 'retif':
                self.instructions.append(['RETIF'])
            elif child.tag == 'ne':
                self.instructions.append(['NE'])
            elif child.tag == 'eq':
                self.instructions.append(['EQ'])
            elif child.tag == 'or':
                self.instructions.append(['OR'])
            elif child.tag == 'and':
                self.instructions.append(['AND'])
            elif child.tag == 'lsh':
                self.instructions.append(['LSH'])
            elif child.tag == 'not':
                self.instructions.append(['NOT'])
            else:
                # ignore <doc> elements, anything else unknown is an error:
                assert child.tag == 'doc', "{}: unknown expression element: {}".format(self.name, child.tag)

        print("XXX {} - {}".format(self.name, str(self.instructions)))

    def get_c_name(self):
        return 'expr_' + get_c_name(self.name)

class ISA(object):
    """Class that encapsulates all the parsed bitset rules
    """
    def __init__(self, xmlpath):
        self.base_path = os.path.dirname(xmlpath)

        # Counter used to name inline (anonymous) expressions:
        self.anon_expression_count = 0

        # Table of (globally defined) expressions:
        self.expressions = {}

        # Table of enums:
        self.enums = {}

        # Table of toplevel bitset hierarchies:
        self.roots = {}

        # Table of leaf nodes of bitset hierarchies:
        self.leafs = {}

        # Table of all bitsets:
        self.bitsets = {}

        root = ElementTree.parse(xmlpath).getroot()
        self.parse_file(root)
        self.validate_isa()

    def parse_file(self, root):
        # Handle imports up-front:
        for imprt in root.findall('import'):
            p = os.path.join(self.base_path, imprt.attrib['file'])
            self.parse_file(ElementTree.parse(p))

        # Extract expressions:
        for expr in root.findall('expr'):
            e = BitSetExpression(self, expr)
            self.expressions[e.name] = e

        # Extract enums:
        for enum in root.findall('enum'):
            e = BitSetEnum(self, enum)
            self.enums[e.name] = e

        # Extract bitsets:
        for bitset in root.findall('bitset'):
            b = BitSet(self, bitset)
            if b.size is not None:
                print("toplevel: " + b.name)
                self.roots[b.name] = b
            else:
                print("derived: " + b.name)
            self.bitsets[b.name] = b
            self.leafs[b.name]  = b

        # Remove non-leaf nodes from the leafs table:
        for name, bitset in self.bitsets.items():
            if bitset.extends is not None:
                if bitset.extends in self.leafs:
                    del self.leafs[bitset.extends]

    def validate_isa(self):
        for name, bitset in self.leafs.items():
            pat = bitset.get_pattern()
            sz  = bitset.get_size()
            print("leaf: {}, mask={:x}".format(bitset.name, pat.mask))
            assert ((pat.mask | pat.field_mask) == (1 << sz) - 1)

        # TODO somehow validating that only one bitset in a hierarchy
        # matches any given bit pattern would be useful.
