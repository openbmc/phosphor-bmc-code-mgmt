#include "pldm_util.hpp"

#include "package_parser.hpp"
#include "types.hpp"

#include <fcntl.h>
#include <libpldm/base.h>
#include <libpldm/firmware_update.h>
#include <libpldm/pldm_types.h>
#include <libpldm/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <cassert>
#include <cstring>

size_t filesize(FILE* file)
{
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    return size;
}

using namespace pldm::fw_update;

namespace pldmutil
{

std::shared_ptr<PackageParser>
    parsePLDMFWUPPackageComplete(const uint8_t* buf, size_t size)
{
    std::vector<uint8_t> pkgData;

    for (size_t i = 0; i < size; i++)
    {
        pkgData.push_back(buf[i]);
    }

    lg2::debug("parsing package header");

    std::unique_ptr<PackageParser> pp =
        pldm::fw_update::parsePkgHeader(pkgData);

    if (pp == nullptr)
    {
        lg2::error("could not parse package header");
        return pp;
    }

    lg2::debug("parsing package, pkg header size: {N}", "N", pp->pkgHeaderSize);

    std::vector<uint8_t> pkgHeaderOnly;
    pkgHeaderOnly.insert(pkgHeaderOnly.end(), pkgData.begin(),
                         pkgData.begin() + (long)(pp->pkgHeaderSize));

    pp->parse(pkgHeaderOnly, pkgData.size());

    return pp;
}

int readPLDMPackage(FILE* file, uint8_t* package_data,
                    const size_t package_size)
{
    if (file == NULL || package_data == NULL)
    {
        return 1;
    }

    if (package_size == 0)
    {
        lg2::error("PLDM Package size is 0");
        return 1;
    }

    lg2::debug("reading {NBYTES} bytes from file", "NBYTES", package_size);

    // Read the package into memory
    if (fread(package_data, 1, package_size, file) != package_size)
    {
        perror("Failed to read package data");
        fclose(file);
        return 1;
    }

    return 0;
}

void* mmap_pldm_package(sdbusplus::message::unix_fd image, size_t* size_out)
{
    lg2::debug("open fd {FD}", "FD", int(image));

    off_t size = lseek(image.fd, 0, SEEK_END);

    if (size < 0)
    {
        lg2::error("failed to determine file size");
        perror("error:");
        return NULL;
    }

    *size_out = size;

    lg2::debug("file size: {SIZE}", "SIZE", (uint64_t)size);

    void* data = mmap(nullptr, size, PROT_READ, MAP_SHARED, image.fd, 0);

    if (data == MAP_FAILED)
    {
        lg2::error("could not mmap the pldm update package");
        return NULL;
    }

    return data;
}

int verifyPLDMPackage(void* image, size_t size)
{
    int status = 0;

    const uint8_t* data2 = static_cast<uint8_t*>(image);

    std::shared_ptr<PackageParser> pp =
        parsePLDMFWUPPackageComplete(data2, size);

    if (pp == nullptr)
    {
        lg2::error("could not parse package header");
        return 1;
    }

    auto fwDeviceIdRecords = pp->getFwDeviceIDRecords();
    lg2::debug("found {N} fw device id records", "N", fwDeviceIdRecords.size());

    auto compImages = pp->getComponentImageInfos();

    if (compImages.size() != 1)
    {
        lg2::error("only 1 component image currently supported");
        return 1;
    }

    ComponentImageInfo& compImage = compImages[0];

    const uint16_t compClass = std::get<0>(compImage);
    lg2::debug("component classification: {VALUE}", "VALUE", compClass);

    const uint16_t compIdentifier = std::get<1>(compImage);
    lg2::debug("component identifier: {VALUE}", "VALUE", compIdentifier);

    const uint32_t compLocationOffset = std::get<5>(compImage);
    lg2::debug("component location offset: {VALUE}", "VALUE",
               compLocationOffset);

    const uint32_t compSize = std::get<6>(compImage);
    lg2::debug("component size: {VALUE}", "VALUE", compSize);

    std::string compVersion = std::get<7>(compImage);
    lg2::debug("component version: {VALUE}", "VALUE", compVersion);

    if (status != 0)
    {
        lg2::error("failed to extract pldm package data");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

bool fwDeviceIDRecordMatchesCompatible(const FirmwareDeviceIDRecord& record,
                                       const std::string& compatible)
{
    Descriptors desc = std::get<3>(record);
    if (desc.empty())
    {
        return false;
    }

    if (!desc.contains(0xffff))
    {
        return false;
    }

    auto v = desc[0xffff];

    if (!std::holds_alternative<VendorDefinedDescriptorInfo>(v))
    {
        lg2::debug(
            "descriptor does not have the vendor defined descriptor info");
        return false;
    }

    auto data = std::get<VendorDefinedDescriptorInfo>(v);

    std::string actualCompatible = std::get<VendorDefinedDescriptorTitle>(data);

    return compatible == actualCompatible;
}

bool fwDeviceIDRecordMatchesIANA(const FirmwareDeviceIDRecord& record,
                                 uint32_t vendorIANA)
{
    Descriptors desc = std::get<3>(record);

    if (desc.empty())
    {
        return false;
    }

    if (!desc.contains(0x1))
    {
        lg2::error("did not find iana enterprise id");
        return false;
    }

    auto viana = desc[0x1];

    if (!std::holds_alternative<DescriptorData>(viana))
    {
        lg2::error("did not find iana enterprise id");
        return false;
    }

    const DescriptorData& dd = std::get<DescriptorData>(viana);

    if (dd.size() != 4)
    {
        lg2::error("descriptor data wrong size ( != 4) for vendor iana");
        return false;
    }

    const uint32_t actualIANA = dd[0] | dd[1] << 8 | dd[2] << 16 | dd[3] << 24;

    return actualIANA == vendorIANA;
}

bool fwDeviceIDRecordMatches(const FirmwareDeviceIDRecord& record,
                             uint32_t vendorIANA, const std::string& compatible)
{
    return pldmutil::fwDeviceIDRecordMatchesIANA(record, vendorIANA) &&
           pldmutil::fwDeviceIDRecordMatchesCompatible(record, compatible);
}

ssize_t findMatchingDeviceDescriptorIndex(
    const FirmwareDeviceIDRecords& records, uint32_t vendorIANA,
    const std::string& compatible)
{
    for (size_t i = 0; i < records.size(); i++)
    {
        const FirmwareDeviceIDRecord& record = records[i];
        if (pldmutil::fwDeviceIDRecordMatches(record, vendorIANA, compatible))
        {
            return (ssize_t)i;
        }
    }
    return -1;
}

int extractMatchingComponentImage(
    const std::shared_ptr<PackageParser>& pp, const std::string& compatible,
    uint32_t vendorIANA, uint32_t* component_offset_out,
    size_t* component_size_out)
{
    const FirmwareDeviceIDRecords& fwDeviceIdRecords =
        pp->getFwDeviceIDRecords();

    // find fw descriptor matching vendor iana and compatible
    ssize_t deviceDescriptorIndex = pldmutil::findMatchingDeviceDescriptorIndex(
        fwDeviceIdRecords, vendorIANA, compatible);

    if (deviceDescriptorIndex < 0)
    {
        lg2::error(
            "did not find a matching device descriptor for {IANA}, {COMPATIBLE}",
            "IANA", lg2::hex, vendorIANA, "COMPATIBLE", compatible);
        return EXIT_FAILURE;
    }

    const FirmwareDeviceIDRecord& descriptor =
        fwDeviceIdRecords[deviceDescriptorIndex];
    // find applicable components
    // iterate over components to find the applicable component

    ApplicableComponents ac = std::get<1>(descriptor);

    if (ac.empty())
    {
        lg2::error("did not find an applicable component image for the device");
        return EXIT_FAILURE;
    }

    // component is 0 based index
    const size_t component = ac[0];

    const ComponentImageInfos& cs = pp->getComponentImageInfos();

    if (component >= cs.size())
    {
        lg2::error("applicable component out of bounds");
        return EXIT_FAILURE;
    }

    const ComponentImageInfo& c = cs[component];

    CompLocationOffset off = std::get<5>(c);
    CompSize size = std::get<6>(c);

    *component_offset_out = off;
    *component_size_out = size;

    return EXIT_SUCCESS;
}

} // namespace pldmutil
