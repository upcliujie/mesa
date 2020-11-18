//
// Copyright 2020 Serge Martin
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// Extract from Serge's printf clover code by airlied.

#include "u_printf.h"

bool
util_printf_find_tokens_pos(const std::string &s, size_t pos,
                            size_t &next_tok, size_t &spec_pos) {

   bool valid_specifier = false;

   // find a valid % format
   do {
      pos = s.find_first_of('%', pos);
      while (pos != std::string::npos && s[pos+1] == '%') {
         pos += 2;
         pos = s.find_first_of('%', pos);
      }

      // find the next possible token
      next_tok = std::string::npos;
      if (pos != std::string::npos)
         next_tok = s.find_first_of('%', pos + 1);

      // look for a specifier
      if (pos != std::string::npos) {
         spec_pos = s.find_first_of("cdieEfgGaAosuxXp", pos + 1);
         valid_specifier = spec_pos < next_tok;
      } else {
         spec_pos = std::string::npos;
      }

      pos++;
   } while (!valid_specifier && spec_pos != std::string::npos);

   return valid_specifier;
}

bool util_printf_next_spec_is_string(const char *str,
                                     size_t *fmt_pos)
{
   bool is_string;
   size_t next_tok;
   size_t spec_pos;
   bool ret;

   ret = util_printf_find_tokens_pos(std::string(str), *fmt_pos,
                                     next_tok,
                                     spec_pos);
   if (!ret)
      return false;

   is_string = str[spec_pos] == 's';
   *fmt_pos = next_tok;
   return is_string;
}
