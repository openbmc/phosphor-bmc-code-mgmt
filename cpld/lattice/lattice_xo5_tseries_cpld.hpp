#pragma once
#include "lattice_xo5_base_cpld.hpp"

namespace phosphor::software::cpld
{

class LatticeXO5TSeriesCPLD : public LatticeXO5BaseCPLD
{
  public:
    LatticeXO5TSeriesCPLD(sdbusplus::async::context& ctx, uint16_t bus,
                          uint8_t address, const std::string& chip,
                          const std::string& target, bool debugMode);

  protected:
    sdbusplus::async::task<bool> checkDeviceReady() override;
    sdbusplus::async::task<bool> prepareUpdate(const uint8_t* image,
                                               size_t imageSize) override;
    sdbusplus::async::task<bool> doErase() override;
    sdbusplus::async::task<bool> finishUpdate() override;
    sdbusplus::async::task<bool> readUserCode(uint32_t& userCode) override;

    uint8_t getCfgIdx(std::string_view target) const override;
    sdbusplus::async::task<bool> eraseCfg(
        std::optional<uint8_t> setIdx = std::nullopt) override;
    sdbusplus::async::task<bool> programCfg(
        std::optional<uint8_t> setIdx = std::nullopt,
        const std::vector<uint8_t>* customData = nullptr) override;
    sdbusplus::async::task<bool> verifyCfg() override;

  private:
    bool crc16Enabled = true;
    uint8_t softIpVersion = 0;

    static std::optional<std::vector<uint8_t>> calculateSha2_384Openssl(
        const std::vector<uint8_t>& input);

    static uint16_t crc16Ccitt(const uint8_t* data, size_t length);
    static uint16_t appendCrc16(std::vector<uint8_t>& data);

    sdbusplus::async::task<bool> lockI2C();
    sdbusplus::async::task<bool> readSoftIPId();
    sdbusplus::async::task<bool> toggleCrc16(bool enable);
    sdbusplus::async::task<bool> readCrcStatus(bool& crc_ok);
    bool isCrcRequired(uint8_t opcode) const;

    sdbusplus::async::task<bool> isCrcSuccessful(
        uint8_t opcode, const std::vector<uint8_t>& responseCrc);

    sdbusplus::async::task<bool> sendReceive(
        const std::vector<uint8_t>& request, std::vector<uint8_t>& response);

    sdbusplus::async::task<bool> readCfg(
        uint8_t targetCfg, std::vector<uint8_t>& dataOut, uint32_t bytesToRead);

    sdbusplus::async::task<bool> backupHeader();
    sdbusplus::async::task<bool> restoreHeader();
};

} // namespace phosphor::software::cpld
