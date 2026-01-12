#include "lattice_base_cpld.hpp"

namespace phosphor::software::cpld
{

class LatticeXO3CPLD : public LatticeBaseCPLD
{
  public:
    LatticeXO3CPLD(sdbusplus::async::context& ctx, const uint16_t bus,
                   const uint8_t address, const std::string& chip,
                   const std::string& target, const bool debugMode) :
        LatticeBaseCPLD(ctx, bus, address, chip, target, debugMode)
    {}
    ~LatticeXO3CPLD() override = default;
    LatticeXO3CPLD(const LatticeXO3CPLD&) = delete;
    LatticeXO3CPLD& operator=(const LatticeXO3CPLD&) = delete;
    LatticeXO3CPLD(LatticeXO3CPLD&&) noexcept = delete;
    LatticeXO3CPLD& operator=(LatticeXO3CPLD&&) noexcept = delete;

  protected:
    sdbusplus::async::task<bool> prepareUpdate(const uint8_t* image,
                                               size_t imageSize) override;
    sdbusplus::async::task<bool> doErase() override;
    sdbusplus::async::task<bool> doUpdate() override;
    sdbusplus::async::task<bool> finishUpdate() override;

  private:
    sdbusplus::async::task<bool> readUserCode(uint32_t& userCode) override;
    sdbusplus::async::task<bool> readDeviceId();
    sdbusplus::async::task<bool> eraseFlash();
    sdbusplus::async::task<bool> writeProgramPage();
    sdbusplus::async::task<bool> programUserCode();
    sdbusplus::async::task<bool> programSinglePage(
        uint16_t pageOffset, std::span<const uint8_t> pageData);
    sdbusplus::async::task<bool> verifySinglePage(
        uint16_t pageOffset, std::span<const uint8_t> pageData);
};

} // namespace phosphor::software::cpld
