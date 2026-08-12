#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace phosphor::software::VR
{

class TPS544X27CsvParser
{
  public:
    enum class CsvOp
    {
        WriteByte,
        ReadByte,
        WriteWord,
        ReadWord,
        BlockWrite,
        BlockRead,
        BlockProcessCall,
        SendByte,
        Pause,
        Reset,
    };

    struct CsvCommand
    {
        CsvOp op = CsvOp::Reset;
        size_t lineNo = 0;

        uint8_t cmd = 0;

        // Write data without block length or PEC.
        std::vector<uint8_t> dataBytes;

        // Expected raw bytes from the CSV.
        std::vector<uint8_t> expectedBytes;

        bool hasWordValue = false;
        uint16_t wordValue = 0;

        bool hasCsvPEC = false;
        uint8_t csvPEC = 0;

        uint32_t pauseMs = 0;
    };

    struct CsvImage
    {
        bool pecEnabled = false;
        bool wordLowByteFirst = false;
        bool includeBlockLength = false;

        std::vector<CsvCommand> commands;
    };

    bool parseImage(const uint8_t* image, size_t imageSize,
                    std::string& errorMsg);

    const CsvImage& getParsedImage() const
    {
        return parsedImage;
    }

    static const char* csvOpToString(CsvOp op);

  private:
    struct CsvOpEntry
    {
        std::string_view name;
        CsvOp op;
    };

    static constexpr std::array<CsvOpEntry, 10> csvOpTable{{
        {"WriteByte", CsvOp::WriteByte},
        {"ReadByte", CsvOp::ReadByte},
        {"WriteWord", CsvOp::WriteWord},
        {"ReadWord", CsvOp::ReadWord},
        {"BlockWrite", CsvOp::BlockWrite},
        {"BlockRead", CsvOp::BlockRead},
        {"BlockProcessCall", CsvOp::BlockProcessCall},
        {"SendByte", CsvOp::SendByte},
        {"Pause", CsvOp::Pause},
        {"Reset", CsvOp::Reset},
    }};

    static bool parseCsvOp(const std::string& text, CsvOp& op);

    static bool parseCsvCommand(const std::vector<std::string>& fields,
                                size_t lineNo, const std::string& line,
                                CsvCommand& cmd, std::string& errorMsg);

    bool parseCsvLines(const std::string& csvText, std::string& errorMsg);

    CsvImage parsedImage;
};

} // namespace phosphor::software::VR
