#include <memory>
#include <string>
#include <functional>

#include <sdbusplus/message/native_types.hpp>

/**
 * @namespace pldm::fw_update
 * @brief Mock namespace for PLDM firmware package interface
 */
namespace pldm::fw_update
{
/**
 * @class PackageParser
 * @brief Mock class for PLDM package parsing
 */
class PackageParser
{
public:
    virtual ~PackageParser() = default;
    PackageParser() = default;
    PackageParser(const PackageParser&) = delete;
    PackageParser& operator=(const PackageParser&) = delete;
    PackageParser(PackageParser&&) = delete;
    PackageParser& operator=(PackageParser&&) = delete;
};
}

/**
 * @namespace pldm_package_util
 * @brief Mock implementation of PLDM package utility functions
 */
namespace pldm_package_util
{
/**
 * @brief Mock function that parses a PLDM package
 * @param buf Buffer containing PLDM package
 * @param size Size of the buffer
 * @return nullptr in this mock implementation
 */
std::shared_ptr<pldm::fw_update::PackageParser> 
parsePLDMPackage(const uint8_t* /*buf*/, size_t /*size*/)
{
    return nullptr;
}

/**
 * @brief Mock function that extracts a component image from a PLDM package
 * @return false in this mock implementation
 */
bool extractMatchingComponentImage(
    const std::shared_ptr<pldm::fw_update::PackageParser>& /*packageParser*/,
    const std::string& /*compatible*/,
    unsigned int /*componentId*/,
    unsigned int* /*componentRevision*/,
    size_t* /*imgSize*/,
    std::string& /*destination*/)
{
    return false;
}

/**
 * @brief Mock function that memory maps a PLDM image
 * @param image File descriptor for the image
 * @param size Pointer to store the image size (set to 0)
 * @return Empty unique_ptr in this mock implementation
 */
std::unique_ptr<void, std::function<void(void*)>> 
mmapImagePackage(sdbusplus::message::unix_fd /*image*/, size_t* size)
{
    if (size) {
        *size = 0;
    }
    return std::unique_ptr<void, std::function<void(void*)>>(
        nullptr, [](void*) {});
}

} // namespace pldm_package_util

namespace sdbusplus {
namespace message {
namespace native_types {} // Empty namespace to satisfy includes
} // namespace message
} // namespace sdbusplus