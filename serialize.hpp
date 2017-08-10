#pragma once

#include <experimental/filesystem>
#include "config.h"

namespace phosphor
{
namespace software
{
namespace updater
{

namespace fs = std::experimental::filesystem;

/** @brief Serialization function - stores activation information to file
 *  @param[in] versionId - The version for which to store information.
 *  @param[in] priority - RedundancyPriority value for that version.
 **/
void storeToFile(std::string versionId, uint8_t priority);

/** @brief Serialization function - restores activation information from file
 *  @param[in] versionId - The version for which to retrieve information.
 *  @param[in] priority - RedundancyPriority reference for that version.
 *  @return true if restore was successful, false if not
 **/
bool restoreFromFile(std::string versionId, uint8_t& priority);

/** @brief Removes the serial file for a given version.
 *  @param[in] versionId - The version for which to remove a file, if it exists.
 **/
void removeFile(std::string versionId);

} // namespace phosphor
} // namespace software
} // namespace openpower
