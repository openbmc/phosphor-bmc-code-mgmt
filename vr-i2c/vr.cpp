#include "vr.hpp"

#include "xdpe1x2xx/xdpe152xx.hpp"

#include <map>

namespace VR
{

std::unique_ptr<VoltageRegulator> create(std::string& vrType, uint8_t bus,
                                         uint8_t address)
{
    std::unique_ptr<i2c::I2C> inf = std::make_unique<i2c::I2C>(bus, address);
    std::unique_ptr<VoltageRegulator> ret;
    switch (VoltageRegulator::stringToEnum(vrType))
    {
        case VRType::XDPE1X2XX:
            ret = XDPE152XX::create(std::move(inf));
            break;
        default:
            return NULL;
    }
    return ret;
}

enum VRType VoltageRegulator::stringToEnum(std::string& vrType)
{
    std::map<std::string, enum VRType> VRTypeToString{
        {"XDPE152XX", VRType::XDPE1X2XX},
    };
    return VRTypeToString[vrType];
}

} // End of Namespace VR
