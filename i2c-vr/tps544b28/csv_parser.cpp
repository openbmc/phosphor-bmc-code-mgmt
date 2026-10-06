#include "csv_parser.hpp"

#include "common/include/utils.hpp"

#include <array>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace phosphor::software::VR
{

namespace
{

struct CsvOpEntry
{
    std::string_view name;
    TPS544B28CsvParser::CsvOp op;
};

constexpr std::array<CsvOpEntry, 9> csvOpTable{{
    {"WriteByte", TPS544B28CsvParser::CsvOp::WriteByte},
    {"ReadByte", TPS544B28CsvParser::CsvOp::ReadByte},
    {"WriteWord", TPS544B28CsvParser::CsvOp::WriteWord},
    {"ReadWord", TPS544B28CsvParser::CsvOp::ReadWord},
    {"BlockWrite", TPS544B28CsvParser::CsvOp::BlockWrite},
    {"BlockRead", TPS544B28CsvParser::CsvOp::BlockRead},
    {"SendByte", TPS544B28CsvParser::CsvOp::SendByte},
    {"Pause", TPS544B28CsvParser::CsvOp::Pause},
    {"Reset", TPS544B28CsvParser::CsvOp::Reset},
}};

bool headerFlag(const std::string& line, std::string_view key, bool& value)
{
    const auto pos = line.find(key);
    if (pos == std::string::npos)
    {
        return false;
    }
    const auto rest = line.substr(pos + key.size());
    value = rest.compare(0, 4, "True") == 0;
    return true;
}

} // namespace

const char* TPS544B28CsvParser::csvOpToString(CsvOp op)
{
    for (const auto& entry : csvOpTable)
    {
        if (entry.op == op)
        {
            return entry.name.data();
        }
    }
    return "Unknown";
}

bool TPS544B28CsvParser::parseCsvOp(const std::string& text, CsvOp& op)
{
    for (const auto& entry : csvOpTable)
    {
        if (entry.name == text)
        {
            op = entry.op;
            return true;
        }
    }
    return false;
}

bool TPS544B28CsvParser::parseHeader(const std::string& line)
{
    if (line.find("Format=CSV") == std::string::npos)
    {
        return false;
    }

    headerFlag(line, "PEC=", parsedImage.pecEnabled);
    headerFlag(line, "IncludeBlockLength=", parsedImage.includeBlockLength);
    headerFlag(line, "SMBusWordCommandBytesOrderAsLowByteFirst=",
               parsedImage.wordLowByteFirst);
    headerFlag(line, "HasAddress=", parsedImage.hasAddress);
    parsedImage.headerSeen = true;

    return true;
}

bool TPS544B28CsvParser::parseCommand(const std::vector<std::string>& fields,
                                      size_t lineNo, std::string& errorMsg)
{
    auto fail = [&](const std::string& reason) {
        errorMsg = "line " + std::to_string(lineNo) + ": " + reason;
        return false;
    };

    CsvOp op{};
    if (!parseCsvOp(fields[0], op))
    {
        return fail("unknown operation '" + fields[0] + "'");
    }

    CsvCommand cmd;
    cmd.op = op;
    cmd.lineNo = lineNo;

    switch (op)
    {
        case CsvOp::Reset:
            // "Reset,," carries no data.
            break;

        case CsvOp::Pause:
        {
            if (fields.size() < 2 || fields[1].empty())
            {
                return fail("Pause needs a duration in milliseconds");
            }
            try
            {
                cmd.pauseMs = static_cast<uint32_t>(std::stoul(fields[1]));
            }
            catch (const std::exception&)
            {
                return fail("Pause duration '" + fields[1] + "' is not a "
                            "number");
            }
            break;
        }

        case CsvOp::SendByte:
            if (fields.size() < 2 || !parseHexByte(fields[1], cmd.cmd))
            {
                return fail("SendByte needs a command code");
            }
            break;

        case CsvOp::WriteByte:
        case CsvOp::WriteWord:
        case CsvOp::BlockWrite:
        {
            if (fields.size() < 3 || !parseHexByte(fields[1], cmd.cmd))
            {
                return fail("write needs a command code");
            }
            if (!parseHexBytes(fields[2], cmd.dataBytes))
            {
                return fail("write data '" + fields[2] + "' is not hex");
            }
            if (op == CsvOp::WriteByte && cmd.dataBytes.size() != 1)
            {
                return fail("WriteByte data must be exactly one byte");
            }
            if (op == CsvOp::WriteWord && cmd.dataBytes.size() != 2)
            {
                return fail("WriteWord data must be exactly two bytes");
            }
            if (op == CsvOp::BlockWrite)
            {
                // Same limit as TPS544x27 executeBlockWrite(): an SMBus block
                // carries at most 255 data bytes. With IncludeBlockLength the
                // first byte is the length itself and is not counted.
                const size_t payload = parsedImage.includeBlockLength
                                           ? cmd.dataBytes.size() - 1
                                           : cmd.dataBytes.size();
                if (payload > 255)
                {
                    return fail("BlockWrite data must not exceed 255 bytes");
                }
            }
            break;
        }

        case CsvOp::ReadByte:
        case CsvOp::ReadWord:
        case CsvOp::BlockRead:
        {
            if (fields.size() < 3 || !parseHexByte(fields[1], cmd.cmd))
            {
                return fail("read needs a command code");
            }
            if (!parseHexBytes(fields[2], cmd.expectedBytes))
            {
                return fail("expected data '" + fields[2] + "' is not hex");
            }
            if (op == CsvOp::ReadByte && cmd.expectedBytes.size() != 1)
            {
                return fail("ReadByte expected value must be one byte");
            }
            if (op == CsvOp::ReadWord && cmd.expectedBytes.size() != 2)
            {
                return fail("ReadWord expected value must be two bytes");
            }
            break;
        }
    }

    parsedImage.commands.push_back(std::move(cmd));
    return true;
}

bool TPS544B28CsvParser::parseImage(const uint8_t* image, size_t imageSize,
                                    std::string& errorMsg)
{
    if (image == nullptr || imageSize == 0)
    {
        errorMsg = "empty image";
        return false;
    }

    parsedImage = CsvImage{};

    std::istringstream stream(
        std::string(reinterpret_cast<const char*>(image), imageSize));

    std::string line;
    size_t lineNo = 0;

    while (std::getline(stream, line))
    {
        lineNo++;

        const auto fields = ::splitCsvLine(line);
        if (fields.empty() || fields[0].empty())
        {
            continue;
        }

        if (fields[0] == "Comment")
        {
            if (!parsedImage.headerSeen)
            {
                parseHeader(line);
            }
            continue;
        }

        if (!parseCommand(fields, lineNo, errorMsg))
        {
            return false;
        }
    }

    if (!parsedImage.headerSeen)
    {
        errorMsg = "missing the Fusion header comment line";
        return false;
    }

    if (parsedImage.commands.empty())
    {
        errorMsg = "no PMBus commands found";
        return false;
    }

    return true;
}

} // namespace phosphor::software::VR
