#pragma once

#include "mctp/endpoint.hpp"

#include <sdbusplus/async.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace phosphor::software::pldm
{

/** @brief Query the active firmware version of a PLDM firmware device.
 *  @param transport  MCTP transport to use
 *  @param eid        endpoint to query
 *  @returns          the active component image set version, nullopt on error
 */
sdbusplus::async::task<std::optional<std::string>> getActiveFirmwareVersion(
    mctp::Transport& transport, uint8_t eid);

} // namespace phosphor::software::pldm
