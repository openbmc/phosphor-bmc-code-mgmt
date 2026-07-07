#include "firmware_parameters.hpp"

#include <libpldm/firmware_update.h>
#include <libpldm/instance-id.h>
#include <libpldm/pldm.h>

#include <phosphor-logging/lg2.hpp>

#include <string_view>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::pldm
{

namespace
{

/** @brief Handle to the instance ID database, shared with pldmd so that
 *         IDs in flight to the same endpoint don't collide.
 *  @returns nullptr if the database could not be opened
 */
pldm_instance_db* instanceDb()
{
    static pldm_instance_db* db = []() -> pldm_instance_db* {
        pldm_instance_db* handle = nullptr;
        int rc = pldm_instance_db_init_default(&handle);

        if (rc != 0)
        {
            error("Failed to open PLDM instance ID database, RC: {RC}", "RC",
                  rc);
            return nullptr;
        }

        return handle;
    }();

    return db;
}

/** @brief Holds an allocated PLDM instance ID and releases it on scope exit. */
class InstanceId
{
  public:
    InstanceId() = delete;
    InstanceId(const InstanceId&) = delete;
    InstanceId& operator=(const InstanceId&) = delete;
    InstanceId(InstanceId&&) = delete;
    InstanceId& operator=(InstanceId&&) = delete;

    InstanceId(pldm_instance_db* inDb, uint8_t inEid) :
        db(inDb), eid(inEid), valid(pldm_instance_id_alloc(db, eid, &id) == 0)
    {}

    ~InstanceId()
    {
        if (valid)
        {
            pldm_instance_id_free(db, eid, id);
        }
    }

    bool isValid() const
    {
        return valid;
    }

    pldm_instance_id_t value() const
    {
        return id;
    }

  private:
    pldm_instance_db* db;
    uint8_t eid;
    pldm_instance_id_t id = 0;
    bool valid = false;
};

} // namespace

sdbusplus::async::task<std::optional<std::string>> getActiveFirmwareVersion(
    mctp::Transport& transport, uint8_t eid)
{
    auto* db = instanceDb();
    if (db == nullptr)
    {
        co_return std::nullopt;
    }

    // Held until the exchange completes, so the ID stays reserved while the
    // request is in flight.
    InstanceId instanceId(db, eid);
    if (!instanceId.isValid())
    {
        error("Failed to allocate PLDM instance ID for EID {EID}", "EID", eid);
        co_return std::nullopt;
    }

    std::vector<uint8_t> requestMsg(
        sizeof(pldm_msg_hdr) + PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* request = reinterpret_cast<pldm_msg*>(requestMsg.data());

    auto rc = encode_get_firmware_parameters_req(
        instanceId.value(), PLDM_GET_FIRMWARE_PARAMETERS_REQ_BYTES, request);
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
    // the tail with null ('\0') bytes and whitespace. Trim that padding off.
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
