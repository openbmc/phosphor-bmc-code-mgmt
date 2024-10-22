#include "vr.hpp"

#include "xdpe1x2xx/xdpe1x2xx.hpp"

#include <map>


std::unique_ptr<VoltageRegulator> create(enum VRType vrType, uint8_t bus,
                                         uint8_t address)
{
    std::unique_ptr<phosphor::i2c::I2C> inf = std::make_unique<phosphor::i2c::I2C>(bus, address);
    std::unique_ptr<VoltageRegulator> ret;
    switch (vrType)
    {
        case VRType::XDPE1X2XX:
            ret = XDPE152XX::create(std::move(inf));
            break;
        default:
            return NULL;
    }
    return ret;
}

enum VRType stringToEnum(std::string& vrType)
{
    std::map<std::string, enum VRType> VRTypeToString{
        {"XDPE1X2XX", VRType::XDPE1X2XX},
    };
    return VRTypeToString[vrType];
}

