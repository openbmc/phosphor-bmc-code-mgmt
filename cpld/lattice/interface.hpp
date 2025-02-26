#include "cpld/cpld_interface.hpp"

namespace phosphor::software::cpld
{

class LatticeCPLD : public CPLDInterface
{
  public:
    LatticeCPLD(const std::string& chipname, uint16_t bus, uint8_t address) :
        CPLDInterface(chipname, bus, address)
    {}

    sdbusplus::async::task<bool> updateFirmware(
        bool force, const uint8_t* image, size_t imageSize) final;
};

} // namespace phosphor::software::cpld
