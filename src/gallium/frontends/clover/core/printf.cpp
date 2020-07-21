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

#include <cstring>
#include <cstdio>
#include <string>

#include "core/printf.hpp"

using namespace clover;

namespace {

   bool
   find_tokens_pos(const std::string &s, size_t pos,
                   size_t &cur_tok, size_t &next_tok, size_t &spec_pos) {

      bool valid_specifier = false;

      // find a valid % format
      do {
         pos = s.find_first_of('%', pos);
         while (pos != std::string::npos && s[pos+1] == '%') {
            pos+=2;
            pos = s.find_first_of('%', pos);
         }

         cur_tok = pos;

         // find the next possible token
         next_tok = std::string::npos;
         if (cur_tok != std::string::npos)
            next_tok = s.find_first_of('%', cur_tok + 1);

         // look for a specifier
         if (cur_tok != std::string::npos) {
            spec_pos = s.find_first_of("cdieEfgGaAosuxXp", cur_tok + 1);
            valid_specifier = spec_pos < next_tok;

         } else {
            spec_pos = std::string::npos;
         }

         pos++;
      } while (!valid_specifier && spec_pos != std::string::npos);

      return valid_specifier;
   }

   void
   print_formatted(const std::vector<printf_handler::formatter> &formatters,
                  const std::vector<char> &buffer) {

      for (size_t buf_pos = 0; buf_pos < buffer.size(); ) {
         cl_uint fmt_idx = *(cl_uint*)&buffer[buf_pos];
         printf_handler::formatter fmt = formatters[fmt_idx-1];

         buf_pos += sizeof(cl_uint);

         if (fmt.arg_sizes.empty()) {
            printf(fmt.format.c_str(), 0);

         } else {
            size_t fmt_last_pos = 0;
            size_t fmt_pos = 0;
            for (int arg_size : fmt.arg_sizes) {
               size_t cur_tok;
               size_t next_tok;
               size_t spec_pos;

               bool specifier_ok =
                  find_tokens_pos(fmt.format, fmt_pos,
                                  cur_tok, next_tok, spec_pos);

               size_t vec_pos = fmt.format.find_first_of("v", cur_tok + 1);
               size_t mod_pos = fmt.format.find_first_of("hl", cur_tok + 1);

               bool is_vector = vec_pos != std::string::npos &&
                                                   vec_pos + 1 < spec_pos;
               bool is_string = fmt.format[spec_pos] == 's';
               bool is_float = std::string("fFeEgGaA")
                  .find(fmt.format[spec_pos]) != std::string::npos;

               // print the part before the format token
               if (cur_tok - fmt_last_pos) {
                  std::string s = fmt.format.substr(fmt_last_pos,
                                                    cur_tok - fmt_last_pos);
                  printf(s.c_str(), 0);
               }

               // print the formated part

               if (!specifier_ok)
                  spec_pos = cur_tok;

               std::string print_str;
               print_str = fmt.format.substr(cur_tok, spec_pos + 1 - cur_tok);

               if (!specifier_ok) {
                  printf("%s", print_str.c_str());

               } else if (is_string) {
                  printf(print_str.c_str(), &buffer[buf_pos]);

               } else {
                  int component_count = 1;

                  if (is_vector) {
                     size_t l = std::min(mod_pos, spec_pos) - vec_pos - 1;
                     std::string s = fmt.format.substr(vec_pos + 1, l);
                     component_count = std::stoi(s);
                     print_str.erase(vec_pos - cur_tok, spec_pos - vec_pos);
                     print_str.push_back(',');
                  }

                  //in fact vec3 are vec4
                  int men_components =
                     component_count == 3 ? 4 : component_count;
                  size_t elmt_size = arg_size / men_components;

                  for (int i = 0; i < component_count; i++) {
                     size_t elmt_buf_pos = buf_pos + i * elmt_size;
                     if (is_vector && i + 1 == component_count)
                        print_str.pop_back();

                     if (is_float) {
                        switch (elmt_size) {
                           case 2:
                              cl_half h;
                              std::memcpy(&h, &buffer[elmt_buf_pos], elmt_size);
                              printf(print_str.c_str(), h);
                              break;
                           case 4:
                              cl_float f;
                              std::memcpy(&f, &buffer[elmt_buf_pos], elmt_size);
                              printf(print_str.c_str(), f);
                              break;
                           default:
                              cl_double d;
                              std::memcpy(&d, &buffer[elmt_buf_pos], elmt_size);
                              printf(print_str.c_str(), d);
                        }

                     } else {
                        cl_long l;
                        std::memcpy(&l, &buffer[elmt_buf_pos], elmt_size);
                        printf(print_str.c_str(), l);
                     }
                  }
               }

               // print the remaining
               if (cur_tok - fmt_last_pos) {
                  std::string s = fmt.format.substr(spec_pos + 1,
                                                    next_tok - spec_pos);
                  printf(s.c_str(), 0);
               }

               fmt_pos = cur_tok;
               fmt_last_pos = next_tok;

               buf_pos+= arg_size;
            }
         }
      }
   }
}

printf_handler::formatter::formatter(std::string &raw) {
   // arg count
   size_t token_pos = raw.find_first_of(":");
   int arg_count = std::stoi(raw.substr(0, token_pos));

   // args sizes
   for (int i = 0; i < arg_count && token_pos != std::string::npos ; i++) {
      size_t arg_size_start = token_pos + 1;
      token_pos = raw.find_first_of(":", arg_size_start);

      if (token_pos != std::string::npos && raw[arg_size_start] != ':') {
         int arg_size = std::stoi(raw.substr(arg_size_start, token_pos));
         arg_sizes.push_back(arg_size);
      }
   }

   // printf format
   for (size_t i = raw.find_last_of(":") + 1; i < raw.size(); i++) {
      char c = raw[i];

      if (c == '\\') {
         i++; // i++ is possible because there is always a \0;
         switch (raw[i]) {
            case 'a': c = '\a'; break;
            case 'b': c = '\b'; break;
            case 'e': c = '\e'; break;
            case 'f': c = '\f'; break;
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            case 'v': c = '\v'; break;
            case '?': c = '\?'; break;
            case '\\': c = '\\'; break;
            case '\'': c = '\''; break;
            case '\"': c = '\"'; break;
            default: --i;
         }
      }

      format.push_back(c);
   }
}

std::unique_ptr<printf_handler>
printf_handler::create(const intrusive_ptr<command_queue> &q,
                       const std::vector<std::string> &formats,
                       cl_uint size) {
   return std::unique_ptr<printf_handler>(
                                       new printf_handler(q, formats, size));
}

printf_handler::printf_handler(const intrusive_ptr<command_queue> &q,
                               const std::vector<std::string> &formats,
                               cl_uint size) :
   _q(q), _formatters(), _size(size), _buffer() {

   if (_size && formats.size()) {
         for (auto f : formats)
            _formatters.push_back(formatter(f));

      std::string data;
      data.reserve(_size);
      cl_uint header[2] = { initial_buffer_offset, _size };
      data.append((char *)header, (char *)(header+sizeof(header)));
      _buffer = std::unique_ptr<root_buffer>(new root_buffer(_q->context,
                                             CL_MEM_COPY_HOST_PTR,
                                             _size, (char*)data.data()));
   }
}

cl_mem
printf_handler::get_mem() {
   return (cl_mem)(_buffer.get());
}

void
printf_handler::print() {
   if (!_buffer)
      return;

   mapping src = { *_q, _buffer->resource_in(*_q), CL_MAP_READ, true,
                  {{ 0 }}, {{ _size, 1, 1 }} };

   cl_uint header[2] = { 0 };
   std::memcpy(header,
               static_cast<const char *>(src),
               initial_buffer_offset);

   cl_uint buffer_size = header[0] - initial_buffer_offset;
   std::vector<char> buf;
   buf.resize(buffer_size);

   std::memcpy(buf.data(),
               static_cast<const char *>(src) + initial_buffer_offset,
               buffer_size);

   print_formatted(_formatters, buf);
}
