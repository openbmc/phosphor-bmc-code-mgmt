#include "vr.hpp"

#include "isl69269/isl69269.hpp"
#include "xdpe1x2xx/xdpe1x2xx.hpp"

#include <map>

namespace phosphor::software::VR
{

std::unique_ptr<VoltageRegulator> create(sdbusplus::async::context& ctx,
                                         enum VRType vrType, uint16_t bus,
                                         uint16_t address)
{
    std::unique_ptr<VoltageRegulator> ret;
    switch (vrType)
    {
        case VRType::XDPE1X2XX:
            ret = std::make_unique<XDPE1X2XX>(ctx, bus, address);
            break;
        case VRType::ISL69269:
            ret = std::make_unique<ISL69269>(ctx, bus, address);
            break;
        case VRType::RAA22XGen2:
            ret = std::make_unique<ISL69269>(ctx, bus, address,
                                             ISL69269::Gen::Gen2);
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
        {"ISL69269Firmware", VRType::ISL69269},
        {"RAA22XGen2Firmware", VRType::RAA22XGen2},
    };

    if (VRTypeToString.contains(vrStr))
    {
        vrType = VRTypeToString[vrStr];
        return true;
    }
    return false;
}

} // namespace phosphor::software::VR
