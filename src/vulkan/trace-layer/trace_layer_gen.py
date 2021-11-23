# coding=utf-8
COPYRIGHT=u"""
/*
 * Copyright © 2015-2021 Intel Corporation
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */
"""

import argparse
import os
import sys

from mako.template import Template

VULKAN_UTIL = os.path.abspath(os.path.join(os.path.dirname(__file__), '../util'))
sys.path.append(VULKAN_UTIL)

# Mesa-local imports must be declared in meson variable
# '{file_without_suffix}_depend_files'.
from vk_dispatch_table_gen import get_entrypoints_from_xml

TEMPLATE_H = Template(COPYRIGHT + """
/* This file generated from ${filename}, don't edit directly. */

#pragma once

#include <vulkan/vulkan.h>

<%def name="guard_begin(e)">\\
% if e.guard is not None:
#ifdef ${e.guard}
% endif
</%def>

<%def name="guard_end(e)">\\
% if e.guard is not None:
#endif
% endif
</%def>

<%def name="define_dispatch_table(name, entrypoints)">
struct Trace${name}DispatchTable {
% for e in entrypoints:
${guard_begin(e)}\\
   PFN_vk${e.name} ${e.name};
${guard_end(e)}\\
% endfor

   Trace${name}DispatchTable(Vk${name} obj, PFN_vkGet${name}ProcAddr gpa) {
% for e in entrypoints:
${guard_begin(e)}\\
      ${e.name} = reinterpret_cast<PFN_vk${e.name}>(gpa(obj, "vk${e.name}"));
${guard_end(e)}\\
% endfor
    }
};
</%def>

${define_dispatch_table("Instance", instance_entrypoints)}
${define_dispatch_table("Device", device_entrypoints)}

PFN_vkVoidFunction
traceInterceptInstanceProcAddr(const char *pName);

PFN_vkVoidFunction
traceInterceptDeviceProcAddr(const char *pName);
""", output_encoding='utf-8')

TEMPLATE_CC = Template(COPYRIGHT + """
/* This file generated from ${filename}, don't edit directly. */

#include "trace_layer.h"

namespace {

<%def name="guard_begin(e)">\\
% if e.guard is not None:
#ifdef ${e.guard}
% endif
</%def>

<%def name="guard_end(e)">\\
% if e.guard is not None:
#endif
% endif
</%def>

<%def name="dispatch_pre(e)">\\
% if e.name == 'GetInstanceProcAddr':
   if (!strcmp(pName, "vkCreateInstance"))
      return traceInterceptInstanceProcAddr(pName);
% endif
</%def>

<%def name="dispatch(e)">\\
% if e.name in ['CreateInstance', 'EnumeratePhysicalDevices', 'EnumeratePhysicalDeviceGroups', 'EnumeratePhysicalDeviceGroupsKHR', 'EnumerateDeviceExtensionProperties']:
   auto ret = TraceInstance::${e.name}(${e.call_params()});
% elif e.name in ['CreateDevice', 'CreateCommandPool', 'AllocateCommandBuffers']:
   auto ret = TraceDevice::${e.name}(${e.call_params()});
% elif e.name in ['GetDeviceQueue', 'GetDeviceQueue2', 'FreeCommandBuffers']:
   TraceDevice::${e.name}(${e.call_params()});
% elif e.name in ['QueueSubmit']:
   auto ret = TraceQueue::${e.name}(${e.call_params()});
% elif e.name in ['CmdExecuteCommands']:
   TraceCommandBuffer::${e.name}(${e.call_params()});
% else:
<%
param_names = []
for i, p in enumerate(e.params):
    if i == 0 or p.type in ['VkCommandPool']:
        param_names.append('traceUnwrap(%s)' % p.name)
    else:
        param_names.append(p.name)
call_params = ', '.join(param_names)
expr = 'traceDispatch(%s).%s(%s) ' % (e.params[0].name, e.name, call_params)
%>\\
  % if e.return_type != 'void':
   auto ret = ${expr};
  % else:
   ${expr};
  % endif:
% endif
</%def>

<%def name="dispatch_post(e)">\\
% if e.name in ['GetInstanceProcAddr', 'GetDeviceProcAddr']:
   if (ret) {
      const auto intercepted = trace${e.name.replace('Get', 'Intercept', 1)}(pName);
      if (intercepted)
         ret = intercepted;
   }
% elif e.name == 'DestroyInstance':
   delete traceFrom(instance);
% elif e.name == 'DestroyDevice':
   delete traceFrom(device);
% elif e.name == 'DestroyCommandPool':
   delete traceFrom(commandPool);
% endif
</%def>

% for e in instance_entrypoints + device_entrypoints:

${guard_begin(e)}\\
VKAPI_ATTR ${e.return_type} VKAPI_CALL
trace${e.name}(${e.decl_params()})
{
% if e.name.startswith('Cmd'):
   TRACE_SLOW("trace${e.name}");
% else:
   TRACE("trace${e.name}");
% endif
${dispatch_pre(e)}\\
${dispatch(e)}\\
${dispatch_post(e)}\\
% if e.return_type != 'void':
   return ret;
% endif
}
${guard_end(e)}\\
% endfor

} // anonymous namespace

PFN_vkVoidFunction
traceInterceptInstanceProcAddr(const char *pName)
{
   // TODO binary search
% for e in instance_entrypoints:
${guard_begin(e)}\\
   if (!strcmp(pName, "vk${e.name}"))
      return reinterpret_cast<PFN_vkVoidFunction>(trace${e.name});
${guard_end(e)}\\
% endfor

   return traceInterceptDeviceProcAddr(pName);
}

PFN_vkVoidFunction
traceInterceptDeviceProcAddr(const char *pName)
{
   // TODO binary search
% for e in device_entrypoints:
${guard_begin(e)}\\
   if (!strcmp(pName, "vk${e.name}"))
      return reinterpret_cast<PFN_vkVoidFunction>(trace${e.name});
${guard_end(e)}\\
% endfor

   return NULL;
}
""", output_encoding='utf-8')

class Handle:
    def __init__(self, handle, name):
        self.handle = handle
        self.name = name
        self.entrypoints = []

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out-cc', required=True, help='Output C++ file.')
    parser.add_argument('--out-h', required=True, help='Output H file.')
    parser.add_argument('--xml',
                        help='Vulkan API XML file.',
                        required=True, action='append', dest='xml_files')
    args = parser.parse_args()

    assert os.path.dirname(args.out_cc) == os.path.dirname(args.out_h)

    entrypoints = get_entrypoints_from_xml(args.xml_files)

    instance_entrypoints = []
    device_entrypoints = []
    for e in entrypoints:
        if e.is_device_entrypoint():
            device_entrypoints.append(e)
        elif e.params[0].type in ['VkInstance', 'VkPhysicalDevice']:
            instance_entrypoints.append(e)
        elif e.name in ['CreateInstance']:
            instance_entrypoints.append(e)

    environment = {
        'filename': os.path.basename(__file__),
        'instance_entrypoints': instance_entrypoints,
        'device_entrypoints': device_entrypoints,
    }
    try:
        with open(args.out_h, 'wb') as f:
            f.write(TEMPLATE_H.render(**environment))
        with open(args.out_cc, 'wb') as f:
            f.write(TEMPLATE_CC.render(**environment))
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
