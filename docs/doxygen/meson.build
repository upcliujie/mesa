doxygen_config = configuration_data()
doxygen_config.set('input_files', meson.source_root() / 'src' / 'intel' / 'isl')
doxygen_config.set('output_xml', meson.current_build_dir())
doxyfile = configure_file(
  configuration: doxygen_config,
  input : 'Doxyfile.in',
  output: 'Doxyfile',
)

doxygen_xml = custom_target(
  'doxygen-docs',
  command: [doxygen, doxyfile],
  output: 'index.xml',
)
