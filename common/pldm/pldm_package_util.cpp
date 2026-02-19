#include "pldm_package_util.hpp"

#include <fcntl.h>
#include <libpldm/base.h>
#include <libpldm/firmware_update.h>
#include <libpldm/pldm_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <libpldm++/firmware_update.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <cassert>
#include <cstring>
#include <functional>

PHOSPHOR_LOG2_USING;

using namespace pldm::fw_update;

namespace pldm_package_util
{

std::shared_ptr<pldm::fw_update::Package> parsePLDMPackage(const uint8_t* buf,
                                                           size_t size)
{
    const std::span<const uint8_t> pkgSpan = {buf, size};

    DEFINE_PLDM_PACKAGE_FORMAT_PIN_FR01H(pin);

    std::expected<std::unique_ptr<Package>, PackageParserError> res =
        pldm::fw_update::PackageParser::parse(pkgSpan, pin);

    if (!res.has_value())
    {
        error("Failed to parse PLDM package: {ERR}", "ERR", res.error().msg);
        return nullptr;
    }

    // we take ownership of the package pointer
    Package* pkg = res.value().release();

    return std::shared_ptr<Package>(pkg);
}

int readImagePackage(FILE* file, uint8_t* packageData, const size_t packageSize)
{
    if (file == NULL || packageData == NULL)
    {
        return 1;
    }

    if (packageSize == 0)
    {
        error("Package size is 0");
        return 1;
    }

    debug("reading {NBYTES} bytes from file", "NBYTES", packageSize);

    // Read the package into memory
    if (fread(packageData, 1, packageSize, file) != packageSize)
    {
        perror("Failed to read package data");
        fclose(file);
        return 1;
    }

    return 0;
}

std::unique_ptr<void, std::function<void(void*)>> mmapImagePackage(
    sdbusplus::message::unix_fd image, size_t* sizeOut)
{
    debug("open fd {FD}", "FD", int(image));

    off_t size = lseek(image.fd, 0, SEEK_END);

    if (size < 0)
    {
        error("failed to determine file size");
        perror("error:");
        return nullptr;
    }

    *sizeOut = size;

    debug("file size: {SIZE}", "SIZE", (uint64_t)size);

    void* data = mmap(nullptr, size, PROT_READ, MAP_SHARED, image.fd, 0);

    if (data == MAP_FAILED)
    {
        error("could not mmap the image");
        return nullptr;
    }

    using mmapUniquePtr = std::unique_ptr<void, std::function<void(void*)>>;

    mmapUniquePtr dataUnique(data, [size](void* arg) {
        if (munmap(arg, size) != 0)
        {
            error("Failed to un map the PLDM package");
        }
    });

    return dataUnique;
}

bool fwDeviceIDRecordMatchesCompatible(const FirmwareDeviceIDRecord& record,
                                       const std::string& compatible)
{
    const auto& desc = record.recordDescriptors;
    if (desc.empty())
    {
        return false;
    }

    if (!desc.contains(PLDM_FWUP_VENDOR_DEFINED))
    {
        return false;
    }

    auto& v = desc.at(PLDM_FWUP_VENDOR_DEFINED);

    if (!v->vendorDefinedDescriptorTitle.has_value())
    {
        debug("descriptor does not have the vendor defined descriptor info");
        return false;
    }

    std::string actualCompatible = v->vendorDefinedDescriptorTitle.value();

    return compatible == actualCompatible;
}

bool fwDeviceIDRecordMatchesIANA(const FirmwareDeviceIDRecord& record,
                                 uint32_t vendorIANA)
{
    const auto& desc = record.recordDescriptors;

    if (desc.empty())
    {
        return false;
    }

    if (!desc.contains(0x1))
    {
        error("did not find iana enterprise id");
        return false;
    }

    auto& viana = desc.at(PLDM_FWUP_IANA_ENTERPRISE_ID);

    const DescriptorData& dd = *viana;

    if (dd.data.size() != 4)
    {
        error("descriptor data wrong size ( != 4) for vendor iana");
        return false;
    }

    const uint32_t actualIANA =
        dd.data[0] | dd.data[1] << 8 | dd.data[2] << 16 | dd.data[3] << 24;

    return actualIANA == vendorIANA;
}

bool fwDeviceIDRecordMatches(const FirmwareDeviceIDRecord& record,
                             uint32_t vendorIANA, const std::string& compatible)
{
    return fwDeviceIDRecordMatchesIANA(record, vendorIANA) &&
           fwDeviceIDRecordMatchesCompatible(record, compatible);
}

ssize_t findMatchingDeviceDescriptorIndex(
    const std::vector<FirmwareDeviceIDRecord>& records, uint32_t vendorIANA,
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
    const uint8_t* buf, const std::shared_ptr<Package>& package,
    const std::string& compatible, uint32_t vendorIANA,
    uint32_t* componentOffsetOut, size_t* componentSizeOut,
    std::string& componentVersionOut)
{
    const std::vector<FirmwareDeviceIDRecord>& fwDeviceIdRecords =
        package->firmwareDeviceIdRecords;

    // find fw descriptor matching vendor iana and compatible
    ssize_t deviceDescriptorIndex = findMatchingDeviceDescriptorIndex(
        fwDeviceIdRecords, vendorIANA, compatible);

    if (deviceDescriptorIndex < 0)
    {
        error(
            "did not find a matching device descriptor for {IANA}, {COMPATIBLE}",
            "IANA", lg2::hex, vendorIANA, "COMPATIBLE", compatible);
        return EXIT_FAILURE;
    }

    const FirmwareDeviceIDRecord& descriptor =
        fwDeviceIdRecords[deviceDescriptorIndex];
    // find applicable components
    // iterate over components to find the applicable component

    const std::vector<size_t>& ac = descriptor.applicableComponents;

    if (ac.empty())
    {
        error("did not find an applicable component image for the device");
        return EXIT_FAILURE;
    }

    // component is 0 based index
    const size_t component = ac[0];

    const std::vector<ComponentImageInfo>& cs =
        package->componentImageInformation;

    if (component >= cs.size())
    {
        error("applicable component out of bounds");
        return EXIT_FAILURE;
    }

    const ComponentImageInfo& c = cs[component];

    // calculate component offset with pointer arithmetic
    *componentOffsetOut = c.componentLocation.ptr - buf;
    *componentSizeOut = c.componentLocation.length;
    componentVersionOut = c.componentVersion;

    return EXIT_SUCCESS;
}

} // namespace pldm_package_util
