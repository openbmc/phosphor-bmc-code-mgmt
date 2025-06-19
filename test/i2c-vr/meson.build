
executable(
    'xdpe1x2xx-manual-update',
    'xdpe1x2xx_manual_update.cpp',
    regulators_src,
    include_directories: ['.', common_include, i2cvr_include],
    dependencies: [phosphor_logging_dep, sdbusplus_dep],
    link_with: [libi2c_dev],
    link_args: '-li2c',
)
