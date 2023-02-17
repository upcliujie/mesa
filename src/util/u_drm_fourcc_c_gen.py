copyright = """\
/* Copyright (C) 2022 Intel Corporation
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
"""

import re
import sys
from mako.template import Template

template = copyright + """\

#include <stdlib.h>
#include <string.h>
#include "drm-uapi/drm_fourcc.h"
#include "u_drm_fourcc.h"
#include "macros.h"

static struct fourcc_mod {
   const char *name;
   uint64_t mod;
} mods_sorted_by_name[] = {
%    for mod in mods_by_name:
   { .name = "${mod[0]}",
     .mod  =  ${mod[0]} /* 0x${hex(mod[1])} */, },
%    endfor
};

static int
bsearch_mod_cmp(const void *m1, const void *m2)
{
   return strcmp(((const struct fourcc_mod *)m1)->name,
                 ((const struct fourcc_mod *)m2)->name);
}

uint64_t
u_get_drm_fourcc_modifier_by_name(const char *name)
{
   const struct fourcc_mod key = { .name = name, };
   const struct fourcc_mod *found =
      bsearch(&key, mods_sorted_by_name, ARRAY_SIZE(mods_sorted_by_name),
              sizeof(mods_sorted_by_name[0]), bsearch_mod_cmp);
   return found != NULL ? found->mod : DRM_FORMAT_MOD_INVALID;
}

uint64_t
u_get_drm_fourcc_modifier_from_string(const char *str)
{
   if (str != NULL) {
      uint64_t mod = u_get_drm_fourcc_modifier_by_name(str);
      if (mod == DRM_FORMAT_MOD_INVALID && str[0] != '\\0') {
         char *end;
         unsigned long long ull = strtoull(str, &end, 16);
         if (*end != '\\0')
            return DRM_FORMAT_MOD_INVALID;
         else
            mod = (uint64_t)ull;
      }
      return mod;
   }
   return DRM_FORMAT_MOD_INVALID;
}

/* We might need to grow the type as drm_fourcc.h grows */
typedef uint8_t index_t;

static index_t sorted_mod_indices[] = {
%    for i in mod_indices:
   ${i} /* 0x${hex(mods_by_name[i][1])}: ${mods_by_name[i][0]} */,
%    endfor
};

const char *
u_get_drm_fourcc_modifier_name(uint64_t mod)
{
   size_t low = 0, high = ARRAY_SIZE(sorted_mod_indices), i, j;
   while (low < high) {
      i = low + (high - low) / 2;
      j = sorted_mod_indices[i];
      if (mods_sorted_by_name[j].mod < mod) {
         low = i + 1;
      } else if (mods_sorted_by_name[j].mod > mod) {
         high = i;
      } else {
         return mods_sorted_by_name[j].name;
      }
   }
   return NULL;
}

bool
u_get_drm_fourcc_modifier_n_by_name(int n, const char **name, uint64_t *mod)
{
   if (n < 0 || n >= ARRAY_SIZE(mods_sorted_by_name))
      return false;

   if (name != NULL)
      *name = mods_sorted_by_name[n].name;
   if (mod != NULL)
      *mod = mods_sorted_by_name[n].mod;

   return true;
}

bool
u_get_drm_fourcc_modifier_n_by_mod(int n, const char **name, uint64_t *mod)
{
   if (n < 0 || n >= ARRAY_SIZE(sorted_mod_indices))
      return false;

   if (name != NULL)
      *name = mods_sorted_by_name[sorted_mod_indices[n]].name;
   if (mod != NULL)
      *mod = mods_sorted_by_name[sorted_mod_indices[n]].mod;

   return true;
}
"""

printer_template = copyright + """\

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include "drm-uapi/drm_fourcc.h"

int
main(int ac, char **as)
{
%    for mod in mods:
   printf("${mod}" ", 0x%" PRIx64 "\\n",
          (uint64_t)${mod});
%    endfor
};
"""

if sys.argv[1] == '--gen-printer':
    mod_define_regex = re.compile(r'^#define\s+((DRM|I915)_FORMAT_MOD_[A-Za-z0-9_]+)\s+.*')

    mods = []
    drm_fourcc_h = open(sys.argv[2])
    for line in drm_fourcc_h.readlines():
        mo = mod_define_regex.match(line)
        if mo is not None:
            mods.append(mo.group(1))
    mods.sort()

    templ = Template(printer_template, output_encoding='utf-8')
    sys.stdout.write(templ.render(mods=mods).decode())
elif sys.argv[1] == '--gen-util-c':
    modifiers_csv = open(sys.argv[2])
    mods = []
    for line in modifiers_csv.readlines():
        line = line.strip()
        if line == '':
            continue
        vs = [ v.strip() for v in line.split(',')  ]
        if (len(vs) != 2):
            raise Exception(f"{line} : {repr(vs)}")
        assert(len(vs) == 2)
        vs[1] = int(vs[1], base=0)
        mods.append(vs)
    # Verify names are sorted
    assert(all(mods[i][0] <= mods[i+1][0] for i in range(len(mods) - 1)))
    mods = [ m for m in mods if not m[0].startswith('DRM_FORMAT_MOD_VENDOR_') ]
    names = [ m[0] for m in mods ]
    sorted_by_mod = [ (mods[i][0], mods[i][1], i) for i in range(len(mods)) ]
    sorted_by_mod = sorted(sorted_by_mod, key=lambda m: m[1])
    mod_indices = [ m[2] for m in sorted_by_mod ]
    templ_params = {
        'mods_by_name': mods,
        'mod_indices': mod_indices,
    }
    templ = Template(template, output_encoding='utf-8')
    sys.stdout.write(templ.render(**templ_params).decode())
else:
    raise Exception(f"Unsupported command {sys.argv[1]}")
