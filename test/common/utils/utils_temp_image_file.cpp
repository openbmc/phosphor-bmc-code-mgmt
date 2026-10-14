#include "common/include/utils.hpp"

#include <sys/stat.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

TEST(TempImageFileTest, CreatesOwnerOnlyFileAndRemovesItOnDestruction)
{
    std::string path;
    {
        auto tmp = TempImageFile::create("utils-test");
        ASSERT_TRUE(tmp.has_value());
        path = tmp->path();

        struct stat st{};
        ASSERT_EQ(stat(path.c_str(), &st), 0);
        EXPECT_EQ(st.st_mode & 0777, 0600);
    }
    EXPECT_FALSE(fs::exists(path));
}

TEST(TempImageFileTest, WritesContent)
{
    const std::array<uint8_t, 4> data = {0xDE, 0xAD, 0xBE, 0xEF};

    auto tmp = TempImageFile::create("utils-test");
    ASSERT_TRUE(tmp.has_value());
    ASSERT_TRUE(tmp->writeAndClose(data.data(), data.size()));

    std::ifstream in(tmp->path(), std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    ASSERT_EQ(content.size(), data.size());
    EXPECT_EQ(std::string(content.begin(), content.end()),
              std::string(data.begin(), data.end()));
}

TEST(TempImageFileTest, NamesAreUnique)
{
    auto a = TempImageFile::create("utils-test");
    auto b = TempImageFile::create("utils-test");
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_NE(a->path(), b->path());
}

TEST(TempImageFileTest, MoveTransfersOwnership)
{
    auto a = TempImageFile::create("utils-test");
    ASSERT_TRUE(a.has_value());
    const std::string path = a->path();

    {
        TempImageFile b = std::move(*a);
        EXPECT_TRUE(fs::exists(path));
    }
    EXPECT_FALSE(fs::exists(path));
}
