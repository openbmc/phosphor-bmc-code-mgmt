
executable(
    'pldm-update-pkg-tool',
    'main.cpp',
    'info.cpp',
    'create.cpp',
    common_src,
    include_directories: [
      common_include,
      '.',
    ],
    dependencies: [
      common_deps,
    ],
    link_with: [libpldmutil, libpldmcreatepkg],
    install: true
)
