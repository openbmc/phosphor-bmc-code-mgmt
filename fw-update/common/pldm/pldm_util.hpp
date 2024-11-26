#pragma once

#include "package_parser.hpp"
#include "sdbusplus/message/native_types.hpp"

#include <cstdint>
#include <memory>

using namespace pldm::fw_update;

size_t filesize(FILE* file);

namespace pldmutil
{

// @param buf           pointer to the pldm package
// @param size          size of 'buf'
// @returns             PackageParser instance
std::shared_ptr<PackageParser>
    parsePLDMFWUPPackageComplete(const uint8_t* buf, size_t size);

// reads a PLDM package from file
// @param file            the file to read from
// @param package_data    the pre-allocated buffer for the pldm package data
// @param package_size    how many bytes to read from the file
int readPLDMPackage(FILE* file, uint8_t* package_data, size_t package_size);

// @param image        file descriptor to the pldm package
// @param size_out     function will write the size of the pldm package here
// @returns            a pointer to the mmapped pldm package
void* mmap_pldm_package(sdbusplus::message::unix_fd image, size_t* size_out);

// @param image        the raw pldm package
// @param image_size   the size of 'image'
// @returns            0 on success
int verifyPLDMPackage(void* image, size_t image_size);

// @param pp                     PackageParser instance
// @param compatible             'compatible' string of device
// @param vendorIANA             vendor iana of device
// @param component_offset_out   function returns offset of component image
// @param component_size_out     function returns size of component image
// @returns                      0 on success
int extractMatchingComponentImage(
    const std::shared_ptr<PackageParser>& pp, const std::string& compatible,
    uint32_t vendorIANA, uint32_t* component_offset_out, size_t* size_out);

} // namespace pldmutil
