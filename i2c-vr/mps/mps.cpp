#include "mps.hpp"

namespace phosphor::software::VR
{

bool MPSImageParser::isValidDataTokens(
    const std::vector<std::string_view>& tokens)
{
    return tokens.size() > static_cast<size_t>(ATE::regName) &&
           !tokens[0].starts_with('*');
}

MPSData MPSImageParser::extractType0Data(
    const std::vector<std::string_view>& tokens)
{
    MPSData data;
    static constexpr size_t type0TokensSize = 7;

    if (tokens.size() != type0TokensSize)
    {
        lg2::error("Invalid token count for Type0 image line: "
                   "expected {EXPECTED}, got {ACTUAL}",
                   "EXPECTED", type0TokensSize, "ACTUAL", tokens.size());
        return data;
    }

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

MPSData MPSImageParser::extractType1Data(
    const std::vector<std::string_view>& tokens)
{
    MPSData data;
    static constexpr size_t type0TokensSize = 8;

    if (tokens.size() != type0TokensSize)
    {
        lg2::error("Invalid token count for Type1 image line: "
                   "expected {EXPECTED}, got {ACTUAL}",
                   "EXPECTED", type0TokensSize, "ACTUAL", tokens.size());
        return data;
    }

    data.page = getVal<uint8_t>(tokens, ATE::pageNum);
    auto addr = getVal<uint16_t>(tokens, ATE::regAddrHex);
    auto cmdType = getVal<std::string>(tokens, ATE::writeType);
    int blockDataBytes = 0;

    if (cmdType.starts_with("P"))
    {
        // Check if these tokens represent a P1 or P2 process call.
        // The upper byte of 'addr' is the command code, and the lower byte
        // is the LSB of the data.
        // Example:
        // addr = 0x0F11 and data = 0x18, this sends 0x1811 to command 0x0F
        static constexpr uint16_t processCallAddrMask = 0xFF00;
        data.data[0] = static_cast<uint8_t>(addr & ~processCallAddrMask);
        data.data[1] = getVal<uint8_t>(tokens, ATE::regDataHex);
        data.addr = static_cast<uint8_t>((addr & processCallAddrMask) >> 8);
        data.length = 2;
        return data;
    }
    else if (cmdType.starts_with("B"))
    {
        // Command types starting with 'B' indicate block r/w commands.
        // The number following 'B' specifies the number of data bytes.
        if (cmdType.size() > 1 && std::isdigit(cmdType[1]))
        {
            blockDataBytes = std::stoi(cmdType.substr(1));
        }
    }

    std::string regData = getVal<std::string>(tokens, ATE::regDataHex);
    size_t byteCount = std::min(regData.length() / 2, size_t(4));
    size_t dataIndex = 0;

    if (blockDataBytes > 0)
    {
        data.data[dataIndex++] = static_cast<uint8_t>(blockDataBytes);
    }

    for (size_t i = 0; i < byteCount; ++i)
    {
        data.data[dataIndex + (byteCount - 1 - i)] = static_cast<uint8_t>(
            std::stoul(regData.substr(i * 2, 2), nullptr, 16));
    }
    data.length = static_cast<uint8_t>(dataIndex + byteCount);
    data.addr = getVal<uint8_t>(tokens, ATE::regAddrHex);
    return data;
}

std::vector<MPSData> MPSImageParser::parse(
    const uint8_t* image, size_t imageSize, MPSImageType imageType)
{
    lineTokens = TokenizedLines(image, imageSize);
    std::vector<MPSData> results;

    using ExtractDataFunc =
        std::function<MPSData(const std::vector<std::string_view>&)>;
    ExtractDataFunc extractDataFunc;

    switch (imageType)
    {
        case MPSImageType::type0:
            extractDataFunc = [this](const auto& tokens) {
                return extractType0Data(tokens);
            };
            break;
        case MPSImageType::type1:
            extractDataFunc = [this](const auto& tokens) {
                return extractType1Data(tokens);
            };
            break;
        default:
            lg2::error("Unsupported or unknown MPS image type: {TYPE}", "TYPE",
                       static_cast<int>(imageType));
            return results;
    }

    for (const auto& tokens : lineTokens)
    {
        if (tokens[0].starts_with("END"))
        {
            break;
        }

        if (isValidDataTokens(tokens))
        {
            auto data = extractDataFunc(tokens);
            if (data.length == 0)
            {
                return {};
            }
            results.push_back(data);
        }
    }

    return results;
}

sdbusplus::async::task<bool> MPSVoltageRegulator::parseImage(
    const uint8_t* image, size_t imageSize, MPSImageType imageType)
{
    configuration = std::make_unique<MPSConfig>();

    try
    {
        configuration->registersData =
            parser->parse(image, imageSize, imageType);

        if (!co_await parseDeviceConfiguration())
        {
            co_return false;
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to parse MPS image: {ERR}", "ERR", e.what());
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
