#include "common/include/utils.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

TEST(UtilsTest, Trim)
{
    EXPECT_EQ(trim("WriteWord"), "WriteWord");
    EXPECT_EQ(trim("  WriteWord  "), "WriteWord");
    EXPECT_EQ(trim("\t0x01\t"), "0x01");
    EXPECT_EQ(trim("  0x112233\r\n"), "0x112233");
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(trim("   \t\r\n"), "");
}

TEST(UtilsTest, ParseHexByte)
{
    uint8_t value = 0;

    ASSERT_TRUE(parseHexByte("0x01", value));
    EXPECT_EQ(value, 0x01);

    ASSERT_TRUE(parseHexByte("0XAB", value));
    EXPECT_EQ(value, 0xAB);

    ASSERT_TRUE(parseHexByte("ff", value));
    EXPECT_EQ(value, 0xFF);

    ASSERT_TRUE(parseHexByte("  0x02  ", value));
    EXPECT_EQ(value, 0x02);

    ASSERT_TRUE(parseHexByte("0", value));
    EXPECT_EQ(value, 0x00);
}

TEST(UtilsTest, ParseHexByteInvalid)
{
    uint8_t value = 0;

    EXPECT_FALSE(parseHexByte("", value));
    EXPECT_FALSE(parseHexByte("0x", value));
    EXPECT_FALSE(parseHexByte("0x100", value));
    EXPECT_FALSE(parseHexByte("123", value));
}

TEST(UtilsTest, ParseHexBytes)
{
    std::vector<uint8_t> output;

    ASSERT_TRUE(parseHexBytes("0x1122", output));
    EXPECT_EQ(output, (std::vector<uint8_t>{0x11, 0x22}));

    ASSERT_TRUE(parseHexBytes("0x112233", output));
    EXPECT_EQ(output, (std::vector<uint8_t>{0x11, 0x22, 0x33}));

    ASSERT_TRUE(parseHexBytes("0x0311223344", output));
    EXPECT_EQ(output, (std::vector<uint8_t>{0x03, 0x11, 0x22, 0x33, 0x44}));

    ASSERT_TRUE(parseHexBytes("0xAABB", output));
    EXPECT_EQ(output, (std::vector<uint8_t>{0xAA, 0xBB}));

    ASSERT_TRUE(parseHexBytes("  0x112233  ", output));
    EXPECT_EQ(output, (std::vector<uint8_t>{0x11, 0x22, 0x33}));
}

TEST(UtilsTest, ParseHexBytesInvalid)
{
    std::vector<uint8_t> output;

    EXPECT_FALSE(parseHexBytes("", output));
    EXPECT_FALSE(parseHexBytes("0x", output));
    EXPECT_FALSE(parseHexBytes("0x1", output));
    EXPECT_FALSE(parseHexBytes("123", output));
}

TEST(UtilsTest, ByteToHex)
{
    EXPECT_EQ(byteToHex(0x00), "0x00");
    EXPECT_EQ(byteToHex(0x01), "0x01");
    EXPECT_EQ(byteToHex(0x0A), "0x0A");
    EXPECT_EQ(byteToHex(0xAB), "0xAB");
    EXPECT_EQ(byteToHex(0xFF), "0xFF");
}

TEST(UtilsTest, BytesToHex)
{
    EXPECT_EQ(bytesToHex({}), "-");
    EXPECT_EQ(bytesToHex({0x11}), "0x11");
    EXPECT_EQ(bytesToHex({0x11, 0x22}), "0x11 0x22");
    EXPECT_EQ(bytesToHex({0x11, 0x22, 0x33}), "0x11 0x22 0x33");
}

TEST(UtilsTest, SplitCsvLine)
{
    auto fields = splitCsvLine("WriteWord,0x01,0x112233");

    ASSERT_EQ(fields.size(), 3);
    EXPECT_EQ(fields[0], "WriteWord");
    EXPECT_EQ(fields[1], "0x01");
    EXPECT_EQ(fields[2], "0x112233");

    fields = splitCsvLine("BlockWrite,0x02,0x0311223344");

    ASSERT_EQ(fields.size(), 3);
    EXPECT_EQ(fields[0], "BlockWrite");
    EXPECT_EQ(fields[1], "0x02");
    EXPECT_EQ(fields[2], "0x0311223344");

    fields = splitCsvLine("BlockProcessCall,0x0311,0x0122");

    ASSERT_EQ(fields.size(), 3);
    EXPECT_EQ(fields[0], "BlockProcessCall");
    EXPECT_EQ(fields[1], "0x0311");
    EXPECT_EQ(fields[2], "0x0122");
}

TEST(UtilsTest, SplitCsvLineTrimsFields)
{
    const auto fields = splitCsvLine("  WriteByte , 0x01 , 0x1122  ");

    ASSERT_EQ(fields.size(), 3);
    EXPECT_EQ(fields[0], "WriteByte");
    EXPECT_EQ(fields[1], "0x01");
    EXPECT_EQ(fields[2], "0x1122");
}

TEST(UtilsTest, SplitCsvLineRemovesTrailingEmptyFields)
{
    const auto fields = splitCsvLine("WriteByte,0x01,0x1122,,,");

    ASSERT_EQ(fields.size(), 3);
    EXPECT_EQ(fields[0], "WriteByte");
    EXPECT_EQ(fields[1], "0x01");
    EXPECT_EQ(fields[2], "0x1122");
}
