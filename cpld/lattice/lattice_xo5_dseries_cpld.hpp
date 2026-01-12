#include "lattice_base_cpld.hpp"

namespace phosphor::software::cpld
{
class LatticeXO5DSeriesCPLD : public LatticeBaseCPLD
{
  public:
    LatticeXO5DSeriesCPLD(sdbusplus::async::context& ctx, const uint16_t bus,
                          const uint8_t address, const std::string& chip,
                          const std::string& target, const bool debugMode) :
        LatticeBaseCPLD(ctx, bus, address, chip, target, debugMode)
    {}

  protected:
    sdbusplus::async::task<bool> prepareUpdate(const uint8_t* image,
                                               size_t imageSize) override;
    sdbusplus::async::task<bool> doErase() override;
    sdbusplus::async::task<bool> doUpdate() override;
    sdbusplus::async::task<bool> finishUpdate() override;
    sdbusplus::async::task<bool> readUserCode(uint32_t& userCode) override;

  private:
    enum class EsfbStatus : uint8_t
    {
        success = 0x00,
        fail = 0x01,
        dryRun = 0x02,
        checksumError = 0x22,
        invalidCommand = 0x33,
        invalidArguments = 0x44,
        timeoutNoPacket = 0x55,
        busy = 0x66,
        defaultStatus = 0x77,
        i2cApiError = 0x80,
        i2cLengthError = 0x81,
        libHandleError = 0x82,
        noChannelScanned = 0x83,
        channelSelectError = 0x84,
        frameHeaderError = 0x85,
        frameLengthError = 0x86,
        frameChecksumError = 0x87,
    };

    enum class EsfbCommand : uint8_t
    {
        detectChannel = 0x01,
        connectionDisable = 0x02,
        customerImageErase = 0x03,
        customerImageProgram = 0x04,
        customerImageDryRun = 0x05,
        customerImageGetStatus = 0x06,
        kakRevoke = 0x07,
        jtagPortLock = 0x08,
        jtagApiLock = 0x09,
        authAndZeroization = 0x20,
        authAndNullification = 0x21,
        warmReboot = 0x22,
        readCustomerPolicy = 0x40,
        eraseUfm = 0x41,
        programUfm = 0x42,
        readUfm = 0x43,
        getCustomerImageDryRunResult = 0x44,
        setPrimaryImage = 0x45,
        setCustomerImageValid = 0x46,
        setMinimumVersion = 0x47,
        getSectorLockInfo = 0x48,
        setProvisionDone = 0x49,
        softwareReboot = 0x4A,
        userMsgAudit = 0x4B,
        checkCurrentRunningImageStatus = 0x4C,
        programCustomerPolicy = 0x4D,
        auditAndLockCustomerPolicy = 0x4E,
        authorizeAndEraseCustomerPolicy = 0x4F,
        programCustomerKeys = 0x50,
        auditAndLockCustomerKeys = 0x51,
        authorizeAndEraseCustomerKeys = 0x52,
        readCustomerKeys = 0x53,
        programCustomerCentralLocks = 0x54,
        auditAndLockCustomerCentralLocks = 0x55,
        authorizeAndEraseCustomerCentralLocks = 0x56,
        readCustomerCentralLocks = 0x57,
        lockUnlockUfm = 0x58,
        getDeviceUniqueId = 0x59,
        getMinimumVersion = 0x5A,
        getTraceId = 0x5B,
        eraseUfmSector = 0x5C,
        getCmdStatusLength = 0x98,
        getData = 0x99,
    };

    enum class ImageId : uint8_t
    {
        id1 = 1,
        id2 = 2
    };

    struct __attribute__((packed)) CustomerImageData
    {
        uint8_t signature[96];
        uint8_t reserved1[160];
        uint32_t bitstreamVersion;
        uint8_t normalKeyBlob[360];
        uint8_t reserved2[144];
    };

    struct __attribute__((packed)) CustomerImageStatus
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
        uint8_t ecc384Signature[96];
    };

    sdbusplus::async::task<bool> esfbWrite(uint16_t fragmentFlag, uint8_t cmdId,
                                           std::vector<uint8_t>& data);
    sdbusplus::async::task<bool> esfbReadStatus(
        uint8_t cmdId, LatticeXO5DSeriesCPLD::EsfbStatus& status,
        uint16_t& data);
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
    ImageId nonActiveImageId;
    const uint8_t* pCustomerImage;
    size_t customerImageSize;
    const uint8_t* pNormalKeyBlob;
    size_t normalKeyBlobSize;
};

} // namespace phosphor::software::cpld
