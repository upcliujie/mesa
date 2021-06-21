subdir('doxygen')

doxygen_dir = meson.current_build_dir() / 'doxygen'

html_docs = custom_target(
  'mesa-html-docs',
  depfile: 'docs.d',
  depend_files: files('conf.py'),
  depends: doxygen_xml,
  command: [sphinx, '-b', 'html', '-q',
            '-Ddepfile=@DEPFILE@',
            '-Dbreathe_projects.mesa=' + doxygen_dir,
            meson.current_source_dir(), '@OUTPUT@'],
  output: 'public',
  build_by_default: get_option('html-docs').enabled(),
)

install_subdir(html_docs.full_path(),
  install_dir: join_paths(get_option('datadir'), 'doc', 'mesa'),
  exclude_files: '.buildinfo',
  strip_directory: true
)
