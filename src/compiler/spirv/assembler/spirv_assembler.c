/*
 * Copyright © 2021 Intel Corporation
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

/* Small and incomplete SPIR-V Assembler to be used by Mesa tests.  Features
 * are added as tests require them.  See also spirv_assembler_info_c.py for
 * the information generated from SPIR-V grammar.
 */

#include "spirv_assembler.h"

#include "util/hash_table.h"
#include "util/u_math.h"
#include "util/bitset.h"
#include "util/ralloc.h"
#include "compiler/spirv/spirv.h"
#include "compiler/spirv/spirv_info.h"
#include "compiler/spirv/assembler/spirv_assembler_info.h"

#include <ctype.h>
#include <errno.h>
#include <unistd.h>

const bool debug = false;

struct token {
   const char *lexeme;
};

struct assembler {
   const char *source;

   struct token *tokens;
   unsigned tokens_size;
   unsigned tokens_cap;
   unsigned current_token;

   uintptr_t bound;
   unsigned binary_size;

   struct hash_table *lexeme_to_id;

   uint32_t *words;
   uint32_t word_count;
};

static bool
is_id_token(struct token *t)
{
   return t && t->lexeme[0] == '%';
}

static bool
is_eol_token(struct token *t)
{
   return !t || t->lexeme[0] == '\n';
}

static bool
is_string_token(struct token *t)
{
   return t && t->lexeme[0] == '"';
}

static struct token *
add_token(struct assembler *as, const char *start, const char *end)
{
   if (as->tokens_size == as->tokens_cap) {
      as->tokens_cap = MAX2(as->tokens_cap * 2, 1);
      as->tokens = reralloc(as, as->tokens, struct token, as->tokens_cap);
   }

   struct token *t = &as->tokens[as->tokens_size++];
   t->lexeme = ralloc_strndup(as, start, end - start);

   if (is_id_token(t)) {
      struct hash_entry *e = _mesa_hash_table_search(as->lexeme_to_id, t->lexeme);
      if (!e)
         _mesa_hash_table_insert(as->lexeme_to_id, t->lexeme, (void *)as->bound++);
   }

   if (t->lexeme[0] == '"') {
      as->binary_size += ALIGN(strlen(t->lexeme) + 1, 4) / 4;
   } else if (t->lexeme[0] != '\n') {
      as->binary_size++;
   }

   return t;
}

static bool
done(struct assembler *as)
{
   return as->current_token >= as->tokens_size;
}

static void
reset(struct assembler *as)
{
   as->current_token = 0;
}

static struct token *
peek(struct assembler *as)
{
   if (as->current_token < as->tokens_size)
      return &as->tokens[as->current_token];
   else
      return NULL;
}

static struct token *
peek2(struct assembler *as)
{
   if (as->current_token+1 < as->tokens_size)
      return &as->tokens[as->current_token+1];
   else
      return NULL;
}

static struct token *
advance(struct assembler *as)
{
   struct token *t = peek(as);
   assert(t);
   as->current_token++;
   return t;
}

static uint32_t *
emit(struct assembler *as)
{
   uint32_t *w = &as->words[as->word_count++];
   return w;
}

static uint32_t *
emit_id(struct assembler *as)
{
   struct token *t = advance(as);
   assert(is_id_token(t));
   struct hash_entry *e = _mesa_hash_table_search(as->lexeme_to_id, t->lexeme);
   assert(e);

   uint32_t *w = emit(as);
   *w = (uintptr_t)e->data;
   return w;
}

static void
emit_id_star(struct assembler *as)
{
   while (is_id_token(peek(as)))
      emit_id(as);
}

static bool
is_integer_literal_token(struct token *t)
{
   return t && (isdigit(t->lexeme[0]) ||
                (t->lexeme[0] == '-' && isdigit(t->lexeme[1])));
}

static void
emit_int_literal(struct assembler *as)
{
   struct token *t = advance(as);
   uint32_t *w = emit(as);
   *w = atoi(t->lexeme);
}

static uint32_t *
emit_string(struct assembler *as)
{
   struct token *t = advance(as);
   assert(t->lexeme[0] == '"');
   assert(t->lexeme[strlen(t->lexeme)-1] == '"');

   /* TODO: Properly handle escapes! */
   unsigned size = strlen(t->lexeme) - 2;

   uint32_t *w = &as->words[as->word_count];

   const char *c = &t->lexeme[1];
   for (unsigned i = 0; i < size; i++)
      w[i/4] |= c[i] << ((i%4) * 8);

   as->word_count += ALIGN(size + 1, 4) / 4;

   return w;
}

#define emit_value(as, f) (*emit(as) = f(advance(as)->lexeme))

static void
flip(struct token *a, struct token *b)
{
   struct token tmp = *a;
   *a = *b;
   *b = tmp;
}

static bool
op_has_id_result_type(SpvOp op)
{
   switch (op) {
   case SpvOpFunctionCall:
   case SpvOpVariable:
   case SpvOpConstantNull:
   case SpvOpConstant:
   case SpvOpConstantTrue:
   case SpvOpConstantFalse:
   case SpvOpConstantComposite:
   case SpvOpFunction:
   case SpvOpLoad:
   case SpvOpAccessChain:
      return true;
   default:
      return false;
   }
}

uint32_t *
spirv_assemble(void *mem_ctx, uint32_t version, const char *input, uint32_t *word_count)
{
   struct assembler *as = rzalloc(mem_ctx, struct assembler);
   as->source = input;
   as->lexeme_to_id = _mesa_hash_table_create(as, _mesa_hash_string,
                                              _mesa_key_string_equal);
   as->bound = 1;

   const char *p = input;
   while (*p != '\0') {
      if (isspace(*p)) {
         if (*p == '\n') {
            const char *start = p;
            p++;
            add_token(as, start, p);
         } else {
            while (*p != '\0' && isspace(*p)) p++;
         }
      } else if (*p == ';') {
         while (*p != '\n' && *p != '\0') p++;
      } else if (*p == '"') {
         /* TODO: Unquote the string contents.  Handle \0. */
         const char *start = p;
         /* Opening quote. */
         p++;
         while (*p != '\0' && *p != '"') p++;
         /* Closing quote. */
         p++;
         add_token(as, start, p);
      } else if (*p == '=') {
         /* Skip it, we will identify presence of result by having the first
          * token starting with % symbol.
          */
         p++;
      } else {
         const char *start = p;
         while (*p != '\0' && !isspace(*p)) p++;
         add_token(as, start, p);
      }
   }

   if (debug) {
      for (unsigned i = 0; i < as->tokens_size; i++) {
         struct token *t = &as->tokens[i];
         if (t->lexeme[0] == '\n') {
            printf("\n");
         } else {
            printf("[%s] ", t->lexeme);
         }
      }
   }

   /* Header. */
   as->binary_size += 5;

   as->words = (uint32_t *) rzalloc_size(mem_ctx, as->binary_size * 4);

   *emit(as) = SpvMagicNumber;
   *emit(as) = version;
   *emit(as) = 0x00070000;      /* Generator */
   *emit(as) = as->bound;
   *emit(as) = 0x00000000;      /* Reserved */

   /* Re-order tokens so they match the expected binary emission order: Op,
    * IdResultType then IdResult.  The Text Assembly representation is
    * "IdResult = Op IdResultType".
    */
   while (!done(as)) {
      struct token *head = advance(as);
      if (head->lexeme[0] == '\n')
         continue;

      if (is_id_token(head)) {
         struct token *id_result = peek(as);
         flip(head, id_result);
      }

      struct spirv_op_info info = spirv_string_to_op_info(head->lexeme);
      if (op_has_id_result_type(info.opcode)) {
         struct token *id_result = peek(as);
         struct token *id_result_type = peek2(as);
         assert(id_result && id_result_type);
         assert(is_id_token(id_result));
         assert(is_id_token(id_result_type));
         flip(id_result, id_result_type);
      }

      /* Advance to next operation. */
      while (!is_eol_token(advance(as)));
   }

   reset(as);

   while (!done(as)) {
      struct token *head = advance(as);
      if (head->lexeme[0] == '\n')
         continue;

      struct spirv_op_info info = spirv_string_to_op_info(head->lexeme);

      /* First word will have opcode and word count (that is filled later). */
      uint32_t *first = emit(as);
      *first = info.opcode;

      switch (info.opcode) {
      case SpvOpExecutionMode: {
         emit_id(as);

         /* TODO: Parse operands information from grammar. */
         SpvExecutionMode mode = (SpvExecutionMode) emit_value(as, spirv_string_to_executionmode);
         switch (mode) {
         case SpvExecutionModeLocalSize:
            emit_int_literal(as);
            emit_int_literal(as);
            emit_int_literal(as);
            break;

         default:
            if (!is_eol_token(peek(as))) {
               fprintf(stderr, "Unhandled ExecutionMode %u\n", mode);
               return NULL;
            }
            break;
         }

         break;
      }

      case SpvOpDecorate:
      case SpvOpMemberDecorate: {
         emit_id(as);

         if (info.opcode == SpvOpMemberDecorate)
            emit_int_literal(as);

         /* TODO: Parse operand information from grammar. */
         SpvDecoration dec = (SpvDecoration) emit_value(as, spirv_string_to_decoration);
         switch (dec) {
         case SpvDecorationOffset:
         case SpvDecorationDescriptorSet:
         case SpvDecorationBinding:
         case SpvDecorationArrayStride:
         case SpvDecorationLocation:
            emit_int_literal(as);
            break;
         default:
            if (!is_eol_token(peek(as))) {
               fprintf(stderr, "Unhandled Decoration %s (%u)\n", spirv_decoration_to_string(dec), dec);
               return NULL;
            }
         }
         break;
      }

      case SpvOpLoad: {
         emit_id(as);
         emit_id(as);
         emit_id_star(as);
         if (peek(as)->lexeme[0] != '\n') {
            SpvMemoryAccessMask ma = (SpvMemoryAccessMask) emit_value(as, spirv_string_to_memoryaccess);
            if (ma & SpvMemoryAccessMakePointerVisibleMask)
               emit_id(as);
         }
         break;
      }

      /* For now, explicitly enable operations since we don't cover the full
       * combinations of operands.
       */
      case SpvOpAccessChain:
      case SpvOpBranch:
      case SpvOpBranchConditional:
      case SpvOpCapability:
      case SpvOpConstant:
      case SpvOpConstantComposite:
      case SpvOpConstantFalse:
      case SpvOpConstantNull:
      case SpvOpConstantTrue:
      case SpvOpEntryPoint:
      case SpvOpExtInstImport:
      case SpvOpFunction:
      case SpvOpFunctionCall:
      case SpvOpFunctionEnd:
      case SpvOpKill:
      case SpvOpLabel:
      case SpvOpLoopMerge:
      case SpvOpMemoryModel:
      case SpvOpName:
      case SpvOpReturn:
      case SpvOpReturnValue:
      case SpvOpSelectionMerge:
      case SpvOpSource:
      case SpvOpStore:
      case SpvOpSwitch:
      case SpvOpTypeBool:
      case SpvOpTypeFloat:
      case SpvOpTypeFunction:
      case SpvOpTypeInt:
      case SpvOpTypePointer:
      case SpvOpTypeStruct:
      case SpvOpTypeVector:
      case SpvOpTypeVoid:
      case SpvOpUnreachable:
      case SpvOpVariable:
      {
         const enum spirv_operands *operand = &info.operands[0];
         while (*operand != NONE) {
            bool is_optional = *operand & OPTIONAL;
            bool is_star = *operand & STAR;
            switch (*operand & ~(OPTIONAL|STAR)) {
            case IDREF:
               if (is_star) {
                  while (is_id_token(peek(as)))
                     emit_id(as);
               } else if (!is_optional || is_id_token(peek(as))){
                  emit_id(as);
               }
               break;
            case LITERALINTEGER:
               if (is_star) {
                  while (is_integer_literal_token(peek(as)))
                     emit_int_literal(as);
               } else if (!is_optional || is_integer_literal_token(peek(as))) {
                   emit_int_literal(as);
               }
               break;
            case LITERALSTRING:
               if (is_star) {
                  while (is_string_token(peek(as)))
                     emit_string(as);
               } else if (!is_optional || is_string_token(peek(as))) {
                   emit_string(as);
               }
               break;
            case LOOPCONTROL:
               emit_value(as, spirv_string_to_loopcontrol);
               break;
            case SELECTIONCONTROL:
               emit_value(as, spirv_string_to_selectioncontrol);
               break;
            case STORAGECLASS:
               emit_value(as, spirv_string_to_storageclass);
               break;
            case FUNCTIONCONTROL:
               emit_value(as, spirv_string_to_functioncontrol);
               break;
            case SOURCELANGUAGE:
               emit_value(as, spirv_string_to_sourcelanguage);
               break;
            case EXECUTIONMODEL:
               emit_value(as, spirv_string_to_executionmodel);
               break;
            case CAPABILITY:
               emit_value(as, spirv_string_to_capability);
               break;
            case ADDRESSINGMODEL:
               emit_value(as, spirv_string_to_addressingmodel);
               break;
            case MEMORYMODEL:
               emit_value(as, spirv_string_to_memorymodel);
               break;
            case MEMORYACCESS:
               if (!is_optional || !is_eol_token(peek(as)))
                  emit_value(as, spirv_string_to_memoryaccess);
               break;
            case PAIRLITERALINTEGERIDREF:
               if (is_star) {
                  while (is_integer_literal_token(peek(as))) {
                     emit_int_literal(as);
                     emit_id(as);
                  }
               } else if (!is_optional || is_integer_literal_token(peek(as))) {
                   emit_int_literal(as);
                   emit_id(as);
               }
               break;
               break;
            default:
               fprintf(stderr, "Unhandled operand type %d for opcode %s (%u)\n",
                       *operand, spirv_op_to_string(info.opcode), info.opcode);
               return NULL;
            }
            operand++;
         }
         break;
      }

      default:
         fprintf(stderr, "Unhandled opcode %s (%u)\n", spirv_op_to_string(info.opcode), info.opcode);
         return NULL;
      }

      const unsigned count = &as->words[as->word_count] - first;
      *first |= count << SpvWordCountShift;

     if (debug)
        printf("[%03lu %s 0x%08X]\n", first - as->words, head->lexeme, first[0]);
   }

   if (getenv("MESA_SPIRV_ASSEMBLER_VALIDATE") != NULL) {
      char tmp[] = "/tmp/spirv.XXXXXX";
      int fd = mkstemp(tmp);
      assert(fd >= 0);

      fprintf(stderr, "Writing output to %s\n", tmp);
      int written = write(fd, as->words, 4 * as->word_count);
      close(fd);

      if (written != 4 * as->word_count) {
         fprintf(stderr, "Error writing to %s: %s\n", tmp,
                 written < 0 ? strerror(errno) : "short write");
         return NULL;
      }

      /* SPIRV-Tools disassembler will also validate the shader. */
      const char *cmd = ralloc_asprintf(as, "spirv-dis --raw-id %s", tmp);
      FILE *p = popen(cmd, "r");
      char line[2048];
      while (fgets(line, sizeof(line), p))
         fputs(line, stderr);

      int ret = pclose(p);
      if (ret != 0) {
         fprintf(stderr, "Failed validation with SPIR-V, file %s\n", tmp);
         return NULL;
      } else {
         fprintf(stderr, "Validation succeeded, file %s\n", tmp);
      }
   }

   uint32_t *words = as->words;
   if (word_count)
      *word_count = as->word_count;

   if (debug)
      fprintf(stderr, "spirv_assemble() generated %u words.\n", *word_count);

   ralloc_free(as);

   return words;
}

