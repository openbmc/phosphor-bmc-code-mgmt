#pragma once

#include <libpldm++/firmware_update.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <cstdint>
#include <functional>
#include <memory>

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

// @param buf                    original package buffer
// @param package                Package instance
// @param compatible             'compatible' string of device
// @param vendorIANA             vendor iana of device
// @param componentOffsetOut     function returns offset of component image
// @param componentSizeOut       function returns size of component image
// @param componentVersionOut    function returns version of component image
// @returns                      0 on success
int extractMatchingComponentImage(
    const uint8_t* buf,
    const std::unique_ptr<pldm::fw_update::Package>& package,
    const std::string& compatible, uint32_t vendorIANA,
    uint32_t* componentOffsetOut, size_t* componentSizeOut,
    std::string& componentVersionOut);

} // namespace pldm_package_util
