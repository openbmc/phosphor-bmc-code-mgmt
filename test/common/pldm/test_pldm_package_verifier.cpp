
#include "common/pldm/pldm_package_util.hpp"
#include "test/create_package/create_pldm_fw_package.hpp"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>

int main()
{
    uint8_t component_image[] = {0xab, 0xba};
    size_t component_image_size = 2;

    std::optional<std::string> optFilename =
        create_pldm_package(component_image, component_image_size);

    assert(optFilename.has_value());

    int fd = open(optFilename.value().c_str(), O_RDONLY);

    if (fd < 0)
    {
        lg2::error("could not create sample pldm fw update package");
        return 1;
    }

    sdbusplus::message::unix_fd image;
    image.fd = fd;

    size_t data_size;
    void* data = pldm_package_util::mmap_pldm_package(image, &data_size);

    int res = pldm_package_util::verifyPLDMPackage(data, data_size);

    close(fd);

    return res;
}
