#include "lattice_base_cpld.hpp"

namespace phosphor::software::cpld
{
enum class oobStatus : uint8_t
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
