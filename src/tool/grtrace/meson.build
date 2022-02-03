sizeof_wchar_t = cc.sizeof('wchar_t', args: ['-include', 'wchar.h'])

# Rename to glide.dll and make the target application load it.
dump_dll = shared_library(
  'glide_dump',
  ['src/dump.c'],
  c_args: ['-DSIZEOF_WCHAR_T=@0@'.format(sizeof_wchar_t)]
)

cargo = find_program('cargo')

sources = [
  'src/lib.rs',
  'src/parse.rs',
]

replay = custom_target(
  'grtrace-replay',
  input: [sources, 'src/bin/replay.rs'],
  output: 'replay',
  command: [
    cargo, 'build', '--release',
    '--manifest-path', '@CURRENT_SOURCE_DIR@/Cargo.toml',
    '--target-dir', '@PRIVATE_DIR@',
    '--bin', 'replay',
    # Required for --out-dir as of cargo 1.59.0.
    '-Z', 'unstable-options',
    '--out-dir', '@OUTDIR@',
  ],
  install: true,
  install_dir: 'bin',
)

extract = custom_target(
  'grtrace-extract',
  input: [sources, 'src/bin/extract.rs'],
  output: 'extract',
  command: [
    cargo, 'build', '--release',
    '--manifest-path', '@CURRENT_SOURCE_DIR@/Cargo.toml',
    '--target-dir', '@PRIVATE_DIR@',
    '--bin', 'extract',
    # Required for --out-dir as of cargo 1.59.0.
    '-Z', 'unstable-options',
    '--out-dir', '@OUTDIR@',
  ],
  install: true,
  install_dir: 'bin',
)
