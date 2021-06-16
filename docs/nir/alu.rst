NIR ALU Instructions
====================

TODO: Docs for :cpp:struct:`nir_alu_instr` go here.

NIR ALU Opcode Reference:
-------------------------

Each opcode in the reference below contains a table the following easy-access
information about each opcode:

 - "Vectorized":  A vectorized operation can be applied to any size vectors and
   performs the same operation on each vector component.  Each component in the
   destination is the result of the operation applied to the corresponding
   component in each of the sources.
 - "Associative":  Mathematical associativity: :math:`(a + b) + c = a + (b + c)`.
 - "2-src commutative":  For 2-source operations, this just mathematical
   commutativity.  Some 3-source operations, like ffma, are only commutative in
   the first two sources.

.. nir:alu-opcodes::
