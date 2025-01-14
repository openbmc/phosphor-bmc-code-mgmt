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
// @param package_data    the pre-allocated buffer for the package data
// @param package_size    how many bytes to read from the file
int readImagePackage(FILE* file, uint8_t* package_data, size_t package_size);

// @param image        file descriptor to the package
// @param size_out     function will write the size of the package here
// @returns            a pointer to the mmapped pldm package
void* mmapImagePackage(sdbusplus::message::unix_fd image, size_t* size_out);

// @param packageParser          PackageParser instance
// @param compatible             'compatible' string of device
// @param vendorIANA             vendor iana of device
// @param component_offset_out   function returns offset of component image
// @param component_size_out     function returns size of component image
// @param component_version_out  function returns version of component image
// @returns                      0 on success
int extractMatchingComponentImage(
    const std::shared_ptr<PackageParser>& packageParser,
    const std::string& compatible, uint32_t vendorIANA,
    uint32_t* component_offset_out, size_t* size_out,
    std::string& component_version_out);

} // namespace pldm_package_util
