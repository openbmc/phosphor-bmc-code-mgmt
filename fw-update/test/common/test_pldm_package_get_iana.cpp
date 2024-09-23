
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
    uint8_t component_image[] = {0x12, 0x34, 0xab};

    size_t size_out = 0;
    std::shared_ptr<uint8_t[]> buf = create_pldm_package_buffer(
        component_image, sizeof(component_image),
        std::optional<uint32_t>(0xdcbaff), std::nullopt, &size_out);

    const std::shared_ptr<PackageParser> pp =
        parsePLDMFWUPPackageComplete(buf, size_out);

    if (pp == nullptr)
    {
        lg2::error("could not parse PLDM Package");
        return EXIT_FAILURE;
    }

    const int64_t iana = getIANAFromPLDMPackage(pp);

    if (iana < 0)
    {
        lg2::error("could not extract IANA from PLDM FW Update Package");
        return EXIT_FAILURE;
    }

    lg2::debug("iana = {IANA}", "IANA", lg2::hex, iana);

    assert(iana == 0xdcbaff);

    return EXIT_SUCCESS;
}
