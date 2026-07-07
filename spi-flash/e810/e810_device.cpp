#include "e810_device.hpp"

#include "mctp/endpoint.hpp"

#include <libpldm/firmware_update.h>
#include <libpldm/pldm.h>

#include <phosphor-logging/lg2.hpp>

#include <string_view>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software;

namespace
{

constexpr auto e810UUID = "5af04860-05df-11e4-af79-000100000000";

constexpr uint8_t instanceIdCount = 32;

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

    // Version discovery runs in the background so neither service startup
    // nor getVersion() ever waits on MCTP.
    ctx.spawn(refreshVersion());
}

std::string E810Device::getVersion()
{
    return version;
}

sdbusplus::async::task<> E810Device::refreshVersion()
{
    auto eid = co_await mctp::waitForEndpoint(ctx, e810UUID);

    auto newVersion = co_await queryVersion(eid);
    if (!newVersion.has_value())
    {
        co_return;
    }

    version = newVersion.value();

    if (softwareCurrent)
    {
        softwareCurrent->setVersion(version,
                                    SoftwareVersion::VersionPurpose::Other);
    }

    debug("E810 version is {VERSION}", "VERSION", version);
    co_return;
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

    auto responseMsg =
        co_await mctp::sendReceive(ctx, eid, mctp::msgTypePldm, requestMsg);

    if (!responseMsg.has_value() || responseMsg->size() < sizeof(pldm_msg_hdr))
    {
        error("Invalid PLDM response from EID {EID}", "EID", eid);
        co_return std::nullopt;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto* response =
        reinterpret_cast<const pldm_msg*>(responseMsg->data());
    size_t respPayloadLen = responseMsg->size() - sizeof(pldm_msg_hdr);

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

    // The version is reported in a fixed-length field, so the device pads
    // the tail with NUL bytes and whitespace. Trim that padding off.
    constexpr std::string_view padding{" \t\r\n\0", 5};
    const auto end = activeVersion.find_last_not_of(padding);

    if (end == std::string::npos)
    {
        error("E810 reported an empty version string");
        co_return std::nullopt;
    }

    activeVersion.resize(end + 1);

    co_return activeVersion;
}
