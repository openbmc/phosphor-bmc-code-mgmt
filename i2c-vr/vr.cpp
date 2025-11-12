#include "vr.hpp"

#include "isl69269/isl69269.hpp"
#include "mps/mp292x.hpp"
#include "mps/mp297x.hpp"
#include "mps/mp2x6xx.hpp"
#include "mps/mp5998.hpp"
#include "mps/mp994x.hpp"
#include "tda38640a/tda38640a.hpp"
#include "xdpe1x2xx/xdpe1x2xx.hpp"

#include <map>

namespace phosphor::software::VR
{

std::unique_ptr<VoltageRegulator> create(sdbusplus::async::context& ctx,
                                         enum VRType vrType, uint16_t bus,
                                         uint16_t address)
{
    switch (vrType)
    {
        case VRType::XDPE1X2XX:
            return std::make_unique<XDPE1X2XX>(ctx, bus, address);
        case VRType::ISL69269:
            return std::make_unique<ISL69269>(ctx, bus, address);
        case VRType::MP2X6XX:
            return std::make_unique<MP2X6XX>(ctx, bus, address);
        case VRType::MP292X:
            return std::make_unique<MP292X>(ctx, bus, address);
        case VRType::MP297X:
            return std::make_unique<MP297X>(ctx, bus, address);
        case VRType::MP5998:
            return std::make_unique<MP5998>(ctx, bus, address);
        case VRType::MP994X:
            return std::make_unique<MP994X>(ctx, bus, address);
        case VRType::RAA22XGen2:
            return std::make_unique<ISL69269>(ctx, bus, address,
                                              ISL69269::Gen::Gen2);
        case VRType::RAA22XGen3p5:
            return std::make_unique<ISL69269>(ctx, bus, address,
                                              ISL69269::Gen::Gen3p5);
        case VRType::TDA38640A:
            return std::make_unique<TDA38640A>(ctx, bus, address);
        default:
            return nullptr;
    }
}

bool stringToEnum(std::string& vrStr, VRType& vrType)
{
    std::map<std::string, enum VRType> VRTypeToString{
        {"XDPE1X2XXFirmware", VRType::XDPE1X2XX},
        {"ISL69269Firmware", VRType::ISL69269},
        {"MP2X6XXFirmware", VRType::MP2X6XX},
        {"MP292XFirmware", VRType::MP292X},
        {"MP297XFirmware", VRType::MP297X},
        {"MP5998Firmware", VRType::MP5998},
        {"MP994XFirmware", VRType::MP994X},
        {"RAA22XGen2Firmware", VRType::RAA22XGen2},
        {"RAA22XGen3p5Firmware", VRType::RAA22XGen3p5},
        {"TDA38640AFirmware", VRType::TDA38640A}};

    if (VRTypeToString.contains(vrStr))
    {
        vrType = VRTypeToString[vrStr];
        return true;
    }
    return false;
}

} // namespace phosphor::software::VR
