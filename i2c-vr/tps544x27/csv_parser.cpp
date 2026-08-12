#include "csv_parser.hpp"

#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <exception>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{
const char* TPS544X27CsvParser::csvOpToString(CsvOp op)
{
    const auto it =
        std::find_if(csvOpTable.begin(), csvOpTable.end(),
                     [op](const CsvOpEntry& entry) { return entry.op == op; });

    if (it == csvOpTable.end())
    {
        return "Unknown";
    }

    return it->name.data();
}

bool TPS544X27CsvParser::parseCsvOp(const std::string& text, CsvOp& op)
{
    const auto it = std::find_if(
        csvOpTable.begin(), csvOpTable.end(),
        [&text](const CsvOpEntry& entry) { return entry.name == text; });

    if (it == csvOpTable.end())
    {
        return false;
    }

    op = it->op;
    return true;
}

bool TPS544X27CsvParser::parseCsvCommand(const std::vector<std::string>& fields,
                                         size_t lineNo, const std::string& line,
                                         CsvCommand& cmd, std::string& errorMsg)
{
    auto fail = [&](const std::string& msg) {
        errorMsg = "CSV line ";
        errorMsg += std::to_string(lineNo);
        errorMsg += ": ";
        errorMsg += msg;
        errorMsg += " | ";
        errorMsg += line;
        error("TPS544x27 CSV parse failed: {ERR}", "ERR", errorMsg);
        return false;
    };
    CsvOp op = CsvOp::Reset;
    if (!parseCsvOp(fields[0], op))
    {
        return fail("unknown operation");
    }

    cmd.op = op;
    cmd.lineNo = lineNo;

    auto parseCmdField = [&]() -> bool {
        if (fields.size() < 2)
        {
            return false;
        }

        return parseHexByte(fields[1], cmd.cmd);
    };

    switch (op)
    {
        case CsvOp::WriteByte:
        {
            if (!parseCmdField() || fields.size() < 3)
            {
                return fail("invalid WriteByte");
            }

            std::vector<uint8_t> raw;
            if (!parseHexBytes(fields[2], raw) || raw.size() != 2)
            {
                return fail("WriteByte data must be data + PEC");
            }

            cmd.dataBytes = {raw[0]};
            cmd.hasCsvPEC = true;
            cmd.csvPEC = raw[1];
            break;
        }

        case CsvOp::ReadByte:
        {
            if (!parseCmdField() || fields.size() < 3)
            {
                return fail("invalid ReadByte");
            }

            if (!parseHexBytes(fields[2], cmd.expectedBytes) ||
                cmd.expectedBytes.size() != 1)
            {
                return fail("ReadByte expected must be 1 byte");
            }
            break;
        }

        case CsvOp::WriteWord:
        {
            if (!parseCmdField() || fields.size() < 3)
            {
                return fail("invalid WriteWord");
            }

            std::vector<uint8_t> raw;
            if (!parseHexBytes(fields[2], raw) || raw.size() != 3)
            {
                return fail("WriteWord data must be low + high + PEC");
            }

            const uint8_t lo = raw[0];
            const uint8_t hi = raw[1];

            cmd.dataBytes = {lo, hi};
            cmd.hasWordValue = true;
            cmd.wordValue = static_cast<uint16_t>(lo) |
                            (static_cast<uint16_t>(hi) << 8);

            cmd.hasCsvPEC = true;
            cmd.csvPEC = raw[2];
            break;
        }

        case CsvOp::ReadWord:
        {
            if (!parseCmdField() || fields.size() < 3)
            {
                return fail("invalid ReadWord");
            }

            if (!parseHexBytes(fields[2], cmd.expectedBytes) ||
                cmd.expectedBytes.size() != 2)
            {
                return fail("ReadWord expected must be 2 bytes");
            }
            break;
        }

        case CsvOp::BlockWrite:
        {
            if (!parseCmdField() || fields.size() < 3)
            {
                return fail("invalid BlockWrite");
            }

            std::vector<uint8_t> raw;
            if (!parseHexBytes(fields[2], raw) || raw.size() < 3)
            {
                return fail("BlockWrite data must be length + data + PEC");
            }

            const uint8_t blockLen = raw[0];
            const size_t dataLen = raw.size() - 2; // remove length and PEC

            if (dataLen != blockLen)
            {
                return fail("BlockWrite length mismatch");
            }

            // Keep only data bytes here, no length and no CSV PEC.
            cmd.dataBytes.assign(raw.begin() + 1, raw.end() - 1);

            cmd.hasCsvPEC = true;
            cmd.csvPEC = raw.back();
            break;
        }

        case CsvOp::BlockRead:
        {
            if (!parseCmdField() || fields.size() < 3)
            {
                return fail("invalid BlockRead");
            }

            if (!parseHexBytes(fields[2], cmd.expectedBytes) ||
                cmd.expectedBytes.empty())
            {
                return fail("BlockRead expected empty");
            }

            const uint8_t blockLen = cmd.expectedBytes[0];
            const size_t dataLen = cmd.expectedBytes.size() - 1;

            if (dataLen != blockLen)
            {
                return fail("BlockRead expected length mismatch");
            }

            break;
        }

        case CsvOp::BlockProcessCall:
        {
            if (fields.size() < 3)
            {
                return fail("invalid BlockProcessCall");
            }

            std::vector<uint8_t> request;
            if (!parseHexBytes(fields[1], request) || request.size() < 2)
            {
                return fail("BlockProcessCall request must be cmd + data");
            }

            cmd.cmd = request[0];
            cmd.dataBytes.assign(request.begin() + 1, request.end());

            if (!parseHexBytes(fields[2], cmd.expectedBytes) ||
                cmd.expectedBytes.empty())
            {
                return fail("BlockProcessCall expected empty");
            }

            const uint8_t blockLen = cmd.expectedBytes[0];
            const size_t dataLen = cmd.expectedBytes.size() - 1;

            if (dataLen != blockLen)
            {
                return fail("BlockProcessCall expected length mismatch");
            }

            break;
        }

        case CsvOp::SendByte:
        {
            if (!parseCmdField())
            {
                return fail("invalid SendByte");
            }
            break;
        }

        case CsvOp::Pause:
        {
            if (fields.size() < 2)
            {
                return fail("invalid Pause");
            }

            try
            {
                cmd.pauseMs = static_cast<uint32_t>(std::stoul(fields[1]));
            }
            catch (const std::exception&)
            {
                return fail("invalid Pause value");
            }
            break;
        }

        case CsvOp::Reset:
        {
            break;
        }
    }
    return true;
}

bool TPS544X27CsvParser::parseCsvLines(const std::string& csvText,
                                       std::string& errorMsg)
{
    std::istringstream iss(csvText);
    std::string line;
    size_t lineNo = 0;

    while (std::getline(iss, line))
    {
        ++lineNo;

        line = trim(line);
        if (line.empty())
        {
            continue;
        }

        auto fields = splitCsvLine(line);
        if (fields.empty())
        {
            continue;
        }

        if (fields[0] == "Comment")
        {
            if (line.find("PEC=True") != std::string::npos)
            {
                parsedImage.pecEnabled = true;
            }

            if (line.find("SMBusWordCommandBytesOrderAsLowByteFirst=True") !=
                std::string::npos)
            {
                parsedImage.wordLowByteFirst = true;
            }

            if (line.find("IncludeBlockLength=True") != std::string::npos)
            {
                parsedImage.includeBlockLength = true;
            }

            continue;
        }

        CsvCommand cmd;

        if (!parseCsvCommand(fields, lineNo, line, cmd, errorMsg))
        {
            return false;
        }

        parsedImage.commands.push_back(std::move(cmd));
    }
    return true;
}

bool TPS544X27CsvParser::parseImage(const uint8_t* image, size_t imageSize,
                                    std::string& errorMsg)
{
    parsedImage = {};

    if (image == nullptr || imageSize == 0)
    {
        errorMsg = "empty image";
        return false;
    }

    std::string csvText(reinterpret_cast<const char*>(image), imageSize);
    return parseCsvLines(csvText, errorMsg);
}

} // namespace phosphor::software::VR
