#pragma once

#include "config.h"

#include "version.hpp"

#include <string>

namespace phosphor
{
namespace software
{
namespace updater
{

using VersionPurpose =
    sdbusplus::server::xyz::openbmc_project::software::Version::VersionPurpose;

/** @brief Serialization function - stores priority information to file
 *  @param[in] flashId - The flash id of the version for which to store
 *                       information.
 *  @param[in] priority - RedundancyPriority value for that version.
 **/
void storePriority(const std::string& flashId, uint8_t priority);

/** @brief Serialization function - stores purpose information to file
 *  @param[in] flashId - The flash id of the version for which to store
 *                       information.
 *  @param[in] purpose - VersionPurpose value for that version.
 **/
void storePurpose(const std::string& flashId, VersionPurpose purpose);

/** @brief Serialization function - restores priority information from file
 *  @param[in] flashId - The flash id of the version for which to retrieve
 *                       information.
 *  @param[in] priority - RedundancyPriority reference for that version.
 *  @return true if restore was successful, false if not
 **/
bool restorePriority(const std::string& flashId, uint8_t& priority);

/** @brief Serialization function - restores purpose information from file
 *  @param[in] flashId - The flash id of the version for which to retrieve
 *                       information.
 *  @param[in] purpose - VersionPurpose reference for that version.
 *  @return true if restore was successful, false if not
 **/
bool restorePurpose(const std::string& flashId, VersionPurpose& purpose);

/** @brief Removes the serial directory for a given version.
 *  @param[in] flash Id - The flash id of the version for which to remove a
 *                        file, if it exists.
 **/
void removePersistDataDirectory(const std::string& flashId);

} // namespace updater
} // namespace software
} // namespace phosphor
