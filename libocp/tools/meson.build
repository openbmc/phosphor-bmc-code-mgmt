if cpp.has_header('CLI/CLI.hpp')
    cli11_dep = declare_dependency()
else
    cli11_dep = dependency('CLI11')
endif

executable(
    'ocp-recovery-tool',
    'ocp_recovery_tool.cpp',
    dependencies: [libocp_dep, cli11_dep],
    install: true,
)
