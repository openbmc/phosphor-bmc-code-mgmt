#include "cpld/cpld_interface.hpp"

namespace phosphor::software::cpld
{

class LatticeCPLD : public CPLDInterface
{
  public:
    LatticeCPLD(const std::string& chipname, uint16_t bus, uint8_t address) :
        chipname(chipname), bus(bus), address(address)
    {}

    bool updateFirmware(bool force, const uint8_t* image,
                        size_t imageSize) final;

  private:
    std::string chipname;
    uint16_t bus;
    uint8_t address;
};

} // namespace phosphor::software::cpld
