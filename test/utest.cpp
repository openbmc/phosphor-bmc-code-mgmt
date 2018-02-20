#include "version.hpp"
#include "key_manager.hpp"
#include <gtest/gtest.h>
#include <experimental/filesystem>
#include <stdlib.h>
#include <fstream>
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
            publicKeyPath = _directory + "/" + "key";

            keyManager = std::make_unique<KeyManager>(manifestFilePath,
                                systemHashFilePath, publicKeyPath);

            std::ofstream manFile;
            manFile.open(manifestFilePath, std::ofstream::out);
            ASSERT_TRUE(manFile.is_open());
            manFile << "HashType=" << "SHA512" << std::endl;
            manFile << "KeyType=" << "OpenBMC" << std::endl;
            manFile.close();
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
    EXPECT_EQ(keyManager->getKeyTypeFromManifest(), "OpenBMC");
}

TEST_F(KeyManagerTest, TestGetHashTypeFromManifest)
{
    EXPECT_EQ(keyManager->getHashTypeFromManifest(), "SHA512");
}

TEST_F(KeyManagerTest, TestSavePublicKeyToSystem)
{
    fs::path path = _directory;
    path /= publicKeyFileName;

    std::ofstream file;
    file.open(path.string(), std::ofstream::out);
    ASSERT_TRUE(file.is_open());
    file << "-----BEGIN PUBLIC KEY-----" << std::endl;
    file << "a133#2a&*91ad~}" << std::endl;
    file << "-----END PUBLIC KEY-----" << std::endl;
    file.close();

    keyManager->savePublicKeyToSystem(path.string());

    //check if the file is copied
    fs::path pubpath = publicKeyPath;
    pubpath /= "OpenBMC";
    pubpath /= publicKeyFileName;
    EXPECT_EQ(fs::exists(pubpath), true);
}

TEST_F(KeyManagerTest, TestGetAllKeyFilePath)
{
    //create public key file
    fs::path pubpath = publicKeyPath;
    pubpath /= "OpenBMC";
    fs::create_directories(pubpath);
    pubpath /= publicKeyFileName;
    std::ofstream keyfile;
    keyfile.open(pubpath.string(), std::ofstream::out);
    keyfile << "-----BEGIN PUBLIC KEY-----" << std::endl;
    keyfile << "a133#2a&*91ad~}" << std::endl;
    keyfile << "-----END PUBLIC KEY-----" << std::endl;
    keyfile.close();

    KeyFileList keyFile = keyManager->getAllKeyFilePath();
    EXPECT_EQ(keyFile.size(), 1);
}

TEST_F(KeyManagerTest, TestGetAllHashFilePath)
{
    //create hash key file
    fs::path hashpath = systemHashFilePath;
    hashpath /= "OpenBMC";
    fs::create_directories(hashpath);
    hashpath /= hashFileName;
    std::ofstream hashfile;
    hashfile.open(hashpath.string(), std::ofstream::out);
    hashfile << "HashType=" << "SHA512" << std::endl;
    hashfile.close();

    HashFileList hashFiles = keyManager->getAllHashFilePath();
    EXPECT_EQ(hashFiles.size(), 1);
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
