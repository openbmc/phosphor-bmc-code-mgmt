#include "vr.hpp"

#include "dummyDevice/dummyDevice.hpp"
#include "xdpe1x2xx/xdpe1x2xx.hpp"

#include <map>

namespace phosphor::software::VR
{

std::unique_ptr<VoltageRegulator> create(SDBusAsync::context& ctx,
                                         enum VRType vrType, uint16_t bus,
                                         uint16_t address)
{
    std::unique_ptr<VoltageRegulator> ret;
    switch (vrType)
    {
        case VRType::XDPE1X2XX:
            ret = std::make_unique<XDPE1X2XX>(ctx, bus, address);
            break;
        case VRType::DummyVR:
            ret = std::make_unique<DummyDevice>(ctx);
            break;
        default:
            return NULL;
    }
    return ret;
}

bool stringToEnum(std::string& vrStr, VRType& vrType)
{
    std::map<std::string, enum VRType> VRTypeToString{
        {"DummyDeviceFirmware", VRType::DummyVR},
        {"XDPE1X2XXFirmware", VRType::XDPE1X2XX},
    };

    if (VRTypeToString.contains(vrStr))
    {
        vrType = VRTypeToString[vrStr];
        return true;
    }
    return false;
}

} // namespace phosphor::software::VR
