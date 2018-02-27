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
        static constexpr auto opensslCmd = "openssl dgst -sha256 -sign ";
    protected:
        void command(const std::string& cmd)
        {
            std::cout << "COMMAND " << cmd << std::endl;
            std::system(cmd.c_str());
        }
        virtual void SetUp()
        {
            command("rm -rf extract*");
            command("rm -rf conf*");
            char tmpDir[] = "extractXXXXXX";
            std::string _imageDir = mkdtemp(tmpDir);

            char tmpConfDir[] = "confXXXXXX";
            std::string _confDir = mkdtemp(tmpConfDir);

            _extractPath = _imageDir;
            _extractPath /=  "images";

            _signedConfPath = _confDir;
            _signedConfPath /= "conf";

            _signedConfOpenBMCPath = _confDir;
            _signedConfOpenBMCPath /= "conf";
            _signedConfOpenBMCPath /= "OpenBMC";

            std::cout << "SETUP " << std::endl;

            command("mkdir " + _extractPath.string());
            command("mkdir " + _signedConfPath.string());
            command("mkdir " + _signedConfOpenBMCPath.string());

            std::string hashFile = _signedConfOpenBMCPath.string() +
                                   "/hashfunc";
            command("echo \"HashType=RSA-SHA256\" > " + hashFile);

            std::string manifestFile = _extractPath.string() + "/" + "MANIFEST";
            command("echo \"HashType=RSA-SHA256\" > " + manifestFile);
            command("echo \"KeyType=OpenBMC\" >> " + manifestFile);

            std::string kernelFile = _extractPath.string() + "/" +
                                     "image-kernel";
            command("echo \"image-kernel file \" > " + kernelFile);

            std::string rofsFile = _extractPath.string() + "/" + "image-rofs";
            command("echo \"image-rofs file \" > " + rofsFile);

            std::string rwfsFile = _extractPath.string() + "/" + "image-rwfs";
            command("echo \"image-rwfs file \" > " + rwfsFile);

            std::string ubootFile = _extractPath.string() + "/" +
                                    "image-u-boot";
            command("echo \"image-u-boot file \" > " + ubootFile);

            std::string pkeyFile = _extractPath.string() + "/" + "private.pem";
            command("openssl genrsa  -out " + pkeyFile + " 2048");

            std::string pubkeyFile = _extractPath.string() + "/" + "publickey";
            command("openssl rsa -in " + pkeyFile + " -outform PEM " +
                    "-pubout -out " + pubkeyFile);

            std::string pubKeyConfFile =
                _signedConfOpenBMCPath.string() + "/" + "publickey";
            command("cp " + pubkeyFile + " " + _signedConfOpenBMCPath.string());
            command(opensslCmd + pkeyFile + " -out " + kernelFile + ".sig "  +
                    kernelFile);

            command(opensslCmd + pkeyFile  +  " -out " + manifestFile +
                    ".sig " +  manifestFile);
            command(opensslCmd + pkeyFile  +  " -out " + rofsFile + ".sig " +
                    rofsFile);
            command(opensslCmd + pkeyFile  +  " -out " + rwfsFile + ".sig " +
                    rwfsFile);
            command(opensslCmd + pkeyFile  +  " -out " + ubootFile + ".sig " +
                    ubootFile);
            command(opensslCmd + pkeyFile  +  " -out " + pubkeyFile + ".sig " +
                    pubkeyFile);

            _signature = std::make_unique<Signature>(
                             _extractPath, _signedConfPath);
        }
        virtual void TearDown()
        {
            try
            {
                std::cout << "CAME TO TEAR DOWN " << std::endl;
                command("rm -rf extract*");
                command("rm -rf conf*");
            }
            catch (const std::exception& e)
            {
                std::cerr << "Failed to remove files " << std::endl;
            }
        }

        std::unique_ptr<Signature> _signature;
        fs::path _extractPath;
        fs::path _signedConfPath;
        fs::path _signedConfOpenBMCPath;
};

/** @brief Test for sucess scenario*/
TEST_F(SignatureTest, TestSignatureVerify)
{
    EXPECT_TRUE(_signature->verify());
}

/** @brief Test failure scenario with corrupted signature file*/
TEST_F(SignatureTest, TestCorruptSignatureFile)
{
    //corrupt the image-kernel.sig file and ensure that verification fails
    std::string kernelFile = _extractPath.string() + "/" + "image-kernel";
    command("echo \"dummy data\" > " + kernelFile + ".sig ");
    EXPECT_FALSE(_signature->verify());
}

/** @brief Test failure scenario with no public key in the image*/
TEST_F(SignatureTest, TestNoPublicKeyInImage)
{
    //Remove publickey file from the image and ensure that verify fails
    std::string pubkeyFile = _extractPath.string() + "/" + "publickey";
    command("rm " + pubkeyFile);
    EXPECT_FALSE(_signature->verify());
}

/** @brief Test failure scenario with invalid hash function value*/
TEST_F(SignatureTest, TestInvalidHashValue)
{
    //Change the hashfunc value and ensure that verification fails
    std::string hashFile = _signedConfOpenBMCPath.string() +
                           "/hashfunc";
    command("echo \"HashType=md5\" > " + hashFile);
    EXPECT_FALSE(_signature->verify());
}

/** @brief Test for failure scenario with no config file in system*/
TEST_F(SignatureTest, TestNoConfigFileInSystem)
{
    //Remove the conf folder in the system and ensure that verify fails
    command("rm -rf " + _signedConfOpenBMCPath.string());
    EXPECT_FALSE(_signature->verify());
}
