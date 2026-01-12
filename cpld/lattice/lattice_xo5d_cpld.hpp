#include "lattice_base_cpld.hpp"

namespace phosphor::software::cpld
{

/* Stable Value in OOB cammand packet format */
#define OOB_HEADER 0xBC

/* OOB command length limit */
#define DATA_MAX_SIZE 245
#define FRAME_MAX_SIZE 256
#define PACKAGE_LEN 128

/* Packet fragment flag */
#define FIRST_PACKET 0x8000
#define LAST_PACKET 0x4000
#define FIRST_AND_LAST_PACKET 0xC000
#define FIRST_PACKET_NUM 1

/* misc setting */
#define SLEEP_INTERVAL 10
#define RETRY_COUNT 5000
#define IMAGE_ID_1 1
#define IMAGE_ID_2 2
#define IMAGE_ID_SIZE 1
#define DRY_RUN_RESULT_SUCCESS 1

/**
OOB command structure, fill data according to this order.

Request packet format:
|1 byte|1 byte|   2 bytes   |1 byte|max 240 bytes| 1 byte |
|Header|Cmd id|Fragment flag|Lenght|     Data    |Checksum|

Response packet format:
|1 byte|1 byte|   2 bytes   |1 byte|1 byte|max 240 bytes| 1 byte |
|Header|Cmd id|Fragment flag|Lenght| Type |     Data    |Checksum|
*/
#define HEADER_POS 0
#define CMD_POS 1
#define FLAG_POS 2
#define LENGTH_POS 4
#define RECEIVE_TYPE_POS 5
#define RECEIVE_LEN_POS 7
#define RECEIVE_DATA_POS 6
#define RECEIVE_STATUS_POS 6
#define SEND_DATA_POS 5

#define OOB_READ_STATUS_LENGTH 10
#define OOB_READ_STABLE_LENGTH 7
#define OOB_WRITE_CMD_LENGTH 6

/* oob command */
enum
{
    OOB_DETECT_CHANNEL = 0x01,
    OOB_CONNECTION_DISABLE = 0x02,
    OOB_CUSTOMER_IMAGE_ERASE = 0x03,
    OOB_CUSTOMER_IMAGE_PROGRAM = 0x04,
    OOB_CUSTOMER_IMAGE_DRYRUN = 0x05,
    OOB_CUSTOMER_IMAGE_GET_STATUS = 0x06,
    OOB_KAK_REVOKE = 0x07,
    OOB_JTAG_PORT_LOCK = 0x08,
    OOB_JTAG_API_LOCK = 0x09,
    OOB_AUTH_AND_ZEROIZATION = 0x20,
    OOB_AUTH_AND_NULLLIFICATION = 0x21,
    OOB_WARM_REBOOT = 0x22,
    OOB_READ_CUSTOMER_POLICY = 0x40,
    OOB_ERASE_UFM = 0x41,
    OOB_PROGRAM_UFM = 0x42,
    OOB_READ_UFM = 0x43,
    OOB_GET_CUSTOMER_IMAGE_DRYRUN_RESULT = 0x44,
    OOB_SET_PRIMARY_IMAGE = 0x45,
    OOB_SET_CUSTOMER_IMAGE_VALID = 0x46,
    OOB_SET_MINIMUM_VERSION = 0x47,
    OOB_GET_SECTOR_LOCK_INFO = 0x48,
    OOB_SET_PROVISION_DONE = 0x49,
    OOB_SOFTWARE_REBOOT = 0x4A,
    OOB_USER_MSG_AUDIT = 0x4B,
    OOB_CHECK_CURRENT_RUNNING_IMAGE_STATUS = 0x4C,
    OOB_PROGRAM_CUSTOMER_POLICY = 0x4D,
    OOB_AUDIT_AND_LOCK_CUSTOMER_POLICY = 0x4E,
    OOB_AUTHORIZE_AND_ERASE_CUSTOMER_POLICY = 0x4F,
    OOB_PROGRAM_CUSTOMER_KEYS = 0x50,
    OOB_AUDIT_AND_LOCK_CUSTOMER_KEYS = 0x51,
    OOB_AUTHORIZE_AND_ERASE_CUSTOMER_KEYS = 0x52,
    OOB_READ_CUSTOMER_KEYS = 0x53,
    OOB_PROGRAM_CUSTOMER_CENTRAL_LOCKS = 0x54,
    OOB_AUDIT_AND_LOCK_CUSTOMER_CENTRAL_LOCKS = 0x55,
    OOB_AUTHORIZE_AND_ERASE_CUSTOMER_CENTRAL_LOCKS = 0x56,
    OOB_READ_CUSTOMER_CENTRAL_LOCKS = 0x57,
    OOB_LOCK_UNLOCK_UFM = 0x58,
    OOB_GET_DEVICE_UNIQUE_ID = 0x59,
    OOB_GET_MINIMUM_VERSION = 0x5A,
    OOB_GET_TRACE_ID = 0x5B,
    OOB_ERASE_UFM_SECTOR = 0x5C,
    OOB_GET_CMD_STATUS_LENGTH = 0x98,
    OOB_GET_DATA = 0x99,
};

/* oob status */
enum class oobStatus : uint8_t
{
    STATUS_SUCCESS = 0x00,
    STATUS_FAIL = 0x01,
    STATUS_DRY_RUN = 0x02,
    STATUS_CHECKSUM_ERROR = 0x22,
    STATUS_INVALID_COMMAND = 0x33,
    STATUS_INVALID_ARGUMENTS = 0x44,
    STATUS_TIMEOUT_NO_PACKET = 0x55,
    STATUS_BUSY = 0x66,
    STATUS_DEFAULT = 0x77,
    STATUS_I2C_API_ERROR = 0x80,
    STATUS_I2C_LENGTH_ERROR = 0x81,
    STATUS_LIB_HANDLE_ERROR = 0x82,
    STATUS_NO_CHANNEL_SCANED = 0x83,
    STATUS_CHANNEL_SELECT_ERROR = 0x84,
    STATUS_FRAME_HEADER_ERROR = 0x85,
    STATUS_FRAME_LENGTH_ERROR = 0x86,
    STATUS_FRAME_CHECKSUM_ERROR = 0x87,
};

typedef struct customerImageData
{
    uint8_t signature[96];
    uint8_t reserved1[160];
    uint32_t bitstreamVersion;
    uint8_t normalKeyBlob[360];
    uint8_t reserved2[144];
} customerImageData_t, *pCustomerImageData;

typedef struct customerImageStatus
{
    uint32_t imageId;
    uint32_t dsebVersion;
    uint32_t bitstreamVersion;
    uint32_t doneStatus;
    uint32_t cancelStatus;
    uint32_t valid;
    uint32_t corruptInfo;
    uint8_t kak[96];
    uint8_t isk[96];
    uint8_t ecc384Sig[96];
} customerImageStatus_t, *pCustomerImageStatus;

class LatticeXO5DCPLD : public LatticeBaseCPLD
{
  public:
    LatticeXO5DCPLD(sdbusplus::async::context& ctx, const uint16_t bus,
                    const uint8_t address, const std::string& chip,
                    const std::string& target, const bool debugMode) :
        LatticeBaseCPLD(ctx, bus, address, chip, target, debugMode)
    {}
    ~LatticeXO5DCPLD() override = default;
    LatticeXO5DCPLD(const LatticeXO5DCPLD&) = delete;
    LatticeXO5DCPLD& operator=(const LatticeXO5DCPLD&) = delete;
    LatticeXO5DCPLD(LatticeXO5DCPLD&&) noexcept = delete;
    LatticeXO5DCPLD& operator=(LatticeXO5DCPLD&&) noexcept = delete;

  protected:
    sdbusplus::async::task<bool> prepareUpdate(const uint8_t* image,
                                               size_t imageSize) override;
    sdbusplus::async::task<bool> doUpdate() override;
    sdbusplus::async::task<bool> finishUpdate() override;
    sdbusplus::async::task<bool> readUserCode(uint32_t& userCode) override;

  private:
    sdbusplus::async::task<bool> esfbWrite(uint16_t fragmentFlag, uint8_t cmdId,
                                           std::vector<uint8_t>& data);
    sdbusplus::async::task<bool> esfbReadStatus(
        uint8_t cmdId, oobStatus* status, uint16_t* data);
    sdbusplus::async::task<bool> esfbReadData(uint8_t cmdId,
                                              std::vector<uint8_t>& data);
    sdbusplus::async::task<bool> checkCurrentRunningImageStatus(
        uint32_t* bitStreamVersion);
    sdbusplus::async::task<bool> eraseNonActiveImage();
    sdbusplus::async::task<bool> programCustomerImage();
    sdbusplus::async::task<bool> dryRunCustomerImage();
    sdbusplus::async::task<bool> getDryRunCustomerImageResult();
    sdbusplus::async::task<bool> setPrimaryCustomerImage();

    static uint8_t checkSum(const std::vector<uint8_t>& vec);
    uint8_t nonActiveImageId;
    const uint8_t* pCustomerImage;
    uint32_t customerImageSize;
    const uint8_t* pNormalKeyBlob;
    uint32_t normalKeyBlobSize;
};

} // namespace phosphor::software::cpld
