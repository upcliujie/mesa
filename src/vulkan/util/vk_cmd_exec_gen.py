# coding=utf-8
COPYRIGHT=u"""
/* Copyright © 2015-2021 Intel Corporation
 * Copyright © 2021 Collabora, Ltd.
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

import argparse
import os
import re
import xml.etree.ElementTree as et

from mako.template import Template

# Mesa-local imports must be declared in meson variable
# '{file_without_suffix}_depend_files'.
from vk_entrypoints import EntrypointParam
from vk_dispatch_table_gen import get_entrypoints_from_xml

MANUAL_COMMANDS = ['CmdPushDescriptorSetKHR',             # This script doesn't know how to copy arrays in structs in arrays
                   'CmdPushDescriptorSetWithTemplateKHR', # pData's size cannot be calculated from the xml
                   'CmdDrawMultiEXT',                     # The size of the elements is specified in a stride param
                   'CmdDrawMultiIndexedEXT',              # The size of the elements is specified in a stride param
                   'CmdExecuteCommands',                  # Only supported on primary command buffers
                   'CmdBeginRendering',                   # The VkPipelineLayout object could be released before the command is executed
                   'CmdBeginRenderingKHR',                # The VkPipelineLayout object could be released before the command is executed
                  ]

TEMPLATE_H = Template(COPYRIGHT + """\
/* This file generated from ${filename}, don't edit directly. */

#ifndef ${guard}
#define ${guard}

#ifdef __cplusplus
extern "C" {
#endif

VKAPI_ATTR void VKAPI_CALL
${prefix}_CmdExecuteCommands(VkCommandBuffer        commandBuffer,
                             uint32_t               commandBufferCount,
                             const VkCommandBuffer* pCommandBuffers);

#ifdef __cplusplus
}
#endif

#endif /* ${guard} */
""", output_encoding='utf-8')

TEMPLATE_C = Template(COPYRIGHT + """
/* This file generated from ${filename}, don't edit directly. */

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#include "pipe/p_context.h"
#include "vk_cmd_queue.h"
#include "vk_command_buffer.h"
#include "vk_util.h"
#include "${header}"

% for c in commands:
% if c.guard is not None:
#ifdef ${c.guard}
% endif
VKAPI_ATTR ${c.return_type} VKAPI_CALL ${primary_prefix}_${c.name} (VkCommandBuffer commandBuffer
% for p in c.params[1:]:
, ${p.decl}
% endfor
);
% if c.guard is not None:
#endif // ${c.guard}
% endif
% endfor

VKAPI_ATTR void VKAPI_CALL
${prefix}_CmdExecuteCommands(VkCommandBuffer        commandBuffer,
                             uint32_t               commandBufferCount,
                             const VkCommandBuffer* pCommandBuffers)
{
   struct vk_cmd_queue_entry *cmd;

   for (uint32_t i = 0; i < commandBufferCount; i++) {
      VK_FROM_HANDLE(vk_command_buffer, cmd_buffer, pCommandBuffers[i]);
      LIST_FOR_EACH_ENTRY(cmd, &cmd_buffer->queue.cmds, cmd_link) {
         switch (cmd->type) {
% for c in commands:
% if c.guard is not None:
#ifdef ${c.guard}
% endif
         case ${to_enum_name(c.name)}:
             ${primary_prefix}_${c.name}(commandBuffer\\
% if len(c.params) > 1:
, ${to_enum_name(c.name)}_ARGS(cmd)\\
% endif
);
             break;
% if c.guard is not None:
#endif // ${c.guard}
% endif
% endfor
         default: unreachable("Unsupported command");
         }
      }
   }
}
""", output_encoding='utf-8')

def remove_prefix(text, prefix):
    if text.startswith(prefix):
        return text[len(prefix):]
    return text

def to_underscore(name):
    return remove_prefix(re.sub('([A-Z]+)', r'_\1', name).lower(), '_')

def to_enum_name(name):
    return "VK_%s" % to_underscore(name).upper()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out-h', required=True, help='Output header file.')
    parser.add_argument('--out-c', required=True, help='Output C file.')
    parser.add_argument('--xml',
                        help='Vulkan API XML file.',
                        required=True, action='append', dest='xml_files')
    parser.add_argument('--prefix',
                        help='Prefix to use for all dispatch tables.',
                        required=True, default='', dest='prefix')
    parser.add_argument('--primary-prefix',
                        help='Prefix to use for all dispatch tables.',
                        required=True, default='', dest='primary_prefix')
    parser.add_argument('--functions-list',
                        help='File containing a list of functions to issue wrappers for.',
                        required=False, default='', dest='functions_file')

    args = parser.parse_args()

    funcs = None
    if args.functions_file != '':
        file = open(args.functions_file, 'r')
        lines = file.readlines()
        funcs = {}
        for line in lines:
            funcs[line.rstrip("\n")] = True

    commands = []
    for e in get_entrypoints_from_xml(args.xml_files):
        if e.name.startswith('Cmd') and not e.alias and e.name not in MANUAL_COMMANDS and (funcs == None or e.name in funcs):
            commands.append(e)

    environment = {
        'commands': commands,
        'prefix': args.prefix,
        'primary_prefix': args.primary_prefix,
        'filename': os.path.basename(__file__),
        'to_underscore': to_underscore,
        'to_enum_name': to_enum_name,
    }

    try:
        with open(args.out_h, 'wb') as f:
            guard = os.path.basename(args.out_h).replace('.', '_').upper()
            f.write(TEMPLATE_H.render(guard=guard, **environment))
        with open(args.out_c, 'wb') as f:
            f.write(TEMPLATE_C.render(header=os.path.basename(args.out_h), **environment))
    except Exception:
        # In the event there's an error, this imports some helpers from mako
        # to print a useful stack trace and prints it, then exits with
        # status 1, if python is run with debug; otherwise it just raises
        # the exception
        if __debug__:
            import sys
            from mako import exceptions
            sys.stderr.write(exceptions.text_error_template().render() + '\n')
            sys.exit(1)
        raise

if __name__ == '__main__':
    main()
