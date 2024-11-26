
#include "common/pldm/pldm_util.hpp"
#include "tool/create_package/create_pldm_fw_package.hpp"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>

std::string
    getCompatibleFromPLDMPackage(const std::shared_ptr<PackageParser>& pp)
{
    auto fwDeviceIdRecords = pp->getFwDeviceIDRecords();
    if (fwDeviceIdRecords.size() != 1)
    {
        lg2::error(
            "fw device id records != 1, rejecting package, incompatible.");
        return "";
    }

    lg2::debug("package has {N} fw device id records", "N",
               fwDeviceIdRecords.size());

    assert(!fwDeviceIdRecords.empty());

    for (FirmwareDeviceIDRecord& record : fwDeviceIdRecords)
    {
        Descriptors desc = std::get<3>(record);
        if (desc.empty())
        {
            continue;
        }

        lg2::debug("fw device id record has {N} descriptors", "N", desc.size());

        if (!desc.contains(0xffff))
        {
            continue;
        }

        auto v = desc[0xffff];

        if (!std::holds_alternative<VendorDefinedDescriptorInfo>(v))
        {
            lg2::debug(
                "descriptor does not have the vendor defined descriptor info");
            continue;
        }

        auto data = std::get<VendorDefinedDescriptorInfo>(v);

        std::string res = std::get<VendorDefinedDescriptorTitle>(data);

        return res;
    }

    return "";
}

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

    std::string compatible = getCompatibleFromPLDMPackage(pp);

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
