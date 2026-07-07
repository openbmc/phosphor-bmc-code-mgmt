#include "mctp/endpoint.hpp"

#include <linux/mctp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/Common/UUID/client.hpp>
#include <xyz/openbmc_project/MCTP/Endpoint/client.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

#include <cerrno>
#include <chrono>
#include <map>
#include <string>
#include <system_error>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::mctp
{

namespace
{

using ObjectMapper = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>;
using EndpointIntf = sdbusplus::client::xyz::openbmc_project::mctp::Endpoint<>;
using UUIDIntf = sdbusplus::client::xyz::openbmc_project::common::UUID<>;

/** @brief GetSubTree return type: object path -> service -> interfaces */
using SubTreeType =
    std::map<std::string, std::map<std::string, std::vector<std::string>>>;

constexpr size_t maxResponseBytes = 1024;

/** @brief Create the AF_MCTP socket, throwing if it fails.
 *  @throws std::system_error on failure
 */
int makeSocket()
{
    int fd = socket(AF_MCTP, SOCK_DGRAM | SOCK_NONBLOCK, 0);

    if (fd < 0)
    {
        throw std::system_error(errno, std::system_category(),
                                "Failed to create AF_MCTP socket");
    }

    return fd;
}

} // namespace

sdbusplus::async::task<std::optional<uint8_t>> findEndpoint(
    sdbusplus::async::context& ctx, const std::string& uuid)
{
    SubTreeType tree;

    try
    {
        tree = co_await ObjectMapper(ctx)
                   .service(ObjectMapper::default_service)
                   .path(ObjectMapper::instance_path)
                   .get_sub_tree("/", 0, {EndpointIntf::interface});
    }
    catch (const std::exception& e)
    {
        debug("No MCTP endpoint present yet: {ERROR}", "ERROR", e);
        co_return std::nullopt;
    }

    for (const auto& [path, services] : tree)
    {
        for (const auto& [service, interfaces] : services)
        {
            try
            {
                auto endpointUUID =
                    co_await UUIDIntf(ctx).service(service).path(path).uuid();

                if (endpointUUID != uuid)
                {
                    continue;
                }

                co_return co_await EndpointIntf(ctx)
                    .service(service)
                    .path(path)
                    .eid();
            }
            catch (const std::exception& e)
            {
                debug("Skipping endpoint {PATH}: {ERROR}", "PATH", path,
                      "ERROR", e);
                continue;
            }
        }
    }

    co_return std::nullopt;
}

sdbusplus::async::task<uint8_t> waitForEndpoint(sdbusplus::async::context& ctx,
                                                const std::string& uuid)
{
    // Subscribe before enumerating, otherwise an endpoint appearing between
    // the two steps would be missed.
    sdbusplus::async::match endpointAdded(
        ctx, sdbusplus::bus::match::rules::interfacesAdded());

    while (!ctx.stop_requested())
    {
        auto eid = co_await findEndpoint(ctx, uuid);

        if (eid.has_value())
        {
            co_return eid.value();
        }

        debug("Waiting for MCTP endpoint {UUID}", "UUID", uuid);
        co_await endpointAdded.next();
    }

    co_return 0;
}

Transport::Transport(sdbusplus::async::context& ctx,
                     std::chrono::microseconds timeout) :
    fd(makeSocket()), fdio(ctx, fd, timeout)
{}

Transport::~Transport()
{
    if (fd >= 0)
    {
        close(fd);
    }
}

sdbusplus::async::task<std::optional<std::vector<uint8_t>>>
    Transport::sendReceive(uint8_t eid, uint8_t msgType,
                           const std::vector<uint8_t>& request)
{
    struct sockaddr_mctp addr{};
    addr.smctp_family = AF_MCTP;
    addr.smctp_network = MCTP_NET_ANY;
    addr.smctp_addr.s_addr = eid;
    addr.smctp_type = msgType;
    addr.smctp_tag = MCTP_TAG_OWNER;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    if (sendto(fd, request.data(), request.size(), 0,
               reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        error("Failed to send MCTP message to EID {EID}", "EID", eid);
        co_return std::nullopt;
    }

    try
    {
        co_await fdio.next();
    }
    catch (const sdbusplus::async::fdio_timeout_exception&)
    {
        error("Timed out waiting for MCTP response from EID {EID}", "EID", eid);
        co_return std::nullopt;
    }

    std::vector<uint8_t> response(maxResponseBytes);
    auto len =
        recvfrom(fd, response.data(), response.size(), 0, nullptr, nullptr);

    if (len <= 0)
    {
        error("Failed to read MCTP response from EID {EID}", "EID", eid);
        co_return std::nullopt;
    }

    response.resize(len);
    co_return response;
}
} // namespace phosphor::software::mctp
