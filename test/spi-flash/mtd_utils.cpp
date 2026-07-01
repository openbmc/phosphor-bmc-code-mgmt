#include "spi-flash/mtd_utils.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

namespace phosphor::software::manager
{
namespace
{

class MTDPartitionTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        auto pattern = (std::filesystem::temp_directory_path() /
                        "spi-mtd-partition-XXXXXX")
                           .string();
        const auto directory = mkdtemp(pattern.data());
        ASSERT_NE(directory, nullptr);
        mtdPath = directory;
    }

    void TearDown() override
    {
        if (!mtdPath.empty())
        {
            std::filesystem::remove_all(mtdPath);
        }
    }

    void addPartition(const std::string& name, const std::string& offset)
    {
        const auto path = mtdPath / name;
        std::filesystem::create_directories(path);
        std::ofstream file(path / "offset");
        file << offset << '\n';
        ASSERT_TRUE(file.good());
    }

    std::filesystem::path mtdPath;
};

TEST_F(MTDPartitionTest, SelectsZeroOffset)
{
    addPartition("mtd11", "0");
    addPartition("mtd12", "33554432");
    EXPECT_EQ(findMTDPartition(mtdPath, 0), "/dev/mtd11");
}

TEST_F(MTDPartitionTest, SelectsNonzeroOffset)
{
    addPartition("mtd11", "0");
    addPartition("mtd12", "33554432");
    EXPECT_EQ(findMTDPartition(mtdPath, 33554432), "/dev/mtd12");
    EXPECT_FALSE(findMTDPartition(mtdPath, 1).has_value());
}

TEST_F(MTDPartitionTest, IgnoresWholeFlashDevice)
{
    std::filesystem::create_directories(mtdPath / "mtd10");
    EXPECT_FALSE(findMTDPartition(mtdPath, 0).has_value());
}

TEST_F(MTDPartitionTest, IgnoresReadOnlyAndNonMTDEntries)
{
    addPartition("mtd11ro", "0");
    addPartition("mtdblock11", "0");
    addPartition("mtd", "0");
    addPartition("other/mtd12", "0");
    addPartition("mtd11", "0");
    EXPECT_EQ(findMTDPartition(mtdPath, 0), "/dev/mtd11");
}

TEST_F(MTDPartitionTest, IgnoresMalformedOffsets)
{
    addPartition("mtd1", "");
    addPartition("mtd2", "-1");
    addPartition("mtd3", "0junk");
    addPartition("mtd4", "18446744073709551616");
    addPartition("mtd5", "0x0");
    EXPECT_FALSE(findMTDPartition(mtdPath, 0).has_value());
}

TEST_F(MTDPartitionTest, RejectsDuplicateOffsets)
{
    addPartition("mtd11", "0");
    addPartition("mtd12", "0");
    EXPECT_FALSE(findMTDPartition(mtdPath, 0).has_value());
}

TEST_F(MTDPartitionTest, SupportsPartitionedMaster)
{
    addPartition("mtd10/mtd11", "0");
    addPartition("mtd10/mtd12", "33554432");
    EXPECT_EQ(findMTDPartition(mtdPath, 0), "/dev/mtd11");
    EXPECT_EQ(findMTDPartition(mtdPath, 33554432), "/dev/mtd12");
}

TEST_F(MTDPartitionTest, ResolvesNestedOffsetsFromChipStart)
{
    addPartition("mtd10/mtd11", "33554432");
    addPartition("mtd10/mtd11/mtd12", "1048576");
    EXPECT_EQ(findMTDPartition(mtdPath, 34603008), "/dev/mtd12");
    EXPECT_FALSE(findMTDPartition(mtdPath, 1048576).has_value());
}

TEST_F(MTDPartitionTest, RejectsOverlappingPartitionStarts)
{
    addPartition("mtd11", "33554432");
    addPartition("mtd11/mtd12", "0");
    EXPECT_FALSE(findMTDPartition(mtdPath, 33554432).has_value());
}

TEST_F(MTDPartitionTest, RejectsInvalidParentOffset)
{
    addPartition("mtd11", "invalid");
    addPartition("mtd11/mtd12", "0");
    EXPECT_FALSE(findMTDPartition(mtdPath, 0).has_value());
}

TEST_F(MTDPartitionTest, RejectsOverflowInNestedOffset)
{
    addPartition("mtd11", "18446744073709551615");
    addPartition("mtd11/mtd12", "1");
    EXPECT_FALSE(findMTDPartition(mtdPath, 0).has_value());
}

TEST_F(MTDPartitionTest, SupportsOffsetsAbove32Bits)
{
    addPartition("mtd11", "4294967296");
    EXPECT_EQ(findMTDPartition(mtdPath, 4294967296), "/dev/mtd11");
}

TEST_F(MTDPartitionTest, IgnoresSymlinks)
{
    addPartition("mtd11", "0");
    std::filesystem::create_directory_symlink(mtdPath / "mtd11",
                                              mtdPath / "mtd12");
    EXPECT_EQ(findMTDPartition(mtdPath, 0), "/dev/mtd11");
}

TEST_F(MTDPartitionTest, RejectsMissingDirectory)
{
    EXPECT_FALSE(findMTDPartition(mtdPath / "missing", 0).has_value());
}

TEST_F(MTDPartitionTest, SupportsTrailingSlash)
{
    addPartition("mtd11", "0");
    EXPECT_EQ(findMTDPartition(mtdPath / "", 0), "/dev/mtd11");
}

} // namespace
} // namespace phosphor::software::manager
