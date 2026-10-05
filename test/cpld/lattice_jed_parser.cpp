#include "cpld/lattice/lattice_base_cpld.hpp"

#include <sdbusplus/async.hpp>

#include <cstdint>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

using namespace phosphor::software::cpld;

namespace
{

// No /dev/i2c-65535 exists, so the parser runs without touching hardware.
constexpr uint16_t testBus = 0xFFFF;
constexpr uint8_t testAddress = 0x40;
constexpr std::string_view testChip = "LCMXO3LF-4300C";

class TestLatticeCPLD : public LatticeBaseCPLD
{
  public:
    TestLatticeCPLD(sdbusplus::async::context& ctx, std::string_view chip) :
        LatticeBaseCPLD(ctx, testBus, testAddress, std::string(chip), "", false)
    {}

    bool parse(std::string_view jed)
    {
        return jedFileParser(reinterpret_cast<const uint8_t*>(jed.data()),
                             jed.size());
    }

    const cpldI2cInfo& info() const
    {
        return fwInfo;
    }

  private:
    sdbusplus::async::task<bool> prepareUpdate(const uint8_t* /*unused*/,
                                               size_t /*unused*/) override
    {
        co_return false;
    }
    sdbusplus::async::task<bool> doErase() override
    {
        co_return false;
    }
    sdbusplus::async::task<bool> doUpdate() override
    {
        co_return false;
    }
    sdbusplus::async::task<bool> finishUpdate() override
    {
        co_return false;
    }
    sdbusplus::async::task<bool> readUserCode(uint32_t& /*unused*/) override
    {
        co_return false;
    }
};

std::string makeJed(std::string_view deviceName, std::string_view checksumLine,
                    std::string_view userCodeLine)
{
    return "NOTE DEVICE NAME:\t" + std::string(deviceName) +
           "-6BG256*\n"
           "QF8*\n"
           "L000000\n"
           "00000001\n"
           "*\n" +
           std::string(checksumLine) +
           "\n"
           "NOTE User Electronic Signature Data*\n" +
           std::string(userCodeLine) + "\n";
}

class LatticeJedParserTest : public testing::Test
{
  protected:
    LatticeJedParserTest() = default;
    ~LatticeJedParserTest() noexcept override {}

    sdbusplus::async::context ctx;

  public:
    LatticeJedParserTest(const LatticeJedParserTest&) = delete;
    LatticeJedParserTest(LatticeJedParserTest&&) = delete;
    LatticeJedParserTest& operator=(const LatticeJedParserTest&) = delete;
    LatticeJedParserTest& operator=(LatticeJedParserTest&&) = delete;
};

} // namespace

TEST_F(LatticeJedParserTest, ValidImageIsParsed)
{
    TestLatticeCPLD cpld(ctx, testChip);

    ASSERT_TRUE(cpld.parse(makeJed(testChip, "C1234*", "UH0000ABCD*")));
    EXPECT_EQ(cpld.info().checksum, 0x1234U);
    EXPECT_EQ(cpld.info().version, 0xABCDU);
    EXPECT_EQ(cpld.info().cfgData.size(), 1U);
}

TEST_F(LatticeJedParserTest, EmptyImageIsRejected)
{
    TestLatticeCPLD cpld(ctx, testChip);

    EXPECT_FALSE(cpld.parse(""));
}

TEST_F(LatticeJedParserTest, ImageForDifferentChipIsRejected)
{
    TestLatticeCPLD cpld(ctx, "LCMXO2-4000HC");

    EXPECT_FALSE(cpld.parse(makeJed(testChip, "C1234*", "UH0000ABCD*")));
}

TEST_F(LatticeJedParserTest, MalformedChecksumIsRejected)
{
    TestLatticeCPLD cpld(ctx, testChip);

    EXPECT_FALSE(cpld.parse(makeJed(testChip, "C*", "UH0000ABCD*")));
}

TEST_F(LatticeJedParserTest, MalformedUserCodeIsRejected)
{
    TestLatticeCPLD cpld(ctx, testChip);

    EXPECT_FALSE(cpld.parse(makeJed(testChip, "C1234*", "UH*")));
}
