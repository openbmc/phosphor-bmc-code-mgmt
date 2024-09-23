
tool_src = files(
  'main.cpp',
  'info.cpp',
  'create.cpp',
)

tool_include = include_directories('.')

executable(
    'pldm-update-pkg-tool',
    tool_src,
    common_src,
    include_directories: [
      common_include,
      tool_include,
    ],
    dependencies: [
      common_deps,
    ],
    install: true
)
