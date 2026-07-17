#pragma once

#include <libpldm++/firmware_update.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace pldm_package_util
{

// One component image extracted from a package, in package order
struct MatchingComponentImage
{
    uint32_t offset;
    size_t size;
    std::string version;
};

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

// Extract ALL component images applicable to the matching device record,
// in package order. Devices whose recovery/update flow consumes multiple
// component images per package (e.g. OCP recovery) use this instead of
// extractMatchingComponentImage.
// @param buf                    original package buffer
// @param package                Package instance
// @param compatible             'compatible' string of device
// @param vendorIANA             vendor iana of device
// @param componentsOut          function returns the applicable components
// @returns                      0 on success
int extractMatchingComponentImages(
    const uint8_t* buf,
    const std::unique_ptr<pldm::fw_update::Package>& package,
    const std::string& compatible, uint32_t vendorIANA,
    std::vector<MatchingComponentImage>& componentsOut);

} // namespace pldm_package_util
