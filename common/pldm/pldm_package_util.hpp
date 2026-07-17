#pragma once

#include "component_image.hpp"

#include <libpldm++/firmware_update.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace pldm_package_util
{

// @param buf           pointer to the pldm package
// @param size          size of 'buf'
// @returns             Package instance
std::unique_ptr<pldm::fw_update::Package> parsePLDMPackage(const uint8_t* buf,
                                                           size_t size);

// reads into a buffer, from file
// @param file            the file to read from
// @param packageData     the pre-allocated buffer for the package data
// @param packageSize     how many bytes to read from the file
int readImagePackage(FILE* file, uint8_t* packageData, size_t packageSize);

// @param image        file descriptor to the package
// @param sizeOut      function will write the size of the package here
// @returns            a unique pointer to the mmapped pldm package
std::unique_ptr<void, std::function<void(void*)>> mmapImagePackage(
    sdbusplus::message::unix_fd image, size_t* sizeOut);

// Extract ALL component images applicable to the matching device record,
// in package order. Each returned image points directly into the buffer
// the package was parsed over (no copy is made).
// @param package                Package instance
// @param compatible             'compatible' string of device
// @param vendorIANA             vendor iana of device
// @param componentsOut          receives the applicable component images
// @returns                      true on success
bool extractMatchingComponentImages(
    const std::unique_ptr<pldm::fw_update::Package>& package,
    const std::string& compatible, uint32_t vendorIANA,
    std::vector<phosphor::software::device::ComponentImage>& componentsOut);

} // namespace pldm_package_util
