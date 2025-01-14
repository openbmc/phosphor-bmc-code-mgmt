#include "common/pldm/package_parser.hpp"
#include "common/pldm/pldm_package_util.hpp"
#include "common/pldm/types.hpp"
#include "test/create_package/create_pldm_fw_package.hpp"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>

#include "gtest/gtest.h"

std::string getCompatibleFromPLDMPackage(
    const std::shared_ptr<PackageParser>& packageParser)
{
    auto fwDeviceIdRecords = packageParser->getFwDeviceIDRecords();
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

std::shared_ptr<PackageParser> createPackageCommon(
    uint8_t* component_image, size_t component_image_size,
    const std::optional<uint32_t>& optVendorIANA,
    const std::optional<std::string>& optCompatible)
{
    size_t size_out = 0;
    std::unique_ptr<uint8_t[]> buf =
        create_pldm_package_buffer(component_image, component_image_size,
                                   optVendorIANA, optCompatible, size_out);

    const std::shared_ptr<PackageParser> packageParser =
        pldm_package_util::parsePLDMPackage(buf.get(), size_out);

    return packageParser;
};

TEST(PLDM_PACKAGE_PARSER, getCompatible)
{
    uint8_t component_image[] = {0x12, 0x34, 0x83, 0x21};

    const std::shared_ptr<PackageParser> packageParser = createPackageCommon(
        component_image, sizeof(component_image), std::nullopt,
        std::optional<std::string>("com.my.compatible"));

    if (packageParser == nullptr)
    {
        lg2::error("could not parse PLDM Package");
        assert(false);
    }

    std::string compatible = getCompatibleFromPLDMPackage(packageParser);

    if (compatible.empty())
    {
        lg2::error(
            "could not extract compatible string from PLDM FW Update Package");
        assert(false);
    }

    lg2::debug("compatible = {COMPATIBLE}", "COMPATIBLE", compatible);

    assert(compatible == "com.my.compatible");
}

TEST(PLDM_PACKAGE_PARSER, getComponentImage)
{
    int status = 0;
    uint8_t component_image[] = {0xab, 0xba, 0xcd, 0xef};
    const size_t component_image_size = 4;

    std::optional<std::string> optFilename =
        create_pldm_package(component_image, component_image_size);

    assert(optFilename.has_value());

    FILE* file = fopen(optFilename.value().c_str(), "r");

    assert(file != NULL);

    const size_t size = std::filesystem::file_size(optFilename.value());

    auto buf = std::make_shared<uint8_t[]>(size);

    status = pldm_package_util::readImagePackage(file, buf.get(), size);

    fclose(file);

    assert(status == 0);

    std::shared_ptr<PackageParser> packageParser =
        pldm_package_util::parsePLDMPackage(buf.get(), size);

    assert(packageParser != nullptr);

    auto compImages = packageParser->getComponentImageInfos();

    lg2::debug("found {N} component images", "N", compImages.size());

    assert(compImages.size() == 1);

    const ComponentImageInfo& compImage = compImages[0];

    const uint16_t compClass = std::get<0>(compImage);
    lg2::debug("component classification: {VALUE}", "VALUE", compClass);

    const uint16_t compIdentifier = std::get<1>(compImage);
    lg2::debug("component identifier: {VALUE}", "VALUE", compIdentifier);

    const uint32_t compLocationOffset = std::get<5>(compImage);
    lg2::debug("component location offset: {VALUE}", "VALUE",
               compLocationOffset);

    const uint32_t compSize = std::get<6>(compImage);
    lg2::debug("component size: {VALUE}", "VALUE", compSize);

    assert(compSize == component_image_size);

    std::string compVersion = std::get<7>(compImage);
    lg2::debug("component version: {VALUE}", "VALUE", compVersion);

    uint8_t* p = buf.get() + compLocationOffset;
    lg2::debug("first few bytes of component image: {1} {2} {3} {4}", "1",
               lg2::hex, p[0], "2", lg2::hex, p[1], "3", lg2::hex, p[2], "4",
               lg2::hex, p[3]);

    assert(p[0] == 0xab);
    assert(p[1] == 0xba);
    assert(p[2] == 0xcd);
    assert(p[3] == 0xef);

    assert(status == 0);
}

TEST(PLDM_PACKAGE_PARSER, getFirmwareDeviceId)
{
    int status = 0;
    uint8_t component_image[] = {0xab, 0xba, 0xcd, 0xef};

    const std::shared_ptr<PackageParser> packageParser = createPackageCommon(
        component_image, sizeof(component_image), std::nullopt, std::nullopt);

    assert(packageParser != nullptr);

    auto fwDeviceIdRecords = packageParser->getFwDeviceIDRecords();
    lg2::debug("found {N} fw device id records", "N", fwDeviceIdRecords.size());

    const FirmwareDeviceIDRecords records =
        packageParser->getFwDeviceIDRecords();

    assert(records.size() == 1);

    const FirmwareDeviceIDRecord& record = records[0];

    const DeviceUpdateOptionFlags optFlags = std::get<0>(record);

    // assert we do not continue component updates after failure
    assert(!optFlags[0]);

    const ApplicableComponents ac = std::get<1>(record);

    assert(ac.size() == 1);

    // assert that the one component image is applicable to this device
    assert(ac[0] == 0);

    const ComponentImageSetVersion cisv = std::get<2>(record);

    assert(!cisv.empty());

    Descriptors desc = std::get<3>(record);

    assert(desc.size() == 1);

    assert(desc.contains(1)); // iana type descriptor

    auto v = desc[1];

    const DescriptorData data = std::get<DescriptorData>(v);

    // iana id is 4 bytes
    assert(data.size() == 4);

    assert(status == 0);
}

TEST(PLDM_PACKAGE_PARSER, getIANA)
{
    uint8_t component_image[] = {0x12, 0x34, 0xab};

    const std::shared_ptr<PackageParser> packageParser =
        createPackageCommon(component_image, sizeof(component_image),
                            std::optional<uint32_t>(0xdcbaff), std::nullopt);

    assert(packageParser != nullptr);

    auto fwDeviceIdRecords = packageParser->getFwDeviceIDRecords();
    if (fwDeviceIdRecords.size() != 1)
    {
        lg2::error(
            "fw device id records != 1, rejecting package, incompatible.");
        assert(false);
    }

    const FirmwareDeviceIDRecord& record = fwDeviceIdRecords[0];

    Descriptors desc = std::get<3>(record);
    if (desc.empty())
    {
        lg2::error("there is no descriptor for the applicable device");
        assert(false);
    }

    if (!desc.contains(1))
    {
        // iana type descriptor
        lg2::error("the package does not have an IANA type descriptor");
        assert(false);
    }

    auto v = desc[1];

    const DescriptorData data = std::get<DescriptorData>(v);

    if (data.size() != 4)
    {
        lg2::error("iana id should be 4 bytes");
        assert(false);
    }

    const uint32_t iana = data[0] | (data[1] << 8) | (data[2] << 16) |
                          (data[3] << 24);

    lg2::debug("iana = {IANA}", "IANA", lg2::hex, iana);

    assert(iana == 0xdcbaff);
}
