#include "i2c-vr/tps544b28/csv_parser.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace phosphor::software::VR;

namespace
{

// The header line exactly as TI's Fusion Digital Power Designer writes it for
// the TPS544B28. Note the fields are separated by semicolons here, while the
// TPS544x27 script uses commas, so the parser has to cope with both.
constexpr const char* tiHeader =
    "Comment,Format=CSV; Hex=CoderUpper; BreakOutBytes=False; PEC=False; "
    "SMBusWordCommandBytesOrderAsLowByteFirst=True; IncludeBlockLength=True; "
    "HasAddress=False [DO NOT REMOVE THIS LINE IF YOU WANT TO IMPORT USING A "
    "FUSION TOOL],\n";

bool parse(const std::string& csv, TPS544B28CsvParser& parser,
           std::string& errorMsg)
{
    return parser.parseImage(reinterpret_cast<const uint8_t*>(csv.data()),
                             csv.size(), errorMsg);
}

std::string withHeader(const std::string& body)
{
    return std::string(tiHeader) + body;
}

} // namespace

TEST(TPS544B28CsvParserTest, ParsesHeaderFlags)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parse(withHeader("SendByte,0x15,\n"), parser, errorMsg))
        << errorMsg;

    const auto& image = parser.getParsedImage();

    EXPECT_TRUE(image.headerSeen);
    EXPECT_FALSE(image.pecEnabled);
    EXPECT_TRUE(image.includeBlockLength);
    EXPECT_TRUE(image.wordLowByteFirst);
    EXPECT_FALSE(image.hasAddress);
}

TEST(TPS544B28CsvParserTest, AcceptsCommaSeparatedHeader)
{
    const std::string csv =
        "Comment,Format=CSV,Hex=CoderUpper,BreakOutBytes=False,PEC=True,"
        "SMBusWordCommandBytesOrderAsLowByteFirst=True,"
        "IncludeBlockLength=True,HasAddress=False\n"
        "SendByte,0x15,\n";

    TPS544B28CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parse(csv, parser, errorMsg)) << errorMsg;

    EXPECT_TRUE(parser.getParsedImage().pecEnabled);
}

TEST(TPS544B28CsvParserTest, ParsesWriteByte)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    // Write ON_OFF_CONFIG, taken from the TI script.
    ASSERT_TRUE(parse(withHeader("WriteByte,0x02,0x16\n"), parser, errorMsg))
        << errorMsg;

    const auto& commands = parser.getParsedImage().commands;
    ASSERT_EQ(commands.size(), 1u);

    EXPECT_EQ(commands[0].op, TPS544B28CsvParser::CsvOp::WriteByte);
    EXPECT_EQ(commands[0].cmd, 0x02);
    EXPECT_EQ(commands[0].dataBytes, (std::vector<uint8_t>{0x16}));
    EXPECT_TRUE(commands[0].expectedBytes.empty());
}

TEST(TPS544B28CsvParserTest, ParsesWriteWordKeepsWireOrder)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    // FREQUENCY_SWITCH 800 kHz. The script writes the low byte first, so the
    // bytes must be kept in the order they appear.
    ASSERT_TRUE(parse(withHeader("WriteWord,0x33,0x0638\n"), parser, errorMsg))
        << errorMsg;

    const auto& commands = parser.getParsedImage().commands;
    ASSERT_EQ(commands.size(), 1u);

    EXPECT_EQ(commands[0].op, TPS544B28CsvParser::CsvOp::WriteWord);
    EXPECT_EQ(commands[0].cmd, 0x33);
    EXPECT_EQ(commands[0].dataBytes, (std::vector<uint8_t>{0x06, 0x38}));
}

TEST(TPS544B28CsvParserTest, ParsesReadCommandsAsExpectedBytes)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parse(withHeader("ReadByte,0xD1,0x41\n"
                                 "ReadWord,0x46,0x0C00\n"),
                      parser, errorMsg))
        << errorMsg;

    const auto& commands = parser.getParsedImage().commands;
    ASSERT_EQ(commands.size(), 2u);

    EXPECT_EQ(commands[0].op, TPS544B28CsvParser::CsvOp::ReadByte);
    EXPECT_EQ(commands[0].cmd, 0xD1);
    EXPECT_EQ(commands[0].expectedBytes, (std::vector<uint8_t>{0x41}));
    EXPECT_TRUE(commands[0].dataBytes.empty());

    EXPECT_EQ(commands[1].op, TPS544B28CsvParser::CsvOp::ReadWord);
    EXPECT_EQ(commands[1].cmd, 0x46);
    EXPECT_EQ(commands[1].expectedBytes, (std::vector<uint8_t>{0x0C, 0x00}));
}

TEST(TPS544B28CsvParserTest, ParsesBlockCommandsIncludingLengthByte)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    // IncludeBlockLength=True, so the leading byte is the block length and is
    // deliberately kept, because it goes on the wire and comes back on a read.
    ASSERT_TRUE(parse(withHeader("BlockWrite,0x99,0x025449\n"
                                 "BlockRead,0xAD,0x065449544B2800\n"),
                      parser, errorMsg))
        << errorMsg;

    const auto& commands = parser.getParsedImage().commands;
    ASSERT_EQ(commands.size(), 2u);

    EXPECT_EQ(commands[0].op, TPS544B28CsvParser::CsvOp::BlockWrite);
    EXPECT_EQ(commands[0].cmd, 0x99);
    EXPECT_EQ(commands[0].dataBytes,
              (std::vector<uint8_t>{0x02, 0x54, 0x49}));

    EXPECT_EQ(commands[1].op, TPS544B28CsvParser::CsvOp::BlockRead);
    EXPECT_EQ(commands[1].cmd, 0xAD);
    EXPECT_EQ(commands[1].expectedBytes,
              (std::vector<uint8_t>{0x06, 0x54, 0x49, 0x54, 0x4B, 0x28,
                                    0x00}));
}

TEST(TPS544B28CsvParserTest, ParsesSendBytePauseAndReset)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parse(withHeader("SendByte,0x15,\n"
                                 "Pause,1000,hold time\n"
                                 "Reset,,\n"),
                      parser, errorMsg))
        << errorMsg;

    const auto& commands = parser.getParsedImage().commands;
    ASSERT_EQ(commands.size(), 3u);

    EXPECT_EQ(commands[0].op, TPS544B28CsvParser::CsvOp::SendByte);
    EXPECT_EQ(commands[0].cmd, 0x15);

    EXPECT_EQ(commands[1].op, TPS544B28CsvParser::CsvOp::Pause);
    EXPECT_EQ(commands[1].pauseMs, 1000u);

    EXPECT_EQ(commands[2].op, TPS544B28CsvParser::CsvOp::Reset);
}

TEST(TPS544B28CsvParserTest, HandlesCommaInsideQuotedComment)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    // The TI script writes the hold time as
    //   Pause,1000,"Pausing 1,000.00 ms for STORE_USER_ALL hold time"
    // so a naive comma split would read the duration as "1".
    ASSERT_TRUE(parse(withHeader("Pause,1000,\"Pausing 1,000.00 ms for "
                                 "STORE_USER_ALL hold time\"\n"),
                      parser, errorMsg))
        << errorMsg;

    const auto& commands = parser.getParsedImage().commands;
    ASSERT_EQ(commands.size(), 1u);
    EXPECT_EQ(commands[0].pauseMs, 1000u);
}

TEST(TPS544B28CsvParserTest, SkipsCommentLines)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parse(withHeader("Comment,Write FREQUENCY_SWITCH 800 kHz,\n"
                                 "WriteWord,0x33,0x0638\n"
                                 "Comment,\"Quoted, with a comma\",\n"),
                      parser, errorMsg))
        << errorMsg;

    EXPECT_EQ(parser.getParsedImage().commands.size(), 1u);
}

TEST(TPS544B28CsvParserTest, RecordsLineNumbers)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parse(withHeader("Comment,filler,\n"
                                 "WriteByte,0x02,0x16\n"),
                      parser, errorMsg))
        << errorMsg;

    const auto& commands = parser.getParsedImage().commands;
    ASSERT_EQ(commands.size(), 1u);
    EXPECT_EQ(commands[0].lineNo, 3u);
}

TEST(TPS544B28CsvParserTest, RejectsMissingHeader)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    EXPECT_FALSE(parse("WriteByte,0x02,0x16\n", parser, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST(TPS544B28CsvParserTest, RejectsScriptWithNoCommands)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    EXPECT_FALSE(parse(withHeader(""), parser, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST(TPS544B28CsvParserTest, RejectsUnknownOperation)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    EXPECT_FALSE(parse(withHeader("WriteQuad,0x02,0x16\n"), parser, errorMsg));
    EXPECT_NE(errorMsg.find("unknown operation"), std::string::npos);
}

TEST(TPS544B28CsvParserTest, RejectsWrongWriteWordLength)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    EXPECT_FALSE(parse(withHeader("WriteWord,0x33,0x06\n"), parser, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST(TPS544B28CsvParserTest, AcceptsBlockWriteOf255DataBytes)
{
    // Length byte 0xFF followed by 255 data bytes.
    const std::string data = "0xFF" + std::string(255 * 2, '0');

    TPS544B28CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parse(withHeader("BlockWrite,0x9B," + data + "\n"), parser,
                      errorMsg))
        << errorMsg;
    EXPECT_EQ(parser.getParsedImage().commands[0].dataBytes.size(), 256u);
}

TEST(TPS544B28CsvParserTest, RejectsBlockWriteOver255DataBytes)
{
    // Length byte followed by 256 data bytes, one more than an SMBus block
    // can carry.
    const std::string data = "0xFF" + std::string(256 * 2, '0');

    TPS544B28CsvParser parser;
    std::string errorMsg;

    EXPECT_FALSE(parse(withHeader("BlockWrite,0x9B," + data + "\n"), parser,
                       errorMsg));
    EXPECT_NE(errorMsg.find("255"), std::string::npos);
}

TEST(TPS544B28CsvParserTest, RejectsOddHexDigits)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    EXPECT_FALSE(parse(withHeader("WriteByte,0x02,0x1\n"), parser, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST(TPS544B28CsvParserTest, RejectsNonHexData)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    EXPECT_FALSE(parse(withHeader("WriteByte,0x02,0xZZ\n"), parser, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST(TPS544B28CsvParserTest, RejectsPauseWithoutDuration)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    EXPECT_FALSE(parse(withHeader("Pause,,\n"), parser, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

TEST(TPS544B28CsvParserTest, ClearsPreviousImage)
{
    TPS544B28CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parse(withHeader("WriteByte,0x02,0x16\n"
                                 "WriteByte,0x01,0x00\n"),
                      parser, errorMsg))
        << errorMsg;
    ASSERT_EQ(parser.getParsedImage().commands.size(), 2u);

    ASSERT_TRUE(parse(withHeader("SendByte,0x15,\n"), parser, errorMsg))
        << errorMsg;
    EXPECT_EQ(parser.getParsedImage().commands.size(), 1u);
}

// A trimmed excerpt of a TI-generated script, covering identity check, a few
// writes with their read backs, the store and the reset.
TEST(TPS544B28CsvParserTest, ParsesRepresentativeTiScript)
{
    const std::string csv = withHeader(
        "Comment,\"SMBus Fields are Request,Address,Command,Data\",\n"
        "Comment,Verifying IC_DEVICE_ID is 0x5449544B2800,\n"
        "BlockRead,0xAD,0x065449544B2800\n"
        "Comment,Write ON_OFF_CONFIG Mode: OPERATION only to turn rail off,\n"
        "WriteByte,0x02,0x00\n"
        "WriteByte,0x01,0x00\n"
        "WriteByte,0x02,0x16\n"
        "ReadByte,0x02,0x16\n"
        "Comment,Write FREQUENCY_SWITCH 800 kHz,\n"
        "WriteWord,0x33,0x0638\n"
        "ReadWord,0x33,0x0638\n"
        "Comment,Write IOUT_OC_FAULT_LIMIT 13.00 A,\n"
        "WriteWord,0x46,0x0C00\n"
        "ReadWord,0x46,0x0C00\n"
        "Comment,Write MFR_ID TI,\n"
        "BlockWrite,0x99,0x025449\n"
        "BlockRead,0x99,0x025449\n"
        "Comment,Execute STORE_USER_ALL,\n"
        "SendByte,0x15,\n"
        "Pause,1000,\"Pausing 1,000.00 ms for STORE_USER_ALL hold time\"\n"
        "Reset,,\n"
        "Pause,100,Wait 100.00 ms for device I2C interface to become "
        "available\n"
        "ReadByte,0x02,0x16\n");

    TPS544B28CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parse(csv, parser, errorMsg)) << errorMsg;

    const auto& image = parser.getParsedImage();

    EXPECT_FALSE(image.pecEnabled);
    ASSERT_EQ(image.commands.size(), 16u);

    // The identity check is the very first command, before any write.
    EXPECT_EQ(image.commands[0].op, TPS544B28CsvParser::CsvOp::BlockRead);
    EXPECT_EQ(image.commands[0].cmd, 0xAD);
    EXPECT_EQ(image.commands[0].expectedBytes,
              (std::vector<uint8_t>{0x06, 0x54, 0x49, 0x54, 0x4B, 0x28,
                                    0x00}));

    // STORE_USER_ALL is followed by the hold time the script asks for.
    size_t storeIndex = image.commands.size();
    for (size_t i = 0; i < image.commands.size(); i++)
    {
        if (image.commands[i].op == TPS544B28CsvParser::CsvOp::SendByte &&
            image.commands[i].cmd == 0x15)
        {
            storeIndex = i;
            break;
        }
    }

    ASSERT_LT(storeIndex, image.commands.size());
    ASSERT_LT(storeIndex + 1, image.commands.size());
    EXPECT_EQ(image.commands[storeIndex + 1].op,
              TPS544B28CsvParser::CsvOp::Pause);
    EXPECT_EQ(image.commands[storeIndex + 1].pauseMs, 1000u);

    // A reset follows, which is where our executor has to stop.
    ASSERT_LT(storeIndex + 2, image.commands.size());
    EXPECT_EQ(image.commands[storeIndex + 2].op,
              TPS544B28CsvParser::CsvOp::Reset);
}
