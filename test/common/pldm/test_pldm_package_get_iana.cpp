
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
    uint8_t component_image[] = {0x12, 0x34, 0xab};

    size_t size_out = 0;
    std::unique_ptr<uint8_t[]> buf = create_pldm_package_buffer(
        component_image, sizeof(component_image),
        std::optional<uint32_t>(0xdcbaff), std::nullopt, size_out);

    const std::shared_ptr<PackageParser> pp =
        pldm_package_util::parsePLDMFWUPPackageComplete(buf.get(), size_out);

    assert(pp != nullptr);

    auto fwDeviceIdRecords = pp->getFwDeviceIDRecords();
    if (fwDeviceIdRecords.size() != 1)
    {
        lg2::error(
            "fw device id records != 1, rejecting package, incompatible.");
        return -1;
    }

    const FirmwareDeviceIDRecord& record = fwDeviceIdRecords[0];

    Descriptors desc = std::get<3>(record);
    if (desc.empty())
    {
        lg2::error("there is no descriptor for the applicable device");
        return -1;
    }

    if (!desc.contains(1))
    {
        // iana type descriptor
        lg2::error("the package does not have an IANA type descriptor");
        return -1;
    }

    auto v = desc[1];

    const DescriptorData data = std::get<DescriptorData>(v);

    if (data.size() != 4)
    {
        lg2::error("iana id should be 4 bytes");
        return -1;
    }

    const uint32_t iana = data[0] | (data[1] << 8) | (data[2] << 16) |
                          (data[3] << 24);

    lg2::debug("iana = {IANA}", "IANA", lg2::hex, iana);

    assert(iana == 0xdcbaff);

    return EXIT_SUCCESS;
}
