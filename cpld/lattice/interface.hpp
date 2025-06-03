#include "cpld/cpld_interface.hpp"

namespace phosphor::software::cpld
{

class LatticeCPLD : public CPLDInterface
{
  public:
    LatticeCPLD(sdbusplus::async::context& ctx, const std::string& chipname,
                uint16_t bus, uint8_t address,
                const std::string& hardwareCompatible) :
        CPLDInterface(ctx, chipname, bus, address, hardwareCompatible)
    {}

    sdbusplus::async::task<bool> updateFirmware(
        bool force, const uint8_t* image, size_t imageSize,
        std::function<bool(int)> progress) final;

    sdbusplus::async::task<bool> getVersion(std::string& version) final;
};

} // namespace phosphor::software::cpld
