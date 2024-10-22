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
            ret = std::make_unique<XDPE152XX>(std::move(inf));
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

uint32_t VoltageRegulator::calcCRC32(const uint32_t* data, int len, uint32_t poly)
{
    uint32_t crc = 0xFFFFFFFF;
    int i;
    int b;

    if (data == NULL) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        crc ^= data[i];

        for (b = 0; b < 32; b++) {
            if (crc & 0x1) {
                crc = (crc >> 1) ^  poly;  // lsb-first
            }
            else
            {
                crc >>= 1;
            }
        }
    }

  return ~crc;
}
