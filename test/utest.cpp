#include "version.hpp"
#include <gtest/gtest.h>
#include <experimental/filesystem>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <openssl/sha.h>
#include "image_verify.hpp"

using namespace phosphor::software::manager;
namespace fs = std::experimental::filesystem;
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
    char mdString[SHA512_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
    {
        snprintf(&mdString[i * 2], 3, "%02x", (unsigned int)digest[i]);
    }
    std::string hexId = std::string(mdString);
    hexId = hexId.substr(0, 8);
    EXPECT_EQ(Version::getId(version), hexId);
}

class SignatureTest : public testing::Test
{
    protected:
        virtual void SetUp()
        {
            _extractPath = "images";
            _signedConfPath = "conf";

            std::cout << "SETUP " << std::endl;
            _signature = std::make_unique<Signature>(
                            _extractPath, _signedConfPath);
            //remove if already existing
            std::system("rm -rf ./conf/");
            std::system("rm -rf ./images/");

            std::system("mkdir ./images");
            std::system("mkdir -p ./conf/OpenBMC/");

            std::system("echo \"HashType=RSA-SHA256\" > conf/OpenBMC/hashfunc");
            std::system("echo \"HashType=RSA-SHA256\" > images/MANIFEST");
            std::system("echo \"KeyType=OpenBMC\" >> images/MANIFEST");
            std::system("echo \"image-kernel file \" > images/image-kernel");
            std::system("echo \"image-rofs file \" > images/image-rofs");
            std::system("echo \"image-rwfs file \" > images/image-rwfs");
            std::system("echo \"image-u-boot file \" > images/image-u-boot");
            std::system("openssl genrsa  -out images/private.pem 2048");
            std::system("openssl rsa -in images/private.pem -outform PEM "
                "-pubout -out images/publickey");
            std::system("cp images/publickey conf/OpenBMC/publickey");
            std::system("openssl dgst -sha256 -sign images/private.pem "
                "-out images/image-kernel.sig images/image-kernel");
            std::system("openssl dgst -sha256 -sign images/private.pem "
                "-out images/MANIFEST.sig images/MANIFEST");
            std::system("openssl dgst -sha256 -sign images/private.pem -out "
                "images/image-rofs.sig images/image-rofs");
            std::system("openssl dgst -sha256 -sign images/private.pem -out "
                "images/image-rwfs.sig images/image-rwfs");
            std::system("openssl dgst -sha256 -sign images/private.pem -out "
                "images/image-u-boot.sig images/image-u-boot");
            std::system("openssl dgst -sha256 -sign images/private.pem -out "
                "images/publickey.sig images/publickey");
        }
        virtual void TearDown()
        {
            std::system("rm -rf ./conf/");
            std::system("rm -rf ./images/");
            std::system("rmdir ./conf/");
            std::system("rmdir ./images/");
        }

        std::unique_ptr<Signature> _signature;
        fs::path _extractPath;
        fs::path _signedConfPath;
};

/** @brief Test for sucess scenario*/
TEST_F(SignatureTest, TestSignatureVerify)
{
    EXPECT_EQ(_signature->verify(), true);
}

/** @brief Test for failure scenario*/
TEST_F(SignatureTest, TestSignatureVerifyFail)
{
    //corrupt the image-kernel.sig file and ensure that verification fails
    std::system("echo \"dummy data\" > images/image-kernel.sig");
    EXPECT_EQ(_signature->verify(), false);
}
