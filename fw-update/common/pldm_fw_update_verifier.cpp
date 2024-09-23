#include "fw-update/common/pldm_fw_update_verifier.hpp"

#include "fw-update/common/pldm/package_parser.hpp"
#include "fw-update/common/pldm/types.hpp"

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

std::shared_ptr<PackageParser> parsePLDMFWUPPackageComplete(
    const std::shared_ptr<uint8_t[]>& buf, size_t size)
{
    std::vector<uint8_t> pkgData;

    for (size_t i = 0; i < size; i++)
    {
        pkgData.push_back(buf.get()[i]);
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

int verifyPLDMPackage(sdbusplus::message::unix_fd image)
{
    int status = 0;

    lg2::debug("open fd {FD}", "FD", image.fd);

    off_t size = lseek(image.fd, 0, SEEK_END);

    if (size < 0)
    {
        lg2::error("failed to determine file size");
        return 1;
    }

    lg2::debug("file size: {SIZE}", "SIZE", (uint64_t)size);

    void* data = mmap(nullptr, size, PROT_READ, MAP_SHARED, image.fd, 0);

    if (data == MAP_FAILED)
    {
        lg2::error("could not mmap the pldm update package");
        return 1;
    }

    auto buf = std::make_shared<uint8_t[]>(size);

    const uint8_t* data2 = static_cast<uint8_t*>(data);

    memcpy(buf.get(), data2, size);

    std::shared_ptr<PackageParser> pp = parsePLDMFWUPPackageComplete(buf, size);

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

    // TODO: do the verification for the firmware device identification area

    return EXIT_SUCCESS;
}

bool isPLDMPackageCompatible(const std::shared_ptr<PackageParser>& pp,
                             const std::string& compatible,
                             const uint32_t vendorIANA)
{
    uint32_t pkgIANA = getIANAFromPLDMPackage(pp);
    std::string pkgCompatible = getCompatibleFromPLDMPackage(pp);

    lg2::debug("found vendor iana in package: {IANA}", "IANA", pkgIANA);
    lg2::debug("found 'compatible' string in package: {COMPATIBLE}",
               "COMPATIBLE", pkgCompatible);

    return (pkgIANA == vendorIANA) && (pkgCompatible == compatible);
}

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

int64_t getIANAFromPLDMPackage(const std::shared_ptr<PackageParser>& pp)
{
    auto fwDeviceIdRecords = pp->getFwDeviceIDRecords();
    if (fwDeviceIdRecords.size() != 1)
    {
        lg2::error(
            "fw device id records != 1, rejecting package, incompatible.");
        return -1;
    }
    FirmwareDeviceIDRecord& record = fwDeviceIdRecords[0];

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

    DescriptorData data = std::get<DescriptorData>(v);

    if (data.size() != 4)
    {
        lg2::error("iana id should be 4 bytes");
        return -1;
    }

    const uint32_t iana = data[0] | (data[1] << 8) | (data[2] << 16) |
                          (data[3] << 24);
    return iana;
}
