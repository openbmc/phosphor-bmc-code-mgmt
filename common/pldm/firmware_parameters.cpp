#include "firmware_parameters.hpp"

#include <libpldm/firmware_update.h>
#include <libpldm/pldm.h>

#include <phosphor-logging/lg2.hpp>

#include <string_view>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::pldm
{

namespace
{

constexpr uint8_t instanceIdCount = 32;

/** @brief PLDM instance ID, rotated per request. */
uint8_t nextInstanceId()
{
    static uint8_t instanceId = 0;
    instanceId = (instanceId + 1) % instanceIdCount;
    return instanceId;
}

} // namespace

sdbusplus::async::task<std::optional<std::string>> getActiveFirmwareVersion(
    mctp::Transport& transport, uint8_t eid)
{
    std::vector<uint8_t> requestMsg(
        sizeof(pldm_msg_hdr) + PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_get_firmware_parameters_req(
        nextInstanceId(), PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES, request);
    if (rc != PLDM_SUCCESS)
    {
        error("Failed to encode PLDM request, RC: {RC}", "RC", rc);
        co_return std::nullopt;
    }

    auto responseMsg =
        co_await transport.sendReceive(eid, mctp::msgTypePldm, requestMsg);

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
        error("Device at EID {EID} reported an empty version string", "EID",
              eid);
        co_return std::nullopt;
    }

    activeVersion.resize(end + 1);

    co_return activeVersion;
}

} // namespace phosphor::software::pldm
