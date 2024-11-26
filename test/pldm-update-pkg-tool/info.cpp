
#include "common/pldm/pldm_package_util.hpp"

#include <libpldm++/firmware_update.hpp>
#include <phosphor-logging/lg2.hpp>

#include <iostream>
#include <string>

void printHelpPkgInfo()
{
    std::cout
        << "--info <filename>                info about the pldm fw update package"
        << std::endl;
}

uint32_t vectorLE(std::vector<uint8_t> v)
{
    uint32_t res = 0;

    for (size_t i = 0; i < v.size(); i++)
    {
        res |= (v[i] << (8 * i));
    }

    return res;
}

std::string descriptorTypeStr(uint16_t type)
{
    if (type == 0x0000)
    {
        return "PCI Vendor ID";
    }
    if (type == 0x0001)
    {
        return "IANA Enterprise ID";
    }
    if (type == 0x0002)
    {
        return "UUID";
    }
    if (type == 0xffff)
    {
        return "Vendor Defined";
    }

    return "unrecognized (implementation incomplete)";
}

int printPkgInfoFirmwareDeviceIDRecord(
    pldm::fw_update::FirmwareDeviceIDRecord& did)
{
    for (auto c : did.applicableComponents)
    {
        lg2::info("	Applicable Component: {COMPONENT}", "COMPONENT", c);
    }

    lg2::info("	    Component Image Set Version {VERSION}", "VERSION",
              did.componentImageSetVersionString);

    for (auto& [descriptorType, v] : did.recordDescriptors)
    {
        std::string typeStr = descriptorTypeStr(descriptorType);
        lg2::info("	found descriptor type {TYPE} ({TYPESTR})", "TYPE", lg2::hex,
                  descriptorType, "TYPESTR", typeStr);

        auto vd = v->data;

        const std::string title = v->vendorDefinedDescriptorTitle.value_or("-");
        auto& data = v->data;
        std::string dataStr(data.begin(), data.end());

        lg2::info("	title: {TITLE}, data: {DATA}", "TITLE", title, "DATA",
                  dataStr);
    }

    auto fdpd = did.firmwareDevicePackageData;
    lg2::info("	Firmware device package data size: {SIZE}", "SIZE",
              fdpd.size());

    return 0;
}

int printPkgInfoDescriptors(const std::unique_ptr<pldm::fw_update::Package>& pp)
{
    auto fwDeviceIdRecords = pp->firmwareDeviceIdRecords;
    lg2::info("found {N} fw device id records", "N", fwDeviceIdRecords.size());

    for (pldm::fw_update::FirmwareDeviceIDRecord& did : fwDeviceIdRecords)
    {
        printPkgInfoFirmwareDeviceIDRecord(did);
    }

    lg2::info("");

    return 0;
}

int printPkgInfoComponentImage(pldm::fw_update::ComponentImageInfo& cii)
{
    lg2::info("");

    lg2::info("	component classification: {VALUE}", "VALUE",
              cii.componentClassification);

    lg2::info("	component identifier: {VALUE}", "VALUE",
              cii.componentIdentifier);

    const uint8_t* clo = cii.componentLocation.ptr;
    lg2::info("	component location offset: {OFFSET}", "OFFSET", lg2::hex, clo);

    const uint32_t compSize = cii.componentLocation.length;
    lg2::info("	component size: {VALUE}", "VALUE", compSize);

    lg2::info("	component version: {VERSION}", "VERSION", cii.componentVersion);

    return 0;
}

int printPkgInfoComponentImages(
    const std::unique_ptr<pldm::fw_update::Package>& pp)
{
    auto compImages = pp->componentImageInformation;
    lg2::info("found {N} component images", "N", compImages.size());

    for (pldm::fw_update::ComponentImageInfo& cii : compImages)
    {
        printPkgInfoComponentImage(cii);
    }

    lg2::info("");

    return 0;
}

int printPkgInfo(int argc, char* argv[])
{
    int status;

    if (argc == 0)
    {
        lg2::info("no filename provided, exiting.");
        return 1;
    }

    std::string filename = argv[0];

    FILE* file = fopen(filename.c_str(), "r");
    size_t size = std::filesystem::file_size(filename);

    auto buf = std::make_shared<uint8_t[]>(size);

    status = pldm_package_util::readImagePackage(file, buf.get(), size);

    if (status != 0)
    {
        fclose(file);
        return 1;
    }

    fclose(file);

    auto pp = pldm::fw_update::PackageParser::parse(
        {buf.get(), size}, pldm::fw_update::PackagePin::v1);

    if (!pp.has_value())
    {
        return 1;
    }

    printPkgInfoDescriptors(pp.value());

    printPkgInfoComponentImages(pp.value());

    return 0;
}
