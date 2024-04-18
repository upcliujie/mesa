#include <gtest/gtest.h>
#include <bitset>

#include "mesa/main/shaderapi.h"
#include "mesa/main/get.h"
#include "mesa/main/enums.h"
#include "mesa/main/shaderobj.h"
#include "util/compiler.h"
#include "main/mtypes.h"
#include "main/macros.h"
#include "compiler/nir/nir.h"
#include "compiler/nir/nir_builder.h"
#include "compiler/glsl/string_to_uint_map.h"
#include "mesa/state_tracker/st_nir.h"

template<size_t bitset_capacity>
void mark_used(std::bitset<bitset_capacity>& bitset, int location, int component)
{
   int index = (location * 4) + component;
   bitset.set(index);
}

template<size_t bitset_capacity>
void mark_used(std::bitset<bitset_capacity>& bitset, int location)
{
   for (int component = 0; component < 4; ++component) {
      mark_used(bitset, location, component);
   }
}

template<size_t bitset_capacity>
bool is_used(const std::bitset<bitset_capacity>& bitset, int location, int component)
{
   int index = (location * 4) + component;
   return bitset[index];
}

template<size_t bitset_capacity>
bool is_used(const std::bitset<bitset_capacity>& bitset, int location)
{
   for (int component = 0; component < 4; ++component) {
      if (is_used(bitset, location, component)) {
         return true;
      }
   }

   return false;
}

class TestIsVariableUsed : public ::testing::Test {
public:
   virtual void SetUp() override;
   virtual void TearDown() override;
   void InitializeShaderStage(gl_shader_stage stage);

   bool stages[MESA_SHADER_STAGES];
   gl_shader_program *prog;
};

void
TestIsVariableUsed::SetUp()
{
   glsl_type_singleton_init_or_ref();

   prog = rzalloc(nullptr, gl_shader_program);
   prog->SeparateShader = false;
   prog->data = rzalloc(prog, gl_shader_program_data);
   prog->data->ProgramResourceList = NULL;
   prog->data->NumProgramResourceList = 0;

   for (int i = 0; i < MESA_SHADER_STAGES; ++i) {
      prog->_LinkedShaders[i] = nullptr;
   }
}

void
TestIsVariableUsed::TearDown()
{
   if (prog->UniformHash)
      string_to_uint_map_dtor(prog->UniformHash);

   ralloc_free(prog);
   prog = nullptr;

   glsl_type_singleton_decref();
}

void
TestIsVariableUsed::InitializeShaderStage(gl_shader_stage stage) {
   assert(stage < MESA_SHADER_STAGES);
   prog->_LinkedShaders[stage] = rzalloc(prog, gl_linked_shader);
   prog->_LinkedShaders[stage]->Program = rzalloc(prog, gl_program);
}

TEST_F(TestIsVariableUsed, VertexShader) {
   constexpr int components_per_location = 4;
   constexpr int num_locations = 32;
   constexpr int num_components = components_per_location * num_locations;
   std::bitset<num_components> component_bitset;
   constexpr gl_shader_stage stage = MESA_SHADER_VERTEX;
   InitializeShaderStage(stage);

   const nir_shader_compiler_options compiler_options {};
   nir_builder builder = nir_builder_init_simple_shader(
      stage, &compiler_options, "test_shader");

   uint64_t& inputs_read = builder.shader->info.inputs_read;

   mark_used(component_bitset, VERT_ATTRIB_GENERIC10 - VERT_ATTRIB_GENERIC0);
   inputs_read |= ((uint64_t)1) << VERT_ATTRIB_GENERIC10;

   mark_used(component_bitset, VERT_ATTRIB_GENERIC6 - VERT_ATTRIB_GENERIC0);
   inputs_read |= ((uint64_t)1) << VERT_ATTRIB_GENERIC6;

   mark_used(component_bitset, VERT_ATTRIB_GENERIC3 - VERT_ATTRIB_GENERIC0);
   inputs_read |= ((uint64_t)1) << VERT_ATTRIB_GENERIC3;

   prog->_LinkedShaders[stage]->Program->nir = builder.shader;
   prog->_LinkedShaders[stage]->Program->DualSlotInputs = 0;

   gl_shader_variable *shader_variable = rzalloc(prog, gl_shader_variable);
   shader_variable->type = glsl_float_type();

   auto is_variable_used = [&](int location, int component) {
      shader_variable->location = location;
      shader_variable->component = component;
      if (st_is_variable_used(prog, shader_variable)) {
         return testing::AssertionSuccess()
            << "location " << location << " "
            << "component " << component
            << " is used";
      } else {
         return testing::AssertionFailure()
            << "location " << location << " "
            << "component " << component
            << " isn't used";
      }
   };

   for (int location = 0; location < num_locations; ++location) {
      for (int component = 0; component < components_per_location; ++component) {
         if (is_used(component_bitset, location, component)) {
            EXPECT_TRUE(is_variable_used(location, component));
         } else {
            EXPECT_FALSE(is_variable_used(location, component));
         }
      }
   }

   ralloc_free(shader_variable);
   ralloc_free(builder.shader);
}
