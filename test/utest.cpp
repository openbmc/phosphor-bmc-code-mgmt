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
using namespace phosphor::software::image;

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
            _extractPath = _directory;

            _signedConfPath = _directory;
            _signedConfPath /= "activationdata";


            keyManager =
                    std::make_unique<KeyManager>(_extractPath, _signedConfPath);

            fs::path manifestFilePath = _extractPath;
            manifestFilePath /= "MANIFEST";

            std::ofstream manFile;
            manFile.open(manifestFilePath, std::ofstream::out);
            ASSERT_TRUE(manFile.is_open());
            manFile << "HashType=" << "RSA-SHA512" << std::endl;
            manFile << "KeyType=" << "OpenBMC" << std::endl;
            manFile.close();
        }

        virtual void TearDown()
        {
            try
            {
                fs::remove_all(_directory);
            }
            catch(const std::exception& e)
            {
                std::cout << "failed to remove files" << std::endl;
            }
        }

        fs::path _directory;
        fs::path _extractPath;
        fs::path _signedConfPath;

        std::unique_ptr<KeyManager> keyManager;
};

TEST_F(KeyManagerTest, TestGetHashTypeFromManifest)
{
    EXPECT_EQ(keyManager->getHashTypeFromManifest(), "RSA-SHA512");
}

TEST_F(KeyManagerTest, TestGetKeyTypeFromManifest)
{
    EXPECT_EQ(keyManager->getKeyTypeFromManifest(), "OpenBMC");
}

TEST_F(KeyManagerTest, TestSaveHashTypeToSystem)
{
    auto hashType = keyManager->getHashTypeFromManifest();

    keyManager->saveHashTypeToSystem();

    //check if the file is copied
    fs::path hashFile{_signedConfPath};
    hashFile /= "OpenBMC";
    hashFile /= HASH_FILE_NAME;

    EXPECT_EQ(fs::exists(hashFile), true);

    fs::remove(hashFile);
}


TEST_F(KeyManagerTest, TestSavePublicKeyToSystem)
{
    fs::path tmpPubKeyFile{_extractPath};
    tmpPubKeyFile /= PUBLICKEY_FILE_NAME;

    std::ofstream file;
    file.open(tmpPubKeyFile.string(), std::ofstream::out);
    ASSERT_TRUE(file.is_open());
    file << "-----BEGIN PUBLIC KEY-----" << std::endl;
    file << "a133#2a&*91ad~}" << std::endl;
    file << "-----END PUBLIC KEY-----" << std::endl;
    file.close();

    keyManager->savePublicKeyToSystem();

    //check if the file is copied
    fs::path pubKeyFile{_signedConfPath};
    pubKeyFile /= "OpenBMC";
    pubKeyFile /= PUBLICKEY_FILE_NAME;

    EXPECT_EQ(fs::exists(pubKeyFile), true);

    fs::remove(tmpPubKeyFile);
    fs::remove(pubKeyFile);
}

TEST_F(KeyManagerTest, TestGetKeyTypesFromSystem)
{
    //create key file
    fs::path keypath{_signedConfPath};
    keypath /= "OpenBMC";
    fs::create_directories(keypath);
    keypath /= PUBLICKEY_FILE_NAME;
   
    std::ofstream keyfile;
    keyfile.open(keypath, std::ofstream::out);
    ASSERT_TRUE(keyfile.is_open());
    keyfile << "Dummy" << std::endl;
    keyfile.close();

    //create hash file
    fs::path hashpath{_signedConfPath};
    hashpath /= "OpenBMC";
    hashpath /= HASH_FILE_NAME;
    std::ofstream hashfile;
    hashfile.open(hashpath, std::ofstream::out);
    ASSERT_TRUE(hashfile.is_open());
    hashfile << "Dummy" << std::endl;
    hashfile.close();

    AvailableKeyTypes keys = keyManager->getAvailableKeyTypesFromSystem();
    EXPECT_EQ(keys.size(), 1);
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
