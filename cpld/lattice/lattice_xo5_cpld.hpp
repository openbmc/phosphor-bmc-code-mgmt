#include "lattice_base_cpld.hpp"

namespace phosphor::software::cpld
{

class LatticeXO5CPLD : public LatticeBaseCPLD
{
  public:
    LatticeXO5CPLD(sdbusplus::async::context& ctx, const uint16_t bus,
                   const uint8_t address, const std::string& chip,
                   const std::string& target, const bool debugMode) :
        LatticeBaseCPLD(ctx, bus, address, chip, target, debugMode)
    {}
    ~LatticeXO5CPLD() override = default;
    LatticeXO5CPLD(const LatticeXO5CPLD&) = delete;
    LatticeXO5CPLD& operator=(const LatticeXO5CPLD&) = delete;
    LatticeXO5CPLD(LatticeXO5CPLD&&) noexcept = delete;
    LatticeXO5CPLD& operator=(LatticeXO5CPLD&&) noexcept = delete;

  protected:
    sdbusplus::async::task<bool> prepareUpdate(const uint8_t* image,
                                               size_t imageSize) override;
    sdbusplus::async::task<bool> doUpdate() override;
    sdbusplus::async::task<bool> finishUpdate() override;
    sdbusplus::async::task<bool> readUserCode(uint32_t& userCode) override;

  private:
    sdbusplus::async::task<bool> waitUntilReady(
        std::chrono::milliseconds timeout);
    sdbusplus::async::task<bool> eraseCfg();
    sdbusplus::async::task<bool> programCfg();
    sdbusplus::async::task<bool> programPage(uint8_t block, uint8_t page,
                                             const std::vector<uint8_t>& data);
    sdbusplus::async::task<bool> verifyCfg();
    sdbusplus::async::task<bool> readPage(uint8_t block, uint8_t page,
                                          std::vector<uint8_t>& data);
};

class LatticeXO5CPLDJtag : public LatticeBaseCPLDJtag
{
  public:
    LatticeXO5CPLDJtag(
        sdbusplus::async::context& ctx, uint16_t bus, uint8_t address,
        const std::string& jtagIndex, const std::string& chipName,
        const std::string& chipModel, const std::vector<std::string>& gpioLines,
        const std::vector<std::string>& gpioValues) :
        LatticeBaseCPLDJtag(ctx, bus, address, jtagIndex, chipName, chipModel,
                            gpioLines, gpioValues)

    {}

    ~LatticeXO5CPLDJtag() override = default;
    sdbusplus::async::task<bool> getVersion(std::string& version) override;
};

} // namespace phosphor::software::cpld
