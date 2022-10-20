# coding=utf-8
# -*- Mode: Python; py-indent-offset: 4 -*-
#
# Copyright © 2012 Intel Corporation
#
# Based on code by Kristian Høgsberg <krh@bitplanet.net>,
#   extracted from mesa/main/get.c
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# on the rights to use, copy, modify, merge, publish, distribute, sub
# license, and/or sell copies of the Software, and to permit persons to whom
# the Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
# IBM AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

# Generate a C header file containing hash tables of glGet parameter
# names for each GL API. The generated file is to be included by glGet.c

from collections import defaultdict
import argparse
import os
import sys
import typing

import get_hash_params

param_desc_file = os.path.join(os.path.dirname(__file__), "get_hash_params.py")

GLAPI = os.path.join(os.path.dirname(__file__), "..", "..", "mapi", "glapi", "gen")
sys.path.insert(0, GLAPI)
import gl_XML

prime_factor = 89
prime_step = 281
hash_table_size = 1024

gl_apis=set(["GL", "GL_CORE", "GLES", "GLES2", "GLES3", "GLES31", "GLES32"])

def print_header(f: 'typing.TextIO') -> 'None':
   f.write("typedef const unsigned short table_t[%d];\n\n" % (hash_table_size))
   f.write("static const int prime_factor = %d, prime_step = %d;\n\n" % \
          (prime_factor, prime_step))

def print_params(f: 'typing.TextIO', params) -> 'None':
   f.write("static const struct value_desc values[] = {\n")
   for p in params:
      f.write("    { %s, %s },\n" % (p[0], p[1]))

   f.write("};\n\n")

def api_name(api):
   return "API_OPEN%s" % api

# This must match gl_api enum in src/mesa/main/mtypes.h
api_enum = [
   'GL',
   'GLES',
   'GLES2',
   'GL_CORE',
   'GLES3', # Not in gl_api enum in mtypes.h
   'GLES31', # Not in gl_api enum in mtypes.h
   'GLES32', # Not in gl_api enum in mtypes.h
]

def api_index(api):
    return api_enum.index(api)

def table_name(api):
   return "table_" + api_name(api)

def print_table(f: 'typing.TextIO', api, table):
   f.write("static table_t %s = {\n" % (table_name(api)))

   # convert sparse (index, value) table into a dense table
   dense_table = [0] * hash_table_size
   for i, v in table:
      dense_table[i] = v

   row_size = 4
   for i in range(0, hash_table_size, row_size):
      row = dense_table[i : i + row_size]
      idx_val = ["%4d" % v for v in row]
      f.write(" " * 4 + ", ".join(idx_val) + ",\n")

   f.write("};\n\n")

def print_tables(f: 'typing.TextIO', tables):
   for table in tables:
      print_table(f, table["apis"][0], table["indices"])

   dense_tables = ['NULL'] * len(api_enum)
   for table in tables:
      tname = table_name(table["apis"][0])
      for api in table["apis"]:
         i = api_index(api)
         dense_tables[i] = "&%s" % (tname)

   f.write("static table_t *table_set[] = {\n")
   for expr in dense_tables:
      f.write("   %s,\n" % expr)
   f.write("};\n\n")

   f.write("#define table(api) (*table_set[api])")

# Merge tables with matching parameter lists (i.e. GL and GL_CORE)
def merge_tables(tables):
   merged_tables = []
   for api, indices in sorted(tables.items()):
      matching_table = list(filter(lambda mt:mt["indices"] == indices,
                              merged_tables))
      if matching_table:
         matching_table[0]["apis"].append(api)
      else:
         merged_tables.append({"apis": [api], "indices": indices})

   return merged_tables

def add_to_hash_table(table, hash_val, value):
   while True:
      index = hash_val & (hash_table_size - 1)
      if index not in table:
         table[index] = value
         break
      hash_val += prime_step

def die(msg):
   sys.stderr.write("%s: %s\n" % (program, msg))
   exit(1)

program = os.path.basename(sys.argv[0])

def generate_hash_tables(enum_list, enabled_apis, param_descriptors):
   tables = defaultdict(lambda:{})

   # the first entry should be invalid, so that get.c:find_value can use
   # its index for the 'enum not found' condition.
   params = [[0, ""]]

   for param_block in param_descriptors:
      if set(["apis", "params"]) != set(param_block):
         die("missing fields (%s) in param descriptor file (%s)" %
               (", ".join(set(["apis", "params"]) - set(param_block)),
                param_desc_file))

      valid_apis = set(param_block["apis"])
      if valid_apis - gl_apis:
         die("unknown API(s) in param descriptor file (%s): %s\n" %
               (param_desc_file, ",".join(valid_apis - gl_apis)))

      if not (valid_apis & enabled_apis):
         continue

      valid_apis &= enabled_apis

      for param in param_block["params"]:
         enum_name = param[0]
         enum_val = enum_list[enum_name].value
         hash_val = enum_val * prime_factor

         for api in valid_apis:
            add_to_hash_table(tables[api], hash_val, len(params))
            # Also add GLES2 items to the GLES3+ hash tables
            if api == "GLES2":
               add_to_hash_table(tables["GLES3"], hash_val, len(params))
               add_to_hash_table(tables["GLES31"], hash_val, len(params))
               add_to_hash_table(tables["GLES32"], hash_val, len(params))
            # Also add GLES3 items to the GLES31+ hash tables
            if api == "GLES3":
               add_to_hash_table(tables["GLES31"], hash_val, len(params))
               add_to_hash_table(tables["GLES32"], hash_val, len(params))
            # Also add GLES31 items to the GLES32+ hash tables
            if api == "GLES31":
               add_to_hash_table(tables["GLES32"], hash_val, len(params))
         params.append(["GL_" + enum_name, param[1]])

   sorted_tables={}
   for api, indices in tables.items():
      sorted_tables[api] = sorted(indices.items())

   return params, merge_tables(sorted_tables)


if __name__ == '__main__':
   parser = argparse.ArgumentParser()
   parser.add_argument('output')
   parser.add_argument('-f', '--file', dest='api_desc_file', required=True, help="Specify GL API XML file" )
   args = parser.parse_args()

   # generate the code for all APIs
   enabled_apis = set(["GLES", "GLES2", "GLES3", "GLES31", "GLES32",
                       "GL", "GL_CORE"])

   try:
      api_desc = gl_XML.parse_GL_API(args.api_desc_file)
   except Exception:
      die("couldn't parse API specification file %s\n" % args.api_desc_file)

   (params, hash_tables) = generate_hash_tables(api_desc.enums_by_name,
                              enabled_apis, get_hash_params.descriptor)

   with open(args.output, 'w') as f:
      print_header(f)
      print_params(f, params)
      print_tables(f, hash_tables)
