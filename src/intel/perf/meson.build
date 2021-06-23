intel_hw_metrics = [
  'hsw',
  'bdw', 'chv',
  'sklgt2', 'sklgt3', 'sklgt4',
  'kblgt2', 'kblgt3',
  'cflgt2', 'cflgt3',
  'bxt', 'glk',
  'icl', 'ehl',
  'tglgt1', 'tglgt2', 'rkl', 'dg1', 'adl',
]

intel_hw_metrics_xml_files = []
foreach hw : intel_hw_metrics
  intel_hw_metrics_xml_files += 'oa-@0@.xml'.format(hw)
endforeach
intel_hw_metrics_xml_files = files(intel_hw_metrics_xml_files)

intel_perf_sources = [
  'intel_perf.c',
  'intel_perf_query.c',
  'intel_perf_mdapi.c',
]

# Lots of equations end up being similar, so group them all in a single
# generated file.
intel_perf_sources += custom_target(
  'intel-perf-equations',
  input : [ 'gen_equations.py', 'codegen.py' ] + intel_hw_metrics_xml_files,
  output : [ 'intel_perf_equations.c', 'intel_perf_equations.h' ],
  command : [
    prog_python, '@INPUT0@',
    '--code', '@OUTPUT0@',
    '--header', '@OUTPUT1@',
    intel_hw_metrics_xml_files,
  ])

# Then build metrics & registers in their own file per HW generation.
foreach hw : intel_hw_metrics
  intel_perf_sources += custom_target(
    'intel-perf-registers-@0@'.format(hw),
    input : [ 'gen_registers.py', 'codegen.py', 'oa-@0@.xml'.format(hw) ],
    output : [ 'intel_perf_registers_@0@.c'.format(hw),
               'intel_perf_registers_@0@.h'.format(hw), ],
    command : [
      prog_python, '@INPUT0@',
      '--code', '@OUTPUT0@',
      '--header', '@OUTPUT1@',
      '--xml-file', '@INPUT2@'
    ])
  intel_perf_sources += custom_target(
    'intel-perf-metrics-@0@'.format(hw),
    input : [ 'gen_metricsets.py', 'codegen.py', 'oa-@0@.xml'.format(hw) ],
    output : [ 'intel_perf_metrics_@0@.c'.format(hw),
               'intel_perf_metrics_@0@.h'.format(hw), ],
    command : [
      prog_python, '@INPUT0@',
      '--code', '@OUTPUT0@',
      '--header', '@OUTPUT1@',
      '--equations-include', 'intel_perf_equations.h',
      '--registers-include', 'intel_perf_registers_@0@.h'.format(hw),
      '--xml-file', '@INPUT2@',
    ])
endforeach

libintel_perf = static_library(
  'intel_perf',
  intel_perf_sources,
  include_directories : [
    inc_include, inc_src, inc_intel,
  ],
  c_args : [no_override_init_args, '-msse2'],
  cpp_args : ['-msse2'],
  gnu_symbol_visibility : 'hidden',
)
