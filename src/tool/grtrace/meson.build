sizeof_wchar_t = cc.sizeof('wchar_t', args: ['-include', 'wchar.h'])

# Rename to glide.dll and make the target application load it.
dump_dll = shared_library(
  'glide_dump',
  ['src/dump.c'],
  c_args: ['-DSIZEOF_WCHAR_T=@0@'.format(sizeof_wchar_t)]
)
