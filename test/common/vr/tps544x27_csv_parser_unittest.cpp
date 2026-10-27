#include "i2c-vr/tps544x27/csv_parser.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace phosphor::software::VR;

TEST(TPS544X27CsvParserTest, ParsesMetadata)
{
    const std::string csv =
        "Comment,Format=CSV,Hex=CoderUpper,BreakOutBytes=False,PEC=True,"
        "SMBusWordCommandBytesOrderAsLowByteFirst=True,"
        "IncludeBlockLength=True,HasAddress=False\n";

    TPS544X27CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parser.parseImage(reinterpret_cast<const uint8_t*>(csv.data()),
                                  csv.size(), errorMsg))
        << errorMsg;

    const auto& image = parser.getParsedImage();

    EXPECT_TRUE(image.pecEnabled);
    EXPECT_TRUE(image.wordLowByteFirst);
    EXPECT_TRUE(image.includeBlockLength);
    EXPECT_TRUE(image.commands.empty());
}

TEST(TPS544X27CsvParserTest, ParsesWriteByte)
{
    const std::string csv = "WriteByte,0x01,0x1122\n";

    TPS544X27CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parser.parseImage(reinterpret_cast<const uint8_t*>(csv.data()),
                                  csv.size(), errorMsg))
        << errorMsg;

    const auto& image = parser.getParsedImage();

    ASSERT_EQ(image.commands.size(), 1);

    const auto& cmd = image.commands[0];

    EXPECT_EQ(cmd.op, TPS544X27CsvParser::CsvOp::WriteByte);
    EXPECT_EQ(cmd.cmd, 0x01);
    EXPECT_EQ(cmd.dataBytes, (std::vector<uint8_t>{0x11}));
    EXPECT_TRUE(cmd.hasCsvPEC);
    EXPECT_EQ(cmd.csvPEC, 0x22);
}

TEST(TPS544X27CsvParserTest, ParsesWriteWord)
{
    const std::string csv = "WriteWord,0x02,0x112233\n";

    TPS544X27CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parser.parseImage(reinterpret_cast<const uint8_t*>(csv.data()),
                                  csv.size(), errorMsg))
        << errorMsg;

    const auto& image = parser.getParsedImage();

    ASSERT_EQ(image.commands.size(), 1);

    const auto& cmd = image.commands[0];

    EXPECT_EQ(cmd.op, TPS544X27CsvParser::CsvOp::WriteWord);
    EXPECT_EQ(cmd.cmd, 0x02);
    EXPECT_EQ(cmd.dataBytes, (std::vector<uint8_t>{0x11, 0x22}));
    EXPECT_TRUE(cmd.hasWordValue);
    EXPECT_EQ(cmd.wordValue, 0x2211);
    EXPECT_TRUE(cmd.hasCsvPEC);
    EXPECT_EQ(cmd.csvPEC, 0x33);
}

TEST(TPS544X27CsvParserTest, ParsesReadCommands)
{
    const std::string csv = "ReadByte,0x03,0x44\n"
                            "ReadWord,0x04,0x5566\n";

    TPS544X27CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parser.parseImage(reinterpret_cast<const uint8_t*>(csv.data()),
                                  csv.size(), errorMsg))
        << errorMsg;

    const auto& image = parser.getParsedImage();

    ASSERT_EQ(image.commands.size(), 2);

    EXPECT_EQ(image.commands[0].op, TPS544X27CsvParser::CsvOp::ReadByte);
    EXPECT_EQ(image.commands[0].cmd, 0x03);
    EXPECT_EQ(image.commands[0].expectedBytes, (std::vector<uint8_t>{0x44}));

    EXPECT_EQ(image.commands[1].op, TPS544X27CsvParser::CsvOp::ReadWord);
    EXPECT_EQ(image.commands[1].cmd, 0x04);
    EXPECT_EQ(image.commands[1].expectedBytes,
              (std::vector<uint8_t>{0x55, 0x66}));
}

TEST(TPS544X27CsvParserTest, ParsesBlockWrite)
{
    const std::string csv = "BlockWrite,0x05,0x0311223344\n";

    TPS544X27CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parser.parseImage(reinterpret_cast<const uint8_t*>(csv.data()),
                                  csv.size(), errorMsg))
        << errorMsg;

    const auto& image = parser.getParsedImage();

    ASSERT_EQ(image.commands.size(), 1);

    const auto& cmd = image.commands[0];

    EXPECT_EQ(cmd.op, TPS544X27CsvParser::CsvOp::BlockWrite);
    EXPECT_EQ(cmd.cmd, 0x05);
    EXPECT_EQ(cmd.dataBytes, (std::vector<uint8_t>{0x11, 0x22, 0x33}));
    EXPECT_TRUE(cmd.hasCsvPEC);
    EXPECT_EQ(cmd.csvPEC, 0x44);
}

TEST(TPS544X27CsvParserTest, ParsesBlockRead)
{
    const std::string csv = "BlockRead,0x06,0x03112233\n";

    TPS544X27CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parser.parseImage(reinterpret_cast<const uint8_t*>(csv.data()),
                                  csv.size(), errorMsg))
        << errorMsg;

    const auto& image = parser.getParsedImage();

    ASSERT_EQ(image.commands.size(), 1);

    const auto& cmd = image.commands[0];

    EXPECT_EQ(cmd.op, TPS544X27CsvParser::CsvOp::BlockRead);
    EXPECT_EQ(cmd.cmd, 0x06);
    EXPECT_EQ(cmd.expectedBytes,
              (std::vector<uint8_t>{0x03, 0x11, 0x22, 0x33}));
}

TEST(TPS544X27CsvParserTest, ParsesBlockProcessCall)
{
    const std::string csv = "BlockProcessCall,0x0711,0x0122\n";

    TPS544X27CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parser.parseImage(reinterpret_cast<const uint8_t*>(csv.data()),
                                  csv.size(), errorMsg))
        << errorMsg;

    const auto& image = parser.getParsedImage();

    ASSERT_EQ(image.commands.size(), 1);

    const auto& cmd = image.commands[0];

    EXPECT_EQ(cmd.op, TPS544X27CsvParser::CsvOp::BlockProcessCall);
    EXPECT_EQ(cmd.cmd, 0x07);
    EXPECT_EQ(cmd.dataBytes, (std::vector<uint8_t>{0x11}));
    EXPECT_EQ(cmd.expectedBytes, (std::vector<uint8_t>{0x01, 0x22}));
}

TEST(TPS544X27CsvParserTest, ParsesPauseAndReset)
{
    const std::string csv = "Pause,1000\n"
                            "Reset\n";

    TPS544X27CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parser.parseImage(reinterpret_cast<const uint8_t*>(csv.data()),
                                  csv.size(), errorMsg))
        << errorMsg;

    const auto& image = parser.getParsedImage();

    ASSERT_EQ(image.commands.size(), 2);

    EXPECT_EQ(image.commands[0].op, TPS544X27CsvParser::CsvOp::Pause);
    EXPECT_EQ(image.commands[0].pauseMs, 1000);

    EXPECT_EQ(image.commands[1].op, TPS544X27CsvParser::CsvOp::Reset);
}

TEST(TPS544X27CsvParserTest, ParsesRepresentativeFusionCsv)
{
    const std::string csv =
        "Comment,Format=CSV,Hex=CoderUpper,BreakOutBytes=False,PEC=True,"
        "SMBusWordCommandBytesOrderAsLowByteFirst=True,"
        "IncludeBlockLength=True,HasAddress=False\n"
        "BlockRead,0x01,0x03112233\n"
        "WriteByte,0x02,0x4455\n"
        "ReadByte,0x02,0x44\n"
        "WriteWord,0x03,0x667788\n"
        "ReadWord,0x03,0x6677\n"
        "BlockWrite,0x04,0x0311223344\n"
        "BlockRead,0x04,0x03112233\n"
        "BlockProcessCall,0x0511,0x0122\n"
        "SendByte,0x06\n"
        "Pause,1000\n"
        "Reset\n";

    TPS544X27CsvParser parser;
    std::string errorMsg;

    ASSERT_TRUE(parser.parseImage(reinterpret_cast<const uint8_t*>(csv.data()),
                                  csv.size(), errorMsg))
        << errorMsg;

    const auto& image = parser.getParsedImage();

    EXPECT_TRUE(image.pecEnabled);
    EXPECT_TRUE(image.wordLowByteFirst);
    EXPECT_TRUE(image.includeBlockLength);

    ASSERT_EQ(image.commands.size(), 11);

    EXPECT_EQ(image.commands[0].op, TPS544X27CsvParser::CsvOp::BlockRead);
    EXPECT_EQ(image.commands[1].op, TPS544X27CsvParser::CsvOp::WriteByte);
    EXPECT_EQ(image.commands[2].op, TPS544X27CsvParser::CsvOp::ReadByte);
    EXPECT_EQ(image.commands[3].op, TPS544X27CsvParser::CsvOp::WriteWord);
    EXPECT_EQ(image.commands[4].op, TPS544X27CsvParser::CsvOp::ReadWord);
    EXPECT_EQ(image.commands[5].op, TPS544X27CsvParser::CsvOp::BlockWrite);
    EXPECT_EQ(image.commands[6].op, TPS544X27CsvParser::CsvOp::BlockRead);
    EXPECT_EQ(image.commands[7].op,
              TPS544X27CsvParser::CsvOp::BlockProcessCall);
    EXPECT_EQ(image.commands[8].op, TPS544X27CsvParser::CsvOp::SendByte);
    EXPECT_EQ(image.commands[9].op, TPS544X27CsvParser::CsvOp::Pause);
    EXPECT_EQ(image.commands[10].op, TPS544X27CsvParser::CsvOp::Reset);
}

TEST(TPS544X27CsvParserTest, RejectsUnknownOperation)
{
    const std::string csv = "Unknown,0x01,0x1122\n";

    TPS544X27CsvParser parser;
    std::string errorMsg;

    EXPECT_FALSE(parser.parseImage(reinterpret_cast<const uint8_t*>(csv.data()),
                                   csv.size(), errorMsg));

    EXPECT_NE(errorMsg.find("unknown operation"), std::string::npos);
}

TEST(TPS544X27CsvParserTest, RejectsInvalidBlockWriteLength)
{
    const std::string csv = "BlockWrite,0x05,0x0211223344\n";

    TPS544X27CsvParser parser;
    std::string errorMsg;

    EXPECT_FALSE(parser.parseImage(reinterpret_cast<const uint8_t*>(csv.data()),
                                   csv.size(), errorMsg));

    EXPECT_NE(errorMsg.find("length mismatch"), std::string::npos);
}

TEST(TPS544X27CsvParserTest, ClearsPreviousImage)
{
    TPS544X27CsvParser parser;
    std::string errorMsg;

    const std::string first = "WriteByte,0x01,0x1122\n"
                              "SendByte,0x02\n";

    ASSERT_TRUE(parser.parseImage(
        reinterpret_cast<const uint8_t*>(first.data()), first.size(), errorMsg))
        << errorMsg;

    ASSERT_EQ(parser.getParsedImage().commands.size(), 2);

    const std::string second = "Reset\n";

    ASSERT_TRUE(
        parser.parseImage(reinterpret_cast<const uint8_t*>(second.data()),
                          second.size(), errorMsg))
        << errorMsg;

    const auto& image = parser.getParsedImage();

    ASSERT_EQ(image.commands.size(), 1);
    EXPECT_EQ(image.commands[0].op, TPS544X27CsvParser::CsvOp::Reset);
}
