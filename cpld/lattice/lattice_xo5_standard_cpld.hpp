#pragma once
#include "lattice_xo5_base_cpld.hpp"

namespace phosphor::software::cpld
{

class LatticeXO5StandardCPLD : public LatticeXO5BaseCPLD
{
  public:
    LatticeXO5StandardCPLD(sdbusplus::async::context& ctx, uint16_t bus,
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
    sdbusplus::async::task<bool> programPage(uint8_t block, uint8_t page,
                                             const std::vector<uint8_t>& data);
    sdbusplus::async::task<bool> readPage(uint8_t block, uint8_t page,
                                          std::vector<uint8_t>& data);
    sdbusplus::async::task<bool> programDone();
};

} // namespace phosphor::software::cpld
