#include "vr.hpp"

#include "xdpe1x2xx/xdpe1x2xx.hpp"

#include <map>

namespace phosphor::software::VR
{

std::unique_ptr<VoltageRegulator> create(enum VRType vrType, uint8_t bus,
                                         uint8_t address)
{
    std::unique_ptr<phosphor::i2c::I2C> inf = std::make_unique<phosphor::i2c::I2C>(bus, address);
    std::unique_ptr<VoltageRegulator> ret;
    switch (vrType)
    {
        case VRType::XDPE1X2XX:
            ret = std::make_unique<XDPE1X2XX>(std::move(inf));
            break;
        default:
            return NULL;
    }
    return ret;
}

bool stringToEnum(VRType& type, std::string& vrType)
{
    std::map<std::string, enum VRType> VRTypeToString{
        {"XDPE152XXFirmware", VRType::XDPE1X2XX},
        {"XDPE192XXFirmware", VRType::XDPE1X2XX},
    };

    if (VRTypeToString.contains(vrType))
    {
        type = VRTypeToString[vrType];
        return true;
    }
    return false;
}

}
