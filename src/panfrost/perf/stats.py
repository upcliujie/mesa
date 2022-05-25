import sys
import json

def percentage(x):
    return '{:>5.2f}%'.format(100.0 * x)

c = json.load(sys.stdin)
for k in c:
    globals()[k] = c[k]

LS = ls_mem_read_full + ls_mem_read_short + ls_mem_write_full + ls_mem_write_short + ls_mem_atomic
V = vary_slot_32 + vary_slot_16
T = tex_filt_num_operations

SFU = exec_instr_sfu * 4
FMA = exec_instr_fma
CVT = exec_instr_cvt

A = max(SFU, FMA, CVT)

print(f"Fragment:        {percentage(js0_active / gpu_active)}")
print(f"Non-fragment:    {percentage(js1_active / gpu_active)}")
print(f"")
print(f"Arithmetic:      {percentage(A / exec_core_active)}")
print(f"Varying:         {percentage(V / exec_core_active)}")
print(f"Load/store:      {percentage(LS / exec_core_active)}")
print(f"Texture:         {percentage(T / exec_core_active)}")
print(f"Starving:        {percentage(exec_starve_arith / exec_core_active)}")
print(f"")
print(f"FMA:             {percentage(FMA / (FMA + CVT + SFU))}")
print(f"CVT:             {percentage(CVT / (FMA + CVT + SFU))}")
print(f"SFU:             {percentage(SFU / (FMA + CVT + SFU))}")
print(f"")
print(f"Full warps:      {percentage(full_quad_warps / (frag_warps + compute_warps))}")
print(f"All reg warps:   {percentage(warp_reg_size_64 / (frag_warps + compute_warps))}")
print(f"16-bit varyings: {percentage(vary_slot_16 / (vary_slot_16 + vary_slot_32))}")
if call_blend_shader:
    print(f"Blend shaders:   {call_blend_shader}")
if exec_icache_miss:
    print(f"I-cache misses:  {exec_icache_miss}")
