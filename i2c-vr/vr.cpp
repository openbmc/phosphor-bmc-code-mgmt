#include "vr.hpp"

#include "xdpe1x2xx/xdpe1x2xx.hpp"

#include <map>

namespace phosphor::software::VR
{

std::unique_ptr<VoltageRegulator> create(SDBusAsync::context& ctx, enum VRType vrType, uint8_t bus,
                                         uint8_t address)
{
    std::unique_ptr<phosphor::i2c::I2C> inf = std::make_unique<phosphor::i2c::I2C>(bus, address);
    std::unique_ptr<VoltageRegulator> ret;
    switch (vrType)
    {
        case VRType::XDPE1X2XX:
            ret = std::make_unique<XDPE1X2XX>(ctx, std::move(inf));
            break;
        default:
            return NULL;
    }
    return ret;
}

bool stringToEnum(std::string& vrStr, VRType& vrType)
{
    std::map<std::string, enum VRType> VRTypeToString{
        {"XDPE1X2XXFirmware", VRType::XDPE1X2XX},
    };

    if (VRTypeToString.contains(vrStr))
    {
        vrType = VRTypeToString[vrStr];
        return true;
    }
    return false;
}

}
