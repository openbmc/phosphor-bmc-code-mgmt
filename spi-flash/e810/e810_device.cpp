#include "e810_device.hpp"

#include <libpldm/firmware_update.h>
#include <libpldm/pldm.h>
#include <linux/mctp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/bus/match.hpp>

#include <array>
#include <cstring>
#include <map>
#include <string>
#include <variant>
#include <vector>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software;

namespace
{

constexpr auto e810UUID = "5af04860-05df-11e4-af79-000100000000";
constexpr auto mapperService = "xyz.openbmc_project.ObjectMapper";
constexpr auto mapperPath = "/xyz/openbmc_project/object_mapper";
constexpr auto mapperIface = "xyz.openbmc_project.ObjectMapper";
constexpr auto endpointIface = "xyz.openbmc_project.MCTP.Endpoint";
constexpr auto uuidIface = "xyz.openbmc_project.Common.UUID";

// PLDM over MCTP message type, DSP0245
constexpr uint8_t mctpMsgTypePldm = 1;

constexpr size_t maxResponseBytes = 1024;
constexpr uint8_t instanceIdCount = 32;

using SubTreeType =
    std::map<std::string, std::map<std::string, std::vector<std::string>>>;

/** @brief Closes the MCTP socket when leaving scope. */
struct FdGuard
{
    explicit FdGuard(int inFd) : fd(inFd) {}
    ~FdGuard()
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }
    FdGuard(const FdGuard&) = delete;
    FdGuard& operator=(const FdGuard&) = delete;
    FdGuard(FdGuard&&) = delete;
    FdGuard& operator=(FdGuard&&) = delete;

    int fd = -1;
};

} // namespace

E810Device::E810Device(sdbusplus::async::context& ctx,
                       uint64_t inSpiControllerIndex, uint64_t inSpiDeviceIndex,
                       bool inDryRun,
                       const std::vector<std::string>& inGpioLines,
                       const std::vector<bool>& inGpioValues,
                       SoftwareConfig& inConfig, SoftwareManager* inParent) :
    SPIDevice(ctx, inSpiControllerIndex, inSpiDeviceIndex, inDryRun,
              inGpioLines, inGpioValues, inConfig, inParent, flashLayoutFlat,
              flashToolFlashcp)
{
    debug("E810 device initialized");
    ctx.spawn(refreshVersion());
}

std::string E810Device::getVersion()
{
    return version;
}

sdbusplus::async::task<> E810Device::refreshVersion()
{
    // The MCTP endpoint may not be discovered yet when this service starts,
    // so watch for new endpoints and retry until the E810 shows up.
    sdbusplus::async::match endpointAdded(
        ctx, sdbusplus::bus::match::rules::interfacesAdded());

    while (!ctx.stop_requested())
    {
        auto eid = co_await getEndpointId();

        if (eid.has_value())
        {
            auto newVersion = co_await queryVersion(eid.value());

            if (newVersion.has_value())
            {
                version = newVersion.value();

                if (softwareCurrent)
                {
                    softwareCurrent->setVersion(
                        version, SoftwareVersion::VersionPurpose::Other);
                }

                debug("E810 version is {VERSION}", "VERSION", version);
                co_return;
            }
        }

        debug("E810 MCTP endpoint not ready, waiting for new endpoints");
        co_await endpointAdded.next();
    }

    co_return;
}

sdbusplus::async::task<std::optional<uint8_t>> E810Device::getEndpointId()
{
    SubTreeType tree;

    try
    {
        tree = co_await sdbusplus::async::proxy()
                   .service(mapperService)
                   .path(mapperPath)
                   .interface(mapperIface)
                   .call<SubTreeType>(ctx, "GetSubTree", std::string{"/"},
                                      int32_t{0},
                                      std::vector<std::string>{endpointIface});
    }
    catch (const std::exception& e)
    {
        debug("No MCTP endpoint found: {ERROR}", "ERROR", e.what());
        co_return std::nullopt;
    }

    for (const auto& [path, services] : tree)
    {
        for (const auto& [service, interfaces] : services)
        {
            try
            {
                auto uuid = co_await sdbusplus::async::proxy()
                                .service(service)
                                .path(path)
                                .interface(uuidIface)
                                .get_property<std::string>(ctx, "UUID");

                if (uuid != e810UUID)
                {
                    continue;
                }

                co_return co_await sdbusplus::async::proxy()
                    .service(service)
                    .path(path)
                    .interface(endpointIface)
                    .get_property<uint8_t>(ctx, "EID");
            }
            catch (const std::exception& e)
            {
                debug("Skipping endpoint {PATH}: {ERROR}", "PATH", path,
                      "ERROR", e.what());
                continue;
            }
        }
    }

    co_return std::nullopt;
}

sdbusplus::async::task<std::optional<std::string>> E810Device::queryVersion(
    uint8_t eid)
{
    std::vector<uint8_t> requestMsg(
        sizeof(pldm_msg_hdr) + PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    instanceId = (instanceId + 1) % instanceIdCount;

    auto rc = encode_get_firmware_parameters_req(
        instanceId, PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES, request);
    if (rc != PLDM_SUCCESS)
    {
        error("Failed to encode PLDM request, RC: {RC}", "RC", rc);
        co_return std::nullopt;
    }

    FdGuard sock(socket(AF_MCTP, SOCK_DGRAM | SOCK_NONBLOCK, 0));
    if (sock.fd < 0)
    {
        error("Failed to create AF_MCTP socket");
        co_return std::nullopt;
    }

    struct sockaddr_mctp addr{};
    addr.smctp_family = AF_MCTP;
    addr.smctp_network = MCTP_NET_ANY;
    addr.smctp_addr.s_addr = eid;
    addr.smctp_type = mctpMsgTypePldm;
    addr.smctp_tag = MCTP_TAG_OWNER;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    if (sendto(sock.fd, requestMsg.data(), requestMsg.size(), 0,
               reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        error("Failed to send PLDM request to EID {EID}", "EID", eid);
        co_return std::nullopt;
    }

    sdbusplus::async::fdio fdio(ctx, sock.fd);
    co_await fdio.next();

    std::vector<uint8_t> responseMsg(maxResponseBytes);
    auto recvLen = recvfrom(sock.fd, responseMsg.data(), responseMsg.size(), 0,
                            nullptr, nullptr);

    if (recvLen < static_cast<ssize_t>(sizeof(pldm_msg_hdr)))
    {
        error("Received invalid PLDM response size from EID {EID}", "EID", eid);
        co_return std::nullopt;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto* response =
        reinterpret_cast<const pldm_msg*>(responseMsg.data());
    size_t respPayloadLen = recvLen - sizeof(pldm_msg_hdr);

    pldm_get_firmware_parameters_resp fwParams{};
    variable_field activeCompImageSetVerStr{};
    variable_field pendingCompImageSetVerStr{};
    variable_field compParamTable{};

    rc = decode_get_firmware_parameters_resp(
        response, respPayloadLen, &fwParams, &activeCompImageSetVerStr,
        &pendingCompImageSetVerStr, &compParamTable);

    if (rc != PLDM_SUCCESS || fwParams.completion_code != PLDM_SUCCESS)
    {
        error("Failed to decode PLDM response, RC: {RC}, CC: {CC}", "RC", rc,
              "CC", fwParams.completion_code);
        co_return std::nullopt;
    }

    if (activeCompImageSetVerStr.ptr == nullptr ||
        activeCompImageSetVerStr.length == 0)
    {
        co_return std::nullopt;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    std::string activeVersion(
        reinterpret_cast<const char*>(activeCompImageSetVerStr.ptr),
        activeCompImageSetVerStr.length);

    auto end = activeVersion.find_last_not_of(std::string(" \n\r\t", 4) + '\0');
    activeVersion.erase(end == std::string::npos ? 0 : end + 1);

    co_return activeVersion;
}
