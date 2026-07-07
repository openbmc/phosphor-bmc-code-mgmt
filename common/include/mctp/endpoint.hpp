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
constexpr auto defaultTimeout =
    std::chrono::microseconds(std::chrono::seconds(3));

/** @brief Owns a long-lived AF_MCTP socket for request/response exchanges. */
class Transport
{
  public:
    Transport() = delete;
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;
    Transport(Transport&&) = delete;
    Transport& operator=(Transport&&) = delete;

    /** @throws std::system_error if the socket cannot be created */
    explicit Transport(sdbusplus::async::context& ctx,
                       std::chrono::microseconds timeout = defaultTimeout);
    ~Transport();

    /** @brief Send a message and await the response.
     *
     *  This suspends the calling coroutine rather than blocking the thread:
     *  the socket is non-blocking and the wait goes through
     *  sdbusplus::async::fdio, so the async context keeps servicing D-Bus
     *  while the response is outstanding.
     *
     *  Only one exchange may be in flight at a time; do not call this
     *  concurrently from multiple coroutines on the same Transport.
     *
     *  @returns the response payload, nullopt on error or timeout
     */
    sdbusplus::async::task<std::optional<std::vector<uint8_t>>> sendReceive(
        uint8_t eid, uint8_t msgType, const std::vector<uint8_t>& request);

  private:
    int fd = -1;
    sdbusplus::async::fdio fdio;
};

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
} // namespace phosphor::software::mctp
