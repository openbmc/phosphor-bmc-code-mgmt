#include "e810_device.hpp"

#include <libpldm/firmware_update.h>
#include <libpldm/pldm.h>
#include <linux/mctp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#ifndef MCTP_MSG_TYPE_PLDM
#define MCTP_MSG_TYPE_PLDM 1
#endif

using namespace phosphor::software;
PHOSPHOR_LOG2_USING;

constexpr auto e810UUID = "5af04860-05df-11e4-af79-000100000000";

E810Device::E810Device(sdbusplus::async::context& ctx,
                       uint64_t spiControllerIndex, uint64_t spiDeviceIndex,
                       bool dryRun, const std::vector<std::string>& gpioLinesIn,
                       const std::vector<bool>& gpioValuesIn,
                       SoftwareConfig& config, SoftwareManager* parent,
                       enum FlashLayout layout, enum FlashTool tool) :
    SPIDevice(ctx, spiControllerIndex, spiDeviceIndex, dryRun, gpioLinesIn,
              gpioValuesIn, config, parent, layout, tool)
{
    lg2::info("E810 Device initialized");
}

static std::optional<uint8_t> getE810Eid()
{
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto mapperCall = bus.new_method_call(
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetSubTree");
        mapperCall.append(
            "/", 0,
            std::vector<std::string>{"xyz.openbmc_project.MCTP.Endpoint"});

        auto mapperReply = bus.call(mapperCall);
        std::map<std::string, std::map<std::string, std::vector<std::string>>>
            tree;
        mapperReply.read(tree);

        for (const auto& [path, services] : tree)
        {
            for (const auto& [service, interfaces] : services)
            {
                try
                {
                    auto uuidCall = bus.new_method_call(
                        service.c_str(), path.c_str(),
                        "org.freedesktop.DBus.Properties", "Get");
                    uuidCall.append("xyz.openbmc_project.Common.UUID", "UUID");
                    auto uuidReply = bus.call(uuidCall);

                    std::variant<std::string> uuidVar;
                    uuidReply.read(uuidVar);

                    if (std::get<std::string>(uuidVar) == e810UUID)
                    {
                        auto eidCall = bus.new_method_call(
                            service.c_str(), path.c_str(),
                            "org.freedesktop.DBus.Properties", "Get");
                        eidCall.append("xyz.openbmc_project.MCTP.Endpoint",
                                       "EID");
                        auto eidReply = bus.call(eidCall);

                        std::variant<uint8_t> eidVar;
                        eidReply.read(eidVar);
                        return std::get<uint8_t>(eidVar);
                    }
                }
                catch (const sdbusplus::exception::SdBusError&)
                {
                    continue;
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to query E810 EID by UUID: {ERROR}", "ERROR",
                   e.what());
    }
    return std::nullopt;
}

std::string E810Device::getVersion()
{
    auto eidOpt = getE810Eid();
    if (!eidOpt)
    {
        lg2::warning("Could not find E810 EID on D-Bus");
        return "Unknown";
    }
    uint8_t eid = eidOpt.value();

    static uint8_t nextInstanceId = 0;
    uint8_t instanceId = nextInstanceId;
    nextInstanceId = (nextInstanceId + 1) % 32;

    std::vector<uint8_t> requestMsg(
        sizeof(pldm_msg_hdr) + PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES);
    auto request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_get_firmware_parameters_req(
        instanceId, PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES, request);
    if (rc != PLDM_SUCCESS)
    {
        lg2::error("Failed to encode PLDM request, RC: {RC}", "RC", rc);
        return "Unknown";
    }

    int fd = socket(AF_MCTP, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        lg2::error("Failed to create AF_MCTP socket");
        return "Unknown";
    }

    struct sockaddr_mctp addr;
    memset(&addr, 0, sizeof(addr));

    addr.smctp_family = AF_MCTP;
    addr.smctp_network = MCTP_NET_ANY;
    addr.smctp_addr.s_addr = eid;
    addr.smctp_type = MCTP_MSG_TYPE_PLDM;
    addr.smctp_tag = MCTP_TAG_OWNER;

    if (sendto(fd, requestMsg.data(), requestMsg.size(), 0,
               reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        lg2::error("Failed to send PLDM request to MCTP socket");
        close(fd);
        return "Unknown";
    }

    struct pollfd pfd = {fd, POLLIN, 0};
    int pollRes = poll(&pfd, 1, 3000);

    if (pollRes <= 0)
    {
        lg2::error("Timeout or error waiting for PLDM response from EID {EID}",
                   "EID", eid);
        close(fd);
        return "Unknown";
    }

    std::vector<uint8_t> responseMsg(1024);
    ssize_t recvLen = recvfrom(fd, responseMsg.data(), responseMsg.size(), 0,
                               nullptr, nullptr);
    close(fd);

    if (recvLen < static_cast<ssize_t>(sizeof(pldm_msg_hdr)))
    {
        lg2::error("Received invalid PLDM response size");
        return "Unknown";
    }

    auto response = reinterpret_cast<const pldm_msg*>(responseMsg.data());
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
        lg2::error(
            "Failed to decode PLDM response or bad completion code, RC: {RC}, CC: {CC}",
            "RC", rc, "CC", fwParams.completion_code);
        return "Unknown";
    }

    if (activeCompImageSetVerStr.ptr != nullptr &&
        activeCompImageSetVerStr.length > 0)
    {
        std::string version(
            reinterpret_cast<const char*>(activeCompImageSetVerStr.ptr),
            activeCompImageSetVerStr.length);

        version.erase(version.find_last_not_of(" \n\r\t\0") + 1);

        lg2::info("Successfully retrieved E810 Version via PLDM: {VER}", "VER",
                  version);
        return version;
    }

    return "Unknown";
}
