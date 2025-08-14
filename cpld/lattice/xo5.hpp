#include "lattice.hpp"

class XO5FamilyUpdate : public CpldLatticeManager
{
  public:
    XO5FamilyUpdate(sdbusplus::async::context& ctx, const uint16_t bus,
                    const uint8_t address, const std::string& chip,
                    const std::string& target, const bool debugMode) :
        CpldLatticeManager(ctx, bus, address, chip, target, debugMode)
    {}
    ~XO5FamilyUpdate() override = default;
    XO5FamilyUpdate(const XO5FamilyUpdate&) = delete;
    XO5FamilyUpdate& operator=(const XO5FamilyUpdate&) = delete;
    XO5FamilyUpdate(XO5FamilyUpdate&&) noexcept = delete;
    XO5FamilyUpdate& operator=(XO5FamilyUpdate&&) noexcept = delete;

  protected:
    sdbusplus::async::task<bool> prepareUpdate(const uint8_t* image,
                                               size_t imageSize) override;
    sdbusplus::async::task<bool> doUpdate() override;
    sdbusplus::async::task<bool> finishUpdate() override;
    sdbusplus::async::task<bool> readUserCode(uint32_t& userCode) override;

  private:
    bool waitUntilReady(std::chrono::milliseconds timeout);
    sdbusplus::async::task<bool> eraseCfg();
    sdbusplus::async::task<bool> programCfg();
    sdbusplus::async::task<bool> programPage(uint8_t block, uint8_t page,
                                             const std::vector<uint8_t>& data);
    sdbusplus::async::task<bool> verifyCfg();
    sdbusplus::async::task<bool> readPage(uint8_t block, uint8_t page,
                                          std::vector<uint8_t>& data);
};
