#include "msl_verify.hpp"

#include <string>

#include <gtest/gtest.h>

using minimum_ship_level::compare;
using minimum_ship_level::parse;
using minimum_ship_level::Version;

namespace
{

const std::string pattern = "^([a-z]*)([0-9]+)\\.([0-9]+)\\.([0-9]+)";

} // namespace

TEST(MinimumShipLevelTest, ParsesVersionComponents)
{
    Version version{};

    EXPECT_TRUE(parse("v2.14.3", pattern, version));
    EXPECT_EQ(version.major, 2U);
    EXPECT_EQ(version.minor, 14U);
    EXPECT_EQ(version.rev, 3U);
}

TEST(MinimumShipLevelTest, KeepsComponentsAbove255)
{
    Version version{};

    EXPECT_TRUE(parse("v1.300.70000", pattern, version));
    EXPECT_EQ(version.major, 1U);
    EXPECT_EQ(version.minor, 300U);
    EXPECT_EQ(version.rev, 70000U);
}

TEST(MinimumShipLevelTest, LargeMinorIsNotTruncatedBelowMsl)
{
    Version msl{};
    Version actual{};

    ASSERT_TRUE(parse("v1.300.0", pattern, msl));
    ASSERT_TRUE(parse("v1.44.0", pattern, actual));

    EXPECT_LT(compare(actual, msl), 0);
}

TEST(MinimumShipLevelTest, RejectsNumberOutOfRange)
{
    Version version{7, 7, 7};

    EXPECT_FALSE(parse("v1.99999999999999999999.0", pattern, version));
    EXPECT_EQ(version.major, 0U);
    EXPECT_EQ(version.minor, 0U);
    EXPECT_EQ(version.rev, 0U);
}

TEST(MinimumShipLevelTest, RejectsVersionWithoutMatch)
{
    Version version{7, 7, 7};

    EXPECT_FALSE(parse("not-a-version", pattern, version));
    EXPECT_EQ(version.major, 0U);
    EXPECT_EQ(version.minor, 0U);
    EXPECT_EQ(version.rev, 0U);
}

TEST(MinimumShipLevelTest, RejectsInvalidPattern)
{
    Version version{};

    EXPECT_FALSE(parse("v1.2.3", "([", version));
}

TEST(MinimumShipLevelTest, RejectsPatternWithoutEnoughGroups)
{
    Version version{};

    EXPECT_FALSE(parse("v1.2.3", "^v([0-9]+)", version));
}

TEST(MinimumShipLevelTest, CompareOrdersByMajorMinorRev)
{
    EXPECT_EQ(compare({2, 0, 0}, {1, 9, 9}), 1);
    EXPECT_EQ(compare({1, 2, 0}, {1, 3, 0}), -1);
    EXPECT_EQ(compare({1, 2, 4}, {1, 2, 3}), 1);
    EXPECT_EQ(compare({1, 2, 3}, {1, 2, 3}), 0);
}
