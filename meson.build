project('phosphor-bmc-code-mgmt', 'cpp',
    default_options: [
        'warning_level=3',
        'werror=true',
        'cpp_std=c++17'
    ],
    license: 'Apache-2.0',
    version: '1.0')

conf = configuration_data()

# DBus information
conf.set_quoted('BMC_INVENTORY_INTERFACE', 'xyz.openbmc_project.Inventory.Item.Bmc')
conf.set_quoted('BUSNAME_UPDATER', 'xyz.openbmc_project.Software.BMC.Updater')
conf.set_quoted('DOWNLOAD_BUSNAME', 'xyz.openbmc_project.Software.Download')
conf.set_quoted('FILEPATH_IFACE', 'xyz.openbmc_project.Common.FilePath')
conf.set_quoted('INVENTORY_PATH', '/xyz/openbmc_project/inventory/')
conf.set_quoted('MAPPER_BUSNAME', 'xyz.openbmc_project.ObjectMapper')
conf.set_quoted('MAPPER_INTERFACE', 'xyz.openbmc_project.ObjectMapper')
conf.set_quoted('MAPPER_PATH', '/xyz/openbmc_project/object_mapper')
conf.set_quoted('SOFTWARE_OBJPATH', '/xyz/openbmc_project/software')
conf.set_quoted('SYSTEMD_BUSNAME', 'org.freedesktop.systemd1')
conf.set_quoted('SYSTEMD_PATH', '/org/freedesktop/systemd1')
conf.set_quoted('SYSTEMD_INTERFACE', 'org.freedesktop.systemd1.Manager')
conf.set_quoted('VERSION_BUSNAME', 'xyz.openbmc_project.Software.Version')
conf.set_quoted('VERSION_IFACE', 'xyz.openbmc_project.Software.Version')

# Names of the forward and reverse associations
conf.set_quoted('ACTIVATION_FWD_ASSOCIATION', 'inventory')
conf.set_quoted('ACTIVATION_REV_ASSOCIATION', 'activation')
conf.set_quoted('ACTIVE_FWD_ASSOCIATION', 'active')
conf.set_quoted('ACTIVE_REV_ASSOCIATION', 'software_version')
conf.set_quoted('FUNCTIONAL_FWD_ASSOCIATION', 'functional')
conf.set_quoted('FUNCTIONAL_REV_ASSOCIATION', 'software_version')
conf.set_quoted('UPDATEABLE_FWD_ASSOCIATION', 'updateable')
conf.set_quoted('UPDATEABLE_REV_ASSOCIATION', 'software_version')

# Filesystem files and directories
# The path of the alt rwfs overlay
conf.set_quoted('ALT_RWFS', '/media/alt/var/persist')
# The prefix path for the versioned read-only bmc partitions
conf.set_quoted('BMC_ROFS_PREFIX', get_option('media-dir') + '/rofs-')
# The name of the BMC table of contents file
conf.set_quoted('OS_RELEASE_FILE', '/etc/os-release')
# The dir where activation data is stored in files
conf.set_quoted('PERSIST_DIR', '/var/lib/phosphor-bmc-code-mgmt/')

# Supported BMC layout types
conf.set('STATIC_LAYOUT', get_option('bmc-layout').contains('static'))
conf.set('UBIFS_LAYOUT', get_option('bmc-layout').contains('ubi'))

# Configurable features
conf.set('HOST_BIOS_UPGRADE', get_option('host-bios-upgrade').enabled())
conf.set('WANT_SIGNATURE_VERIFY', get_option('verify-signature').enabled())

# Configurable variables
conf.set('ACTIVE_BMC_MAX_ALLOWED', get_option('active-bmc-max-allowed'))
conf.set_quoted('HASH_FILE_NAME', get_option('hash-file-name'))
conf.set_quoted('IMG_UPLOAD_DIR', get_option('img-upload-dir'))
conf.set_quoted('MANIFEST_FILE_NAME', get_option('manifest-file-name'))
conf.set_quoted('MEDIA_DIR', get_option('media-dir'))
conf.set_quoted('PUBLICKEY_FILE_NAME', get_option('publickey-file-name'))
conf.set_quoted('SIGNATURE_FILE_EXT', get_option('signature-file-ext'))
conf.set_quoted('SIGNED_IMAGE_CONF_PATH', get_option('signed-image-conf-path'))
conf.set_quoted('SYNC_LIST_DIR_PATH', get_option('sync-list-dir-path'))
conf.set_quoted('SYNC_LIST_FILE_NAME', get_option('sync-list-file-name'))

configure_file(output: 'config.h', configuration: conf)

sdbuspp = find_program('sdbus++')
subdir('xyz/openbmc_project/Software/Image')
