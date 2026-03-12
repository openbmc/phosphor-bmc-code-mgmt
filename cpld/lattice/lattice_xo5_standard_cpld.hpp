#pragma once
#include "lattice_xo5_base_cpld.hpp"

namespace phosphor::software::cpld
{

class LatticeXO5StandardCPLD : public LatticeXO5BaseCPLD
{
  public:
    using LatticeXO5BaseCPLD::LatticeXO5BaseCPLD;

  protected:
    sdbusplus::async::task<bool> prepareUpdate(const uint8_t* image,
                                               size_t imageSize) override;
    sdbusplus::async::task<bool> doErase() override;
    sdbusplus::async::task<bool> finishUpdate() override;
    sdbusplus::async::task<bool> readUserCode(uint32_t& userCode) override;

    uint8_t getCfgIdx(std::string_view target) const override;
    sdbusplus::async::task<bool> waitUntilReady(
        std::chrono::milliseconds timeout) override;
    sdbusplus::async::task<bool> eraseCfg(
        std::optional<uint8_t> setIdx = std::nullopt) override;
    sdbusplus::async::task<bool> programCfg(
        std::optional<uint8_t> setIdx = std::nullopt,
        const std::vector<uint8_t>* customData = nullptr) override;
    sdbusplus::async::task<bool> verifyCfg() override;

  private:
    static constexpr std::chrono::milliseconds readyPollInterval{10};

    sdbusplus::async::task<bool> programPage(uint8_t block, uint8_t page,
                                             const std::vector<uint8_t>& data);
    sdbusplus::async::task<bool> readPage(uint8_t block, uint8_t page,
                                          std::vector<uint8_t>& data);
};

} // namespace phosphor::software::cpld
