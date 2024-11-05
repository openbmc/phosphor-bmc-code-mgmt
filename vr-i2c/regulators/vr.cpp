#include "vr.hpp"

#include "xdpe152xx.hpp"

namespace VR
{

std::unique_ptr<VoltageRegulator> create(std::string& vrType, uint8_t bus,
                                         uint8_t address)
{
    std::unique_ptr<i2c::I2C> inf = i2c::create(bus, address);
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

enum VRType VoltageRegulator::stringToEnum(std::string& vrType)
{
    std::map<std::string, enum VRType> VRTypeToString{
        {"XDPE152XX", VRType::XDPE152XX},
        {"XDPE192XX", VRType::XDPE152XX},
        {"XDPE1A2XX", VRType::XDPE152XX},
        {"ISL69260", VRType::ISL69260},
    };
    return VRTypeToString[vrType];
}

int VoltageRegulator::vr_fw_update(const uint8_t* image, size_t image_size,
                                   bool force)
{
    this->fw_validate_file(image, image_size);
    this->fw_parse_file(image, image_size);
    this->fw_update(force);
    this->fw_verify();
    return 0;
}

} // End of Namespace VR
