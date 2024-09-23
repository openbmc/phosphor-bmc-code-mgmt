
#include "fw-update/common/create_package/create_pldm_fw_package.hpp"
#include "fw-update/common/pldm_fw_update_verifier.hpp"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>

int main()
{
    uint8_t component_image[] = {0x12, 0x34, 0x83, 0x21};

    size_t size_out = 0;
    std::shared_ptr<uint8_t[]> buf = create_pldm_package_buffer(
        component_image, sizeof(component_image), std::nullopt,
        std::optional<std::string>("com.my.compatible"), &size_out);

    const std::shared_ptr<PackageParser> pp =
        pldmutil::parsePLDMFWUPPackageComplete(buf.get(), size_out);

    if (pp == nullptr)
    {
        lg2::error("could not parse PLDM Package");
        return EXIT_FAILURE;
    }

    std::string compatible = pldmutil::getCompatibleFromPLDMPackage(pp);

    if (compatible.empty())
    {
        lg2::error(
            "could not extract compatible string from PLDM FW Update Package");
        return EXIT_FAILURE;
    }

    lg2::debug("compatible = {COMPATIBLE}", "COMPATIBLE", compatible);

    assert(compatible == "com.my.compatible");

    return EXIT_SUCCESS;
}
