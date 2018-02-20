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
            manifestFilePath = _directory + "/" + "MANIFEST";
            systemHashFilePath = _directory + "/" + "hash";
            publicKeyPath = _directory;

            keyManager = std::make_unique<KeyManager>(manifestFilePath,
                                systemHashFilePath, publicKeyPath);

            std::ofstream file;
            file.open(manifestFilePath, std::ofstream::out);
            ASSERT_TRUE(file.is_open());
            file << "KeyType=" << "OpenBmc" << std::endl;
            file << "HashType=" << "SHA512" << std::endl;
            file.close();
        }

        virtual void TearDown()
        {
            fs::remove_all(_directory);
        }

        std::string _directory;
        std::string manifestFilePath;
        std::string systemHashFilePath;
        std::string publicKeyPath;

        std::unique_ptr<KeyManager> keyManager;
};

TEST_F(KeyManagerTest, TestGetKeyTypeFromManifest)
{
    std::ofstream file;
    file.open(manifestFilePath, std::ofstream::out);
    ASSERT_TRUE(file.is_open());

    EXPECT_EQ(keyManager->getKeyTypeFromManifest(), KeyType::OpenBMC);
}

TEST_F(KeyManagerTest, TestGetHashTypeFromManifest)
{
    std::ofstream file;
    file.open(manifestFilePath, std::ofstream::out);
    ASSERT_TRUE(file.is_open());

    EXPECT_EQ(keyManager->getHashTypeFromManifest(), HashType::sha512);
}

TEST_F(KeyManagerTest, TestPublicKeyPathsTest)
{
    fs::path path = _directory;
    std::string openbmcKeyFileName{};
    fs::path openbmcPath = path;
    {
        openbmcPath /= OpenBMC;
        openbmcPath /= publicKeyFileName;
        fs::create_directories(openbmcPath.parent_path());
        openbmcKeyFileName = openbmcPath.string();
        std::ofstream file;
        file.open(openbmcKeyFileName, std::ofstream::out);
        ASSERT_TRUE(file.is_open());
        file << "-----BEGIN PUBLIC KEY-----" << std::endl;
        file << "a133#2a&*91ad~}" << std::endl;
        file << "-----END PUBLIC KEY-----" << std::endl;
        file.close();
    }
    std::string gaKeyFileName{};
    fs::path gaPath = path;
    {
        gaPath /= GA;
        gaPath /= publicKeyFileName;
        fs::create_directories(gaPath.parent_path());
        gaKeyFileName = gaPath.string();
        fs::create_directories(gaPath.parent_path());
        std::ofstream file;
        file.open(gaKeyFileName, std::ofstream::out);
        ASSERT_TRUE(file.is_open());
        file << "-----BEGIN PUBLIC KEY-----" << std::endl;
        file << "a133#2a&*91ad~}" << std::endl;
        file << "-----END PUBLIC KEY-----" << std::endl;
        file.close();
    }

    //validate the key files added
    AvailKeysPath paths = keyManager->getAvailKeysPathFromSystem();
    std::string testGAFile = paths.at(0);
    std::string testOpenBmcFile = paths.at(1);
    EXPECT_EQ(testOpenBmcFile, openbmcKeyFileName);
    EXPECT_EQ(testGAFile, gaKeyFileName);

    //Remove the keys and check if size is zero
    fs::remove(openbmcPath);
    fs::remove(gaPath);
    AvailKeysPath newpaths = keyManager->getAvailKeysPathFromSystem();
    EXPECT_EQ(newpaths.size(), 0);
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
