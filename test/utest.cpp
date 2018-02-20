#include "version.hpp"
#include "signature_config.hpp"
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

class SignatureConfigTest : public testing::Test
{
    protected:

        virtual void SetUp()
        {
            char SignatureConfigDir[] = "./SignatureConfigXXXXXX";
            directory = mkdtemp(SignatureConfigDir);
            if (directory.empty())
            {
                throw std::bad_alloc();
            }
            extractPath = directory;

            signedConfPath = directory;
            signedConfPath /= "activationdata";

            fs::path manifestFilePath = extractPath;
            manifestFilePath /= "MANIFEST";

            std::ofstream manFile;
            manFile.open(manifestFilePath, std::ofstream::out);
            ASSERT_TRUE(manFile.is_open());
            manFile << "HashType=" << "RSA-SHA512" << std::endl;
            manFile << "KeyType=" << "OpenBMC" << std::endl;
            manFile.close();

            signatureConfig =
                    std::make_unique<SignatureConfig>(extractPath, signedConfPath);

        }

        virtual void TearDown()
        {
            try
            {
                fs::remove_all(directory);
            }
            catch(const std::exception& e)
            {
                std::cout << "failed to remove files" << std::endl;
            }
        }

        fs::path directory;
        fs::path extractPath;
        fs::path signedConfPath;

        std::unique_ptr<SignatureConfig> signatureConfig;
};

TEST_F(SignatureConfigTest, TestGetHashTypeFromManifest)
{
    EXPECT_EQ(signatureConfig->getHashTypeFromManifest(), "RSA-SHA512");
}

TEST_F(SignatureConfigTest, TestGetKeyTypeFromManifest)
{
    EXPECT_EQ(signatureConfig->getKeyTypeFromManifest(), "OpenBMC");
}

TEST_F(SignatureConfigTest, TestSaveHashTypeToSystem)
{
    auto hashType = signatureConfig->getHashTypeFromManifest();

    signatureConfig->saveHashTypeToSystem();

    //check if the file is copied
    fs::path hashFile{signedConfPath};
    hashFile /= "OpenBMC";
    hashFile /= HASH_FILE_NAME;

    EXPECT_EQ(fs::exists(hashFile), true);

    fs::remove(hashFile);
}


TEST_F(SignatureConfigTest, TestSavePublicKeyToSystem)
{
    fs::path tmpPubKeyFile{extractPath};
    tmpPubKeyFile /= PUBLICKEY_FILE_NAME;

    std::ofstream file;
    file.open(tmpPubKeyFile.string(), std::ofstream::out);
    ASSERT_TRUE(file.is_open());
    file << "-----BEGIN PUBLIC KEY-----" << std::endl;
    file << "a133#2a&*91ad~}" << std::endl;
    file << "-----END PUBLIC KEY-----" << std::endl;
    file.close();

    signatureConfig->savePublicKeyToSystem();

    //check if the file is copied
    fs::path pubKeyFile{signedConfPath};
    pubKeyFile /= "OpenBMC";
    pubKeyFile /= PUBLICKEY_FILE_NAME;

    EXPECT_EQ(fs::exists(pubKeyFile), true);

    fs::remove(tmpPubKeyFile);
    fs::remove(pubKeyFile);
}

TEST_F(SignatureConfigTest, TestGetKeyTypesFromSystem)
{
    //create key file
    fs::path keypath{signedConfPath};
    keypath /= "OpenBMC";
    fs::create_directories(keypath);
    keypath /= PUBLICKEY_FILE_NAME;

    std::ofstream keyfile;
    keyfile.open(keypath, std::ofstream::out);
    ASSERT_TRUE(keyfile.is_open());
    keyfile << "Dummy" << std::endl;
    keyfile.close();

    //create hash file
    fs::path hashpath{signedConfPath};
    hashpath /= "OpenBMC";
    hashpath /= HASH_FILE_NAME;
    std::ofstream hashfile;
    hashfile.open(hashpath, std::ofstream::out);
    ASSERT_TRUE(hashfile.is_open());
    hashfile << "Dummy" << std::endl;
    hashfile.close();

    AvailableKeyTypes keys = signatureConfig->getAvailableKeyTypesFromSystem();
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
