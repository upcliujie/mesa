doxygen_paths = [
  'src/compiler/nir',
  'src/intel/isl',
]

input_files = []
foreach p : doxygen_paths
  input_files += meson.source_root() / p
endforeach

doxygen_config = configuration_data()
doxygen_config.set('input_files', ' '.join(input_files))
doxygen_config.set('output_xml', meson.current_build_dir())
doxyfile = configure_file(
  configuration: doxygen_config,
  input : 'Doxyfile.in',
  output: 'Doxyfile',
)

doxygen_xml = custom_target(
  'doxygen-xml',
  command: [doxygen, doxyfile],
  output: 'index.xml',
)
