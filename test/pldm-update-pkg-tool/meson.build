
executable(
    'pldm-update-pkg-tool',
    'main.cpp',
    'info.cpp',
    'create.cpp',
    include_directories: [
      common_include,
      '.',
    ],
    dependencies: [
      libpldm_dep,
      phosphor_logging_dep,
    ],
    link_with: [libpldmutil, libpldmcreatepkg],
    install: false
)
