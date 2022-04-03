#
# Copyright © 2022 Igalia S.L.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

import argparse
import os
import re
from typing import Dict

from attr import dataclass

from gpu_trace_aggregator import gpu_trace_parse_file, GPUEventAggregate


@dataclass
class GPURenderPassComparison:
    hash: str
    params: Dict[str, str]
    sysmem_rp: GPUEventAggregate
    gmem_rp: GPUEventAggregate


def dir_path(string):
    if os.path.isdir(string):
        return string
    else:
        raise NotADirectoryError(string)


parser = argparse.ArgumentParser(description='')
parser.add_argument('logs', type=dir_path)

args = parser.parse_args()

GPU_TRACE_RUN_DELIMETER = "TU: info: start of autotune results"

gpu_trace_params = ['width', 'height', 'layers', 'MRTs', 'max_samples',
                    'clearCPP', 'loadCPP', 'storeCPP', 'drawCount', 'totalDrawCost']
autotune_params = ['avg_passed', 'use_bypass_dynamic']
additional_params = ['gmem_mean', 'gmem_error', 'sysmem_mean', 'sysmem_error']

print(", ".join(additional_params + gpu_trace_params + autotune_params))

for sysmem_log in os.listdir(args.logs):
    if not sysmem_log.endswith(".sysmem.log"):
        continue

    gmem_log = sysmem_log.replace("sysmem", "gmem")
    sysmem_log = args.logs + sysmem_log
    gmem_log = args.logs + gmem_log

    in_sysmem = open(sysmem_log, 'r')
    sysmem_run = gpu_trace_parse_file(in_sysmem, GPU_TRACE_RUN_DELIMETER)

    in_gmem = open(gmem_log, 'r')
    gmem_run = gpu_trace_parse_file(in_gmem, GPU_TRACE_RUN_DELIMETER)

    render_pass_map = {}

    for idx, sysmem_rp in enumerate(sysmem_run.events):
        gmem_rp = gmem_run.events[idx]
        params = {}
        for match in re.findall('(\w+)=(\w+)', gmem_rp.description):
            params[match[0]] = match[1]

        if params["numberOfBins"] == "0":
            # Not a gmem, nothing to compare
            continue

        rp_comp = GPURenderPassComparison(
            params['hash'], params, sysmem_rp, gmem_rp)

        render_pass_map[rp_comp.hash] = rp_comp

    in_sysmem.seek(0, 0)
    autotune_results= [l for l in in_sysmem.readlines() if l.startswith("TU: info: rp_hash=")]

    for at_result in autotune_results:
        params = {}
        for match in re.findall('(\w+)=(\w+)', at_result):
            params[match[0]] = match[1]

        hash = params['rp_hash']

        render_pass_map[hash].params.update(params)

    for render_pass in render_pass_map.values():
        if "rp_hash" not in render_pass.params:
            continue

        print("{}, {:2.3f}, {}, {:2.3f}, ".format(render_pass.gmem_rp.duration.mean, render_pass.gmem_rp.duration.error,
                                                  render_pass.sysmem_rp.duration.mean, render_pass.sysmem_rp.duration.error), end="")
        for param in (gpu_trace_params + autotune_params):
            print(render_pass.params[param] + ", ", end="")
        print()
