#include "lattice_base_cpld.hpp"

namespace phosphor::software::cpld
{

class LatticeXO5CPLD2 : public LatticeBaseCPLD
{
  public:
    LatticeXO5CPLD2(sdbusplus::async::context& ctx, const uint16_t bus,
                   const uint8_t address, const std::string& chip,
                   const std::string& target, const bool debugMode) :
        LatticeBaseCPLD(ctx, bus, address, chip, target, debugMode)
    {}
    ~LatticeXO5CPLD2() override = default;
    LatticeXO5CPLD2(const LatticeXO5CPLD2&) = delete;
    LatticeXO5CPLD2& operator=(const LatticeXO5CPLD2&) = delete;
    LatticeXO5CPLD2(LatticeXO5CPLD2&&) noexcept = delete;
    LatticeXO5CPLD2& operator=(LatticeXO5CPLD2&&) noexcept = delete;

  protected:
    sdbusplus::async::task<bool> prepareUpdate(const uint8_t* image,
                                               size_t imageSize) override;
    sdbusplus::async::task<bool> doErase() override;
    sdbusplus::async::task<bool> doUpdate() override;
    sdbusplus::async::task<bool> finishUpdate() override;
    sdbusplus::async::task<bool> readUserCode(uint32_t& userCode) override;
    sdbusplus::async::task<bool> readSoftIPId();

  private:
    sdbusplus::async::task<bool> waitUntilReady(std::chrono::milliseconds timeout);
    sdbusplus::async::task<bool> eraseCfg();
    sdbusplus::async::task<bool> programCfg();
    sdbusplus::async::task<bool> verifyCfg();
    
    sdbusplus::async::task<bool> toggle_crc16(bool enable);
    sdbusplus::async::task<bool> read_crc_status(bool& crc_ok);
    sdbusplus::async::task<bool> sendReceive(const std::vector<uint8_t>& request, std::vector<uint8_t>& response);
  private:
    bool crc16Enabled_ = true;
};

} // namespace phosphor::software::cpld
