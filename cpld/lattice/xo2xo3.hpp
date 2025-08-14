#include "lattice.hpp"

class XO2XO3FamilyUpdate : public CpldLatticeManager
{
  public:
    XO2XO3FamilyUpdate(sdbusplus::async::context& ctx, const uint16_t bus,
                       const uint8_t address, const std::string& chip,
                       const std::string& target, const bool debugMode) :
        CpldLatticeManager(ctx, bus, address, chip, target, debugMode)
    {}
    ~XO2XO3FamilyUpdate() override = default;
    XO2XO3FamilyUpdate(const XO2XO3FamilyUpdate&) = delete;
    XO2XO3FamilyUpdate& operator=(const XO2XO3FamilyUpdate&) = delete;
    XO2XO3FamilyUpdate(XO2XO3FamilyUpdate&&) noexcept = delete;
    XO2XO3FamilyUpdate& operator=(XO2XO3FamilyUpdate&&) noexcept = delete;

  protected:
    sdbusplus::async::task<bool> prepareUpdate(const uint8_t* image,
                                               size_t imageSize) override;
    sdbusplus::async::task<bool> doUpdate() override;
    sdbusplus::async::task<bool> finishUpdate() override;
    sdbusplus::async::task<bool> readUserCode(uint32_t& userCode) override;

  private:
    sdbusplus::async::task<bool> readDeviceId();
    sdbusplus::async::task<bool> eraseFlash();
    sdbusplus::async::task<bool> writeProgramPage();
    sdbusplus::async::task<bool> programUserCode();
};
