#pragma once

#include "config.h"

#include "version.hpp"

#include <experimental/filesystem>

namespace phosphor
{
namespace software
{
namespace updater
{

namespace fs = std::experimental::filesystem;
using VersionPurpose =
    sdbusplus::xyz::openbmc_project::Software::server::Version::VersionPurpose;

/** @brief Serialization function - stores priority information to file
 *  @param[in] versionId - The version for which to store information.
 *  @param[in] priority - RedundancyPriority value for that version.
 **/
void storePriorityToFile(std::string versionId, uint8_t priority);

/** @brief Serialization function - stores purpose information from file
 *  @param[in] versionId - The version for which to store information.
 *  @param[in] purpose - VersionPurpose value for that version.
 **/
void storePurposeToFile(std::string versionId, VersionPurpose purpose);

/** @brief Serialization function - restores priority information from file
 *  @param[in] versionId - The version for which to retrieve information.
 *  @param[in] priority - RedundancyPriority reference for that version.
 *  @return true if restore was successful, false if not
 **/
bool restorePriorityFromFile(std::string versionId, uint8_t& priority);

/** @brief Serialization function - restores purpose information from file
 *  @param[in] versionId - The version for which to retrieve information.
 *  @param[in] purpose - VersionPurpose reference for that version.
 *  @return true if restore was successful, false if not
 **/
bool restorePurposeFromFile(std::string versionId, VersionPurpose& purpose);

/** @brief Removes the serial file for a given version.
 *  @param[in] versionId - The version for which to remove a file, if it exists.
 **/
void removeFile(std::string versionId);

} // namespace updater
} // namespace software
} // namespace phosphor
