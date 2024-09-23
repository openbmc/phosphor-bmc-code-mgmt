#pragma once

#include "fw-update/common/pldm/package_parser.hpp"
#include "fw-update/common/pldm/types.hpp"
#include "sdbusplus/message/native_types.hpp"

#include <cstdint>
#include <memory>

using namespace pldm::fw_update;

size_t filesize(FILE* file);

std::shared_ptr<PackageParser> parsePLDMFWUPPackageComplete(
    const std::shared_ptr<uint8_t[]>& buf, size_t size);

// reads a PLDM package from file
int readPLDMPackage(FILE* file, uint8_t* package_data, size_t package_size);

int verifyPLDMPackage(sdbusplus::message::unix_fd image);

// this takes the 'compatible' string from the json configuration
// and determines if the package is compatible with the device
bool isPLDMPackageCompatible(const std::shared_ptr<PackageParser>& pp,
                             const std::string& compatible,
                             uint32_t vendorIANA);

// IANA Enterprise ID is unsigned, 4 bytes in PLDM spec
// returns < 0 on failure
int64_t getIANAFromPLDMPackage(const std::shared_ptr<PackageParser>& pp);

// extract 'compatible' string from a pldm fw update package
// This will be 0xffff (Vendor Defined) type in Table 7
std::string
    getCompatibleFromPLDMPackage(const std::shared_ptr<PackageParser>& pp);
