/*
 * Copyright © 2020 Intel Corporation
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
 *
 */

#include "nir_debug.h"

static void
populate_lines(nir_source_file *f)
{
   if (util_dynarray_contains(&f->lines, char *))
      return;

   char *source = f->source.data;
   util_dynarray_append(&f->lines, char *, source);

   unsigned line = 2;
   char *next;
   while ((next = strchr(source, '\n')) != NULL) {
      next = next + 1;
      unsigned newsize = line * sizeof(char *);
      if (!util_dynarray_ensure_cap(&f->lines, newsize))
         return;
      if (f->lines.size < newsize)
         f->lines.size = newsize;
      *(char **)util_dynarray_element(&f->lines, char *, line - 1) = next;
      source = next;
      line++;
      sscanf(next, "#line %d", &line);
   }
}

uint16_t
nir_shader_set_source_file(nir_shader *shader, const char *file)
{
   if (!shader)
      return 0;

   if (!shader->sources) {
      shader->sources = ralloc(shader, struct util_dynarray);
      util_dynarray_init(shader->sources, shader->sources);
   }

   uint16_t i = 1;
   util_dynarray_foreach(shader->sources, nir_source_file, f) {
      if (!strcmp(file, f->name))
         return i;
      i++;
   }

   if (i >= (1 << NIR_INSTR_SOURCE_FILES_MAX_LOG2))
      return 0;

   nir_source_file *f =
      (nir_source_file *)util_dynarray_grow(shader->sources,
                                            nir_source_file, 1);
   if (!f)
      return 0;

   f->name = ralloc_strdup(shader->sources, file);
   util_dynarray_init(&f->source, shader->sources);
   util_dynarray_init(&f->lines, shader->sources);
   return util_dynarray_num_elements(shader->sources, nir_source_file);
}

void
nir_shader_append_source_contents(nir_shader *shader, uint16_t file,
                                  const char *contents)
{
   if (!shader || !shader->sources || !file)
      return;

   int num_sources = util_dynarray_num_elements(shader->sources,
                                                nir_source_file);
   if (file > num_sources)
      return;

   nir_source_file *f =
      (nir_source_file *)util_dynarray_element(shader->sources, nir_source_file,
                                               file - 1);
   bool first_write = f->source.size == 0;
   size_t growsize = strlen(contents) + (first_write ? 1 : 0);
   char *dest = (char *)util_dynarray_grow(&f->source, char, growsize);
   if (!first_write)
      dest--;
   strcpy(dest, contents);
}

const char *
nir_shader_source_name(nir_shader *shader, uint16_t file)
{
   if (!file || !shader || !shader->sources)
      return "";

   unsigned num_sources = util_dynarray_num_elements(shader->sources,
                                                     nir_source_file);
   if (file > num_sources)
      return "";

   nir_source_file *f = util_dynarray_element(shader->sources, nir_source_file,
                                              file - 1);
   return f->name ? f->name : "";
}

void
nir_shader_source_line(nir_shader *shader, uint16_t file, uint16_t line,
                       const char **out_str, size_t *out_len)
{
   if (out_str)
      *out_str = "";

   if (out_len)
      *out_len = 0;

   if (!shader || !shader->sources || !file || !line)
      return;

   int num_sources = util_dynarray_num_elements(shader->sources,
                                                nir_source_file);
   if (file > num_sources)
      return;

   nir_source_file *f =
      (nir_source_file *)util_dynarray_element(shader->sources, nir_source_file,
                                               file - 1);

   int num_lines = util_dynarray_num_elements(&f->lines, char *);
   if (!num_lines) {
      populate_lines(f);
      num_lines = util_dynarray_num_elements(&f->lines, char *);
   }

   if (line > num_lines)
      return;

   *out_str = *(char **)util_dynarray_element(&f->lines, char *, line - 1);
   *out_len = strchr(*out_str, '\n') ? strchr(*out_str, '\n') - *out_str :
      strlen(*out_str);
}
