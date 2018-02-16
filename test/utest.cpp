#include "version.hpp"
#include "key_manager.hpp"
#include <gtest/gtest.h>
#include <experimental/filesystem>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <openssl/sha.h>

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

class KeyManagerTest : public testing::Test
{
    protected:

        virtual void SetUp()
        {
            char keyManagerDir[] = "./keyManagerXXXXXX";
            _directory = mkdtemp(keyManagerDir);

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

TEST_F(KeyManagerTest, TestGetPublicKeyPath)
{
    auto manifestFilePath = _directory + "/" + "MANIFEST";
    auto publicKeyPath = "/usr/share";

    std::ofstream file;
    file.open(manifestFilePath, std::ofstream::out);
    ASSERT_TRUE(file.is_open());

    file << "publickeypath=" << publicKeyPath << std::endl;
    file.close();

    EXPECT_EQ(KeyManager::getPublicKeyPath(manifestFilePath), publicKeyPath);
}

TEST_F(KeyManagerTest, TestIsPublicKeyFound)
{
    auto manifestFilePath = _directory + "/" + "MANIFEST";

    std::ofstream file;
    file.open(manifestFilePath, std::ofstream::out);
    ASSERT_TRUE(file.is_open());

    file <<"-----BEGIN PUBLIC KEY-----" << std::endl;
    file <<"MIIBIDANBgkqhkiG9w " << std::endl;
    file <<"MIIBIDANBgkqhkiG9w " << std::endl;
    file <<"-----END PUBLIC KEY-----" << std::endl;
    file.close();

    EXPECT_EQ(KeyManager::isPublicKeyFound(manifestFilePath), true);
}

TEST_F(KeyManagerTest, TestWritePublicKey)
{
    auto manifestFilePath = _directory + "/" + "MANIFEST";

    std::ofstream file;
    file.open(manifestFilePath, std::ofstream::out);
    ASSERT_TRUE(file.is_open());

    file <<"-----BEGIN PUBLIC KEY-----" << std::endl;
    file <<"MIIBIDANBgkqhkiG9w " << std::endl;
    file <<"MIIBIDANBgkqhkiG9w " << std::endl;
    file <<"-----END PUBLIC KEY-----" << std::endl;
    file.close();

    auto pubkeyFile = _directory + "/" + "pub.Pem";
    KeyManager::writePublicKey(manifestFilePath, pubkeyFile);
    
    EXPECT_EQ(KeyManager::isPublicKeyFound(pubkeyFile), true);
}


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

    EXPECT_EQ(Version::getValue(manifestFilePath, "version"), version);
    EXPECT_EQ(Version::getValue(manifestFilePath, "purpose"), purpose);
}

/** @brief Make sure we correctly get the Id from getId()*/
TEST_F(VersionTest, TestGetId)
{
    auto version = "test-id";
    unsigned char digest[SHA512_DIGEST_LENGTH];
    SHA512_CTX ctx;
    SHA512_Init(&ctx);
    SHA512_Update(&ctx, version, strlen(version));
    SHA512_Final(digest, &ctx);
    char mdString[SHA512_DIGEST_LENGTH*2+1];
    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
    {
        snprintf(&mdString[i*2], 3, "%02x", (unsigned int)digest[i]);
    }
    std::string hexId = std::string(mdString);
    hexId = hexId.substr(0, 8);
    EXPECT_EQ(Version::getId(version), hexId);
}
