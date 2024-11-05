#include "vr.hpp"
#include "xdpe152xx.hpp"

namespace VR
{

std::unique_ptr<VoltageRegulator> create(std::string& vrType, uint8_t bus, uint8_t address)
{
    std::unique_ptr<i2c::I2CInterface> inf = i2c::create(bus, address, i2c::I2CInterface::InitialState::OPEN, 0);
    std::unique_ptr<VoltageRegulator> ret;
    switch (VoltageRegulator::stringToEnum(vrType))
    {
        case VRType::XDPE152XX:
            ret = XDPE152XX::create(std::move(inf));
            break;
        case VRType::ISL69260:
            break;
        default:
            return NULL;
    }
    return ret;
}

std::map<std::string, enum VRType> VoltageRegulator::VRTypeToString {
			{ "XDPE152XX", VRType::XDPE152XX },
            { "XDPE192XX", VRType::XDPE152XX },
            { "XDPE1A2XX", VRType::XDPE152XX},
			{ "ISL69260", VRType::ISL69260 },
};

enum VRType VoltageRegulator::stringToEnum(std::string& vrType)
{
	return VRTypeToString[vrType];
}

} // End of Namespace VR
