
#include "common/pldm/package_parser.hpp"
#include "common/pldm/pldm_util.hpp"

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

std::string descriptorTypeStr(DescriptorType type)
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

int printPkgInfoFirmwareDeviceIDRecord(FirmwareDeviceIDRecord& did)
{
    lg2::info("");
    DeviceUpdateOptionFlags flag = std::get<0>(did);
    (void)flag;

    ApplicableComponents ac = std::get<1>(did);
    for (auto c : ac)
    {
        lg2::info("	Applicable Component: {COMPONENT}", "COMPONENT", c);
    }

    ComponentImageSetVersion cisv = std::get<2>(did);
    (void)cisv;

    Descriptors descriptors = std::get<3>(did);
    for (auto& [descriptorType, v] : descriptors)
    {
        std::string typeStr = descriptorTypeStr(descriptorType);
        lg2::info("	found descriptor type {TYPE} ({TYPESTR})", "TYPE", lg2::hex,
                  descriptorType, "TYPESTR", typeStr);

        if (std::holds_alternative<DescriptorData>(v))
        {
            const uint32_t data = vectorLE(std::get<DescriptorData>(v));
            lg2::info("	data: {DATA}", "DATA", lg2::hex, data);
        }
        else
        {
            VendorDefinedDescriptorInfo vd =
                std::get<VendorDefinedDescriptorInfo>(v);

            auto& title = std::get<0>(vd);
            auto& data = std::get<1>(vd);
            std::string dataStr(data.begin(), data.end());

            lg2::info("	title: {TITLE}, data: {DATA}", "TITLE", title, "DATA",
                      dataStr);
        }
    }

    FirmwareDevicePackageData fdpd = std::get<4>(did);
    (void)fdpd;

    return 0;
}

int printPkgInfoDescriptors(const std::shared_ptr<PackageParser>& pp)
{
    auto fwDeviceIdRecords = pp->getFwDeviceIDRecords();
    lg2::info("found {N} fw device id records", "N", fwDeviceIdRecords.size());

    for (FirmwareDeviceIDRecord& did : fwDeviceIdRecords)
    {
        printPkgInfoFirmwareDeviceIDRecord(did);
    }

    lg2::info("");

    return 0;
}

int printPkgInfoComponentImage(ComponentImageInfo& cii)
{
    lg2::info("");

    const uint16_t compClass = std::get<0>(cii);
    lg2::info("	component classification: {VALUE}", "VALUE", compClass);

    const uint16_t compIdentifier = std::get<1>(cii);
    lg2::info("	component identifier: {VALUE}", "VALUE", compIdentifier);

    CompLocationOffset clo = std::get<5>(cii);
    lg2::info("	component location offset: {OFFSET}", "OFFSET", lg2::hex, clo);

    const uint32_t compSize = std::get<6>(cii);
    lg2::info("	component size: {VALUE}", "VALUE", compSize);

    CompVersion cv = std::get<7>(cii);
    lg2::info("	component version: {VERSION}", "VERSION", cv);

    return 0;
}

int printPkgInfoComponentImages(const std::shared_ptr<PackageParser>& pp)
{
    auto compImages = pp->getComponentImageInfos();
    lg2::info("found {N} component images", "N", compImages.size());

    for (ComponentImageInfo& cii : compImages)
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
    size_t size = filesize(file);

    auto buf = std::make_shared<uint8_t[]>(size);

    status = pldmutil::readPLDMPackage(file, buf.get(), size);

    if (status != 0)
    {
        fclose(file);
        return 1;
    }

    fclose(file);

    std::shared_ptr<PackageParser> pp =
        pldmutil::parsePLDMFWUPPackageComplete(buf.get(), size);

    if (pp == nullptr)
    {
        return 1;
    }

    printPkgInfoDescriptors(pp);

    printPkgInfoComponentImages(pp);

    return 0;
}
