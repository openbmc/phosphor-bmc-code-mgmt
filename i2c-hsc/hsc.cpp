#include "hsc.hpp"

#include "tps25990/rs31390.hpp"
#include "tps25990/tps25990.hpp"

#include <map>

namespace phosphor::software::HSC
{

std::unique_ptr<HotSwapController> create(sdbusplus::async::context& ctx,
                                           enum HSCType hscType, uint16_t bus,
                                           uint16_t address)
{
    switch (hscType)
    {
        case HSCType::TPS25990:
            return std::make_unique<TPS25990>(ctx, bus, address);
        case HSCType::RS31390:
            return std::make_unique<RS31390>(ctx, bus, address);
        default:
            return nullptr;
    }
}

bool stringToEnum(std::string& hscStr, HSCType& hscType)
{
    std::map<std::string, enum HSCType> HSCTypeToString{
        {"TPS25990Firmware", HSCType::TPS25990},
        {"RS31390Firmware", HSCType::RS31390}};

    if (HSCTypeToString.contains(hscStr))
    {
        hscType = HSCTypeToString[hscStr];
        return true;
    }
    return false;
}

} // namespace phosphor::software::HSC
