
#include "fw-update/common/pldm_fw_update_verifier.hpp"
#include "phosphor-logging/lg2.hpp"

#include <fcntl.h>
#include <inttypes.h>
/*
#include <libpldm/base.h>
#include <libpldm/firmware_update.h>
#include <libpldm/pldm_types.h>
#include <libpldm/utils.h>
*/
#include "create_pldm_fw_package.hpp"

#include <unistd.h>

#include <cstdlib>
#include <cstring>

int main()
{
    std::string filename = create_pldm_package();

    if (filename.empty())
    {
        return 1;
    }

    int fd = open(filename.c_str(), O_RDONLY);

    if (fd < 0)
    {
        lg2::error("could not create sample pldm fw update package");
        return 1;
    }

    sdbusplus::message::unix_fd image;
    image.fd = fd;

    int res = verifyPLDMPackage(image);

    close(fd);

    return res;
}
