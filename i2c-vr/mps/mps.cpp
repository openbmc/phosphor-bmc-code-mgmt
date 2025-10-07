#include "mps.hpp"

namespace phosphor::software::VR
{

bool MPSImageParser::isValidDataTokens(
    const std::vector<std::string_view>& tokens)
{
    return tokens.size() > static_cast<size_t>(ATE::regName) &&
           !tokens[0].starts_with('*');
}

MPSData MPSImageParser::extractData(const std::vector<std::string_view>& tokens)
{
    MPSData data;
    data.page = getVal<uint8_t>(tokens, ATE::pageNum);
    data.addr = getVal<uint8_t>(tokens, ATE::regAddrHex);

    std::string regData = getVal<std::string>(tokens, ATE::regDataHex);
    size_t byteCount = std::min(regData.length() / 2, size_t(4));
    for (size_t i = 0; i < byteCount; ++i)
    {
        data.data[byteCount - 1 - i] = static_cast<uint8_t>(
            std::stoul(regData.substr(i * 2, 2), nullptr, 16));
    }

    data.length = static_cast<uint8_t>(byteCount);
    return data;
}

std::vector<MPSData> MPSImageParser::getRegistersData()
{
    std::vector<MPSData> registersData;
    for (const auto& tokens : lineTokens)
    {
        if (tokens[0].starts_with("END"))
        {
            break;
        }

        if (isValidDataTokens(tokens))
        {
            registersData.push_back(extractData(tokens));
        }
    }
    return registersData;
}

sdbusplus::async::task<bool> MPSVoltageRegulator::parseImage(
    const uint8_t* image, size_t imageSize,
    std::unique_ptr<MPSImageParser> customParser)
{
    if (customParser)
    {
        parser = std::move(customParser);
    }
    else
    {
        parser = std::make_unique<MPSImageParser>(image, imageSize);
    }

    configuration = std::make_unique<MPSConfig>();
    configuration->registersData = parser->getRegistersData();

    if (!co_await parseDeviceConfiguration())
    {
        co_return false;
    }

    lg2::debug(
        "Parsed configuration: Data Size={SIZE}, Vendor ID={VID}, "
        "Product ID={PID}, Config ID={CID}, CRC User={CRCUSR}, "
        "CRC Multi={CRCMULTI}",
        "SIZE", configuration->registersData.size(), "VID", lg2::hex,
        configuration->vendorId, "PID", lg2::hex, configuration->productId,
        "CID", lg2::hex, configuration->configId, "CRCUSR", lg2::hex,
        configuration->crcUser, "CRCMULTI", lg2::hex, configuration->crcMulti);

    co_return true;
}

std::map<uint8_t, std::vector<MPSData>>
    MPSVoltageRegulator::getGroupedConfigData(uint8_t configMask, uint8_t shift)
{
    std::map<uint8_t, std::vector<MPSData>> groupedData;

    if (!configuration)
    {
        return groupedData;
    }

    for (const auto& data : configuration->registersData)
    {
        uint8_t config = (data.page & configMask) >> shift;
        groupedData[config].push_back(data);
    }

    return groupedData;
}

} // namespace phosphor::software::VR
