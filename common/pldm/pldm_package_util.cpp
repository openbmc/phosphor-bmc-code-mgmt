#include "pldm_package_util.hpp"

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

using namespace pldm::fw_update;

namespace pldm_package_util
{

std::shared_ptr<PackageParser> parsePLDMPackage(const uint8_t* buf, size_t size)
{
    std::vector<uint8_t> pkgData;

    pkgData.reserve(size);
    for (size_t i = 0; i < size; i++)
    {
        pkgData.push_back(buf[i]);
    }

    lg2::debug("parsing package header");

    std::unique_ptr<PackageParser> packageParser =
        pldm::fw_update::parsePackageHeader(pkgData);

    if (packageParser == nullptr)
    {
        lg2::error("could not parse package header");
        return packageParser;
    }

    lg2::debug("parsing package, pkg header size: {N}", "N",
               packageParser->pkgHeaderSize);

    std::vector<uint8_t> pkgHeaderOnly;
    pkgHeaderOnly.insert(
        pkgHeaderOnly.end(), pkgData.begin(),
        pkgData.begin() + (long)(packageParser->pkgHeaderSize));

    packageParser->parse(pkgHeaderOnly, pkgData.size());

    return packageParser;
}

int readImagePackage(FILE* file, uint8_t* package_data,
                     const size_t package_size)
{
    if (file == NULL || package_data == NULL)
    {
        return 1;
    }

    if (package_size == 0)
    {
        lg2::error("Package size is 0");
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

void* mmapImagePackage(sdbusplus::message::unix_fd image, size_t* size_out)
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
        lg2::error("could not mmap the image");
        return NULL;
    }

    return data;
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
    return fwDeviceIDRecordMatchesIANA(record, vendorIANA) &&
           fwDeviceIDRecordMatchesCompatible(record, compatible);
}

ssize_t findMatchingDeviceDescriptorIndex(
    const FirmwareDeviceIDRecords& records, uint32_t vendorIANA,
    const std::string& compatible)
{
    for (size_t i = 0; i < records.size(); i++)
    {
        const FirmwareDeviceIDRecord& record = records[i];
        if (fwDeviceIDRecordMatches(record, vendorIANA, compatible))
        {
            return (ssize_t)i;
        }
    }
    return -1;
}

int extractMatchingComponentImage(
    const std::shared_ptr<PackageParser>& packageParser,
    const std::string& compatible, uint32_t vendorIANA,
    uint32_t* component_offset_out, size_t* component_size_out,
    std::string& component_version_out)
{
    const FirmwareDeviceIDRecords& fwDeviceIdRecords =
        packageParser->getFwDeviceIDRecords();

    // find fw descriptor matching vendor iana and compatible
    ssize_t deviceDescriptorIndex = findMatchingDeviceDescriptorIndex(
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

    const ComponentImageInfos& cs = packageParser->getComponentImageInfos();

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
    component_version_out = std::get<7>(c);

    return EXIT_SUCCESS;
}

} // namespace pldm_package_util
