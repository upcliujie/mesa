project(
  'D3D12TranslationLayer',
  'cpp',
  license : 'MIT',
  version : 'cb629b42264a0a9d882c9b8c0d013bf5d9901e5a',
  meson_version : '>= 0.55.0',
)

d3d12translationlayer_incdir = include_directories('include')

d3d12translationlayer_sources = files(
  'src/ShaderBinary.cpp',
  'src/DxbcBuilder.cpp',
)

d3d12translationlayer_lib = library(
  'd3d12translationlayer',
  d3d12translationlayer_sources,
  include_directories: d3d12translationlayer_incdir,
)

d3d12translationlayer_dep = declare_dependency(
  link_with : d3d12translationlayer_lib,
  dependencies : [dep_dxheaders],
  include_directories : d3d12translationlayer_incdir
)
