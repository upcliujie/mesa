if with_android_stub
  stub_libs = []

  foreach lib : ['backtrace', 'cutils', 'hardware', 'log', 'sync']
    stub_libs += shared_library(
      lib,
      files(lib + '_stub.cpp'),
      include_directories : inc_include,
      install : false,
    )
  endforeach

  if get_option('platform-sdk-version') >= 26
    stub_libs += shared_library(
      'nativewindow',
      files('nativewindow_stub.cpp'),
      include_directories : inc_include,
      install : false,
    )
  endif

  dep_android = declare_dependency(
    link_with : stub_libs,
  )
endif
