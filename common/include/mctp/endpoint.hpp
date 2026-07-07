#pragma once

#include <sdbusplus/async.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace phosphor::software::mctp
{
// MCTP message types, DSP0239
constexpr uint8_t msgTypePldm = 0x01;

// Default time to wait for a response.
constexpr auto defaultTimeout = std::chrono::seconds(3);

/** @brief Look up the EID of an MCTP endpoint by its UUID.
 *  @param ctx    async context
 *  @param uuid   endpoint UUID to match
 *  @returns      the EID, or nullopt if no such endpoint is on D-Bus yet
 */
sdbusplus::async::task<std::optional<uint8_t>> findEndpoint(
    sdbusplus::async::context& ctx, const std::string& uuid);

/** @brief Resolve an endpoint by UUID, waiting for it if necessary.
 *
 *  Subscribes to InterfacesAdded before enumerating, so an endpoint which
 *  is already present and one which appears later are both handled.
 */
sdbusplus::async::task<uint8_t> waitForEndpoint(sdbusplus::async::context& ctx,
                                                const std::string& uuid);

/** @brief Send an MCTP message and await the response.
 *  @param msgType   MCTP message type, DSP0239
 *  @param timeout   how long to wait for the response
 *  @returns         the response payload, nullopt on error or timeout
 */
sdbusplus::async::task<std::optional<std::vector<uint8_t>>> sendReceive(
    sdbusplus::async::context& ctx, uint8_t eid, uint8_t msgType,
    const std::vector<uint8_t>& request,
    std::chrono::milliseconds timeout = defaultTimeout);

} // namespace phosphor::software::mctp
