#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace phosphor::software::VR
{

// Parser for the TI Fusion Digital Power Designer "PMBus Programmer Script"
// CSV exported for the TPS544B28.
//
// The first line of the script declares the export options, for example:
//
//   Comment,Format=CSV; Hex=CoderUpper; BreakOutBytes=False; PEC=False;
//   SMBusWordCommandBytesOrderAsLowByteFirst=True; IncludeBlockLength=True;
//   HasAddress=False [DO NOT REMOVE THIS LINE ...]
//
// Data fields are kept as the raw byte sequence that goes on the wire after
// the command code, so no endianness conversion is needed either when
// writing or when comparing a read back against its expected value.
class TPS544B28CsvParser
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
        SendByte,
        Pause,
        Reset,
    };

    struct CsvCommand
    {
        CsvOp op = CsvOp::Reset;
        size_t lineNo = 0;

        uint8_t cmd = 0;

        // Wire-order bytes following the command code. For a block command
        // this includes the leading length byte when the script was exported
        // with IncludeBlockLength=True.
        std::vector<uint8_t> dataBytes;

        // For read operations, the bytes the device is expected to return.
        std::vector<uint8_t> expectedBytes;

        uint32_t pauseMs = 0;
    };

    struct CsvImage
    {
        // Values taken from the header comment line.
        bool pecEnabled = false;
        bool includeBlockLength = true;
        bool wordLowByteFirst = true;
        bool hasAddress = false;
        bool headerSeen = false;

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
    static bool parseCsvOp(const std::string& text, CsvOp& op);

    bool parseHeader(const std::string& line);
    bool parseCommand(const std::vector<std::string>& fields, size_t lineNo,
                      std::string& errorMsg);

    CsvImage parsedImage;
};

} // namespace phosphor::software::VR
