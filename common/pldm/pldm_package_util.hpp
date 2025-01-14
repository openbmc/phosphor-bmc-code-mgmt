#pragma once

#include "package_parser.hpp"
#include "sdbusplus/message/native_types.hpp"

#include <cstdint>
#include <memory>

using namespace pldm::fw_update;

namespace pldm_package_util
{

// @param buf           pointer to the pldm package
// @param size          size of 'buf'
// @returns             PackageParser instance
std::shared_ptr<PackageParser> parsePLDMPackage(const uint8_t* buf,
                                                size_t size);

// reads into a buffer, from file
// @param file            the file to read from
// @param packageData     the pre-allocated buffer for the package data
// @param packageSize     how many bytes to read from the file
int readImagePackage(FILE* file, uint8_t* packageData, size_t packageSize);

// @param image        file descriptor to the package
// @param sizeOut      function will write the size of the package here
// @returns            a pointer to the mmapped pldm package
void* mmapImagePackage(sdbusplus::message::unix_fd image, size_t* sizeOut);

// @param packageParser          PackageParser instance
// @param compatible             'compatible' string of device
// @param vendorIANA             vendor iana of device
// @param componentOffsetOut     function returns offset of component image
// @param componentSizeOut       function returns size of component image
// @param componentVersionOut    function returns version of component image
// @returns                      0 on success
int extractMatchingComponentImage(
    const std::shared_ptr<PackageParser>& packageParser,
    const std::string& compatible, uint32_t vendorIANA,
    uint32_t* componentOffsetOut, size_t* componentSizeOut,
    std::string& componentVersionOut);

} // namespace pldm_package_util
