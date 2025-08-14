#include "cpld/cpld_interface.hpp"
#include "cpld/lattice/lattice_base_cpld.hpp"

namespace phosphor::software::cpld
{

class LatticeCPLDFactory : public CPLDInterface
{
  public:
    LatticeCPLDFactory(sdbusplus::async::context& ctx,
                       const std::string& chipName, latticeChip chipEnum,
                       uint16_t bus, uint8_t address) :
        CPLDInterface(ctx, chipName, bus, address), chipEnum(chipEnum)
    {}

    sdbusplus::async::task<bool> updateFirmware(
        bool force, const uint8_t* image, size_t imageSize,
        std::function<bool(int)> progress) final;

    sdbusplus::async::task<bool> getVersion(std::string& version) final;

  private:
    std::unique_ptr<LatticeBaseCPLD> getLatticeCPLD();
    latticeChip chipEnum;
};

} // namespace phosphor::software::cpld
