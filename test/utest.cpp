#include "version.hpp"
#include <gtest/gtest.h>
#include <experimental/filesystem>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace phosphor::software::manager;
namespace fs = std::experimental::filesystem;


class VersionTest : public testing::Test
{
    protected:

        virtual void SetUp()
        {
            char versionDir[] = "./versionXXXXXX";
            _directory = mkdtemp(versionDir);

            if (_directory.empty())
            {
                throw std::bad_alloc();
            }
        }

        virtual void TearDown()
        {
            fs::remove_all(_directory);
        }

        std::string _directory;
};

/** @brief Make sure we correctly get the version and purpose from getValue()*/
TEST_F(VersionTest, TestGetValue)
{
    auto manifestFilePath = _directory + "/" + "MANIFEST";
    auto version = "test-version";
    auto purpose = "BMC";

    std::ofstream file;
    file.open(manifestFilePath, std::ofstream::out);
    ASSERT_TRUE(file.is_open());

    file << "version=" << version << std::endl;
    file << "purpose=" << purpose << std::endl;
    file.close();

    EXPECT_EQ(Version<int>::getValue(manifestFilePath, "version"), version);
    EXPECT_EQ(Version<int>::getValue(manifestFilePath, "purpose"), purpose);
}

/** @brief Make sure we correctly get the Id from getId()*/
TEST_F(VersionTest, TestGetId)
{
    std::stringstream hexId;
    auto version = "test-id";

    hexId << std::hex << ((std::hash<std::string> {}(
                               version)) & 0xFFFFFFFF);

    EXPECT_EQ(Version<int>::getId(version), hexId.str());

}
