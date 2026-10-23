#include "cpld/altera/max10_standard_cpld.hpp"

#include <cstdint>
#include <optional>

#include <gtest/gtest.h>

namespace phosphor::software::cpld
{
namespace
{

constexpr uint32_t sectorProtectBits = 0x1FU << 23;

class TestMax10CPLD : public Max10StandardCPLD
{
  public:
    TestMax10CPLD(sdbusplus::async::context& ctx, const Max10Profile& profile) :
        Max10StandardCPLD(ctx, 65535, 0x40, "10M16", "CFM1", profile)
    {}

    bool protect(sdbusplus::async::context& ctx)
    {
        bool result = false;
        ctx.spawn(protectTask(ctx, result));
        ctx.run();
        return result;
    }

    bool readOk = true;
    uint32_t ctrlValue = 0;
    std::optional<uint32_t> writtenReg;
    std::optional<uint32_t> writtenValue;

  protected:
    sdbusplus::async::task<bool> readReg(uint32_t, uint32_t& value) override
    {
        value = ctrlValue;
        co_return readOk;
    }

    sdbusplus::async::task<bool> writeReg(uint32_t reg, uint32_t value) override
    {
        writtenReg = reg;
        writtenValue = value;
        co_return true;
    }

  private:
    sdbusplus::async::task<> protectTask(sdbusplus::async::context& ctx,
                                         bool& result)
    {
        result = co_await protectSectors();
        ctx.request_stop();
    }
};

class Max10ProtectSectorsTest : public testing::Test
{
  protected:
    Max10ProtectSectorsTest() = default;
    ~Max10ProtectSectorsTest() noexcept override = default;
    Max10ProtectSectorsTest(const Max10ProtectSectorsTest&) = delete;
    Max10ProtectSectorsTest& operator=(const Max10ProtectSectorsTest&) = delete;
    Max10ProtectSectorsTest(Max10ProtectSectorsTest&&) = delete;
    Max10ProtectSectorsTest& operator=(Max10ProtectSectorsTest&&) = delete;

    sdbusplus::async::context ctx;
    Max10Profile profile;
    TestMax10CPLD cpld{ctx, profile};
};

TEST_F(Max10ProtectSectorsTest, SetsSectorProtectBitsOnly)
{
    constexpr uint32_t initial = 0x00123456;
    cpld.ctrlValue = initial;

    ASSERT_TRUE(cpld.protect(ctx));

    ASSERT_TRUE(cpld.writtenValue.has_value());
    EXPECT_EQ(*cpld.writtenReg, profile.csrBase + 0x04);
    EXPECT_EQ(*cpld.writtenValue, initial | sectorProtectBits);
}

TEST_F(Max10ProtectSectorsTest, DoesNotWriteWhenReadFails)
{
    cpld.readOk = false;

    EXPECT_FALSE(cpld.protect(ctx));
    EXPECT_FALSE(cpld.writtenValue.has_value());
}

} // namespace
} // namespace phosphor::software::cpld
