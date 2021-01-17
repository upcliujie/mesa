#ifndef NIRLOWERINSTRUCTION_H
#define NIRLOWERINSTRUCTION_H

#include "nir.h"
#include "nir_builder.h"

namespace r600 {

class NirLowerInstruction
{
public:
   NirLowerInstruction();

   bool run(nir_shader *shader);

private:

   bool run(nir_function_impl *func);

   virtual bool filter(nir_instr *instr) const = 0;
   virtual nir_ssa_def *lower(nir_instr *instr) const = 0;

   nir_builder *b;
};

}

#endif // NIRLOWERINSTRUCTION_H
