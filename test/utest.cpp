#include "image_verify.hpp"
#include "utils.hpp"
#include "version.hpp"

#include <openssl/sha.h>
#include <stdlib.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

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

    file << "version=" << version << "\n";
    file << "purpose=" << purpose << "\n";
    file.close();

    EXPECT_EQ(Version::getValue(manifestFilePath, "version"), version);
    EXPECT_EQ(Version::getValue(manifestFilePath, "purpose"), purpose);
}

TEST_F(VersionTest, TestGetValueWithCRLF)
{
    auto manifestFilePath = _directory + "/" + "MANIFEST";
    auto version = "test-version";
    auto purpose = "BMC";

    std::ofstream file;
    file.open(manifestFilePath, std::ofstream::out);
    ASSERT_TRUE(file.is_open());

    file << "version=" << version << "\r\n";
    file << "purpose=" << purpose << "\r\n";
    file.close();

    EXPECT_EQ(Version::getValue(manifestFilePath, "version"), version);
    EXPECT_EQ(Version::getValue(manifestFilePath, "purpose"), purpose);
}

TEST_F(VersionTest, TestGetVersionWithQuotes)
{
    auto releasePath = _directory + "/" + "os-release";
    auto version = "1.2.3-test-version";

    std::ofstream file;
    file.open(releasePath, std::ofstream::out);
    ASSERT_TRUE(file.is_open());

    file << "VERSION_ID=\"" << version << "\"\n";
    file.close();

    EXPECT_EQ(Version::getBMCVersion(releasePath), version);
}

TEST_F(VersionTest, TestGetVersionWithoutQuotes)
{
    auto releasePath = _directory + "/" + "os-release";
    auto version = "9.88.1-test-version";

    std::ofstream file;
    file.open(releasePath, std::ofstream::out);
    ASSERT_TRUE(file.is_open());

    file << "VERSION_ID=" << version << "\n";
    file.close();

    EXPECT_EQ(Version::getBMCVersion(releasePath), version);
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

TEST_F(VersionTest, TestGetExtendedVersion)
{
    auto releasePath = _directory + "/" + "os-release";
    auto ExtendedVersion = "9.88.1-test-ExtendedVersion";

    std::ofstream file;
    file.open(releasePath, std::ofstream::out);
    ASSERT_TRUE(file.is_open());

    file << "EXTENDED_VERSION=" << ExtendedVersion << "\n";
    file.close();

    EXPECT_EQ(Version::getBMCExtendedVersion(releasePath), ExtendedVersion);
}

class SignatureTest : public testing::Test
{
    static constexpr auto opensslCmd = "openssl dgst -sha256 -sign ";
    static constexpr auto testPath = "/tmp/_testSig";

  protected:
    void command(const std::string& cmd)
    {
        auto val = std::system(cmd.c_str());
        if (val)
        {
            std::cout << "COMMAND Error: " << val << std::endl;
        }
    }
    virtual void SetUp()
    {
        // Create test base directory.
        fs::create_directories(testPath);

        // Create unique temporary path for images
        std::string tmpDir(testPath);
        tmpDir += "/extractXXXXXX";
        std::string imageDir = mkdtemp(const_cast<char*>(tmpDir.c_str()));

        // Create unique temporary configuration path
        std::string tmpConfDir(testPath);
        tmpConfDir += "/confXXXXXX";
        std::string confDir = mkdtemp(const_cast<char*>(tmpConfDir.c_str()));

        extractPath = imageDir;
        extractPath /= "images";

        signedConfPath = confDir;
        signedConfPath /= "conf";

        signedConfOpenBMCPath = confDir;
        signedConfOpenBMCPath /= "conf";
        signedConfOpenBMCPath /= "OpenBMC";

        std::cout << "SETUP " << std::endl;

        command("mkdir " + extractPath.string());
        command("mkdir " + signedConfPath.string());
        command("mkdir " + signedConfOpenBMCPath.string());

        std::string hashFile = signedConfOpenBMCPath.string() + "/hashfunc";
        command("echo \"HashType=RSA-SHA256\" > " + hashFile);

        std::string manifestFile = extractPath.string() + "/" + "MANIFEST";
        command("echo \"HashType=RSA-SHA256\" > " + manifestFile);
        command("echo \"KeyType=OpenBMC\" >> " + manifestFile);

        std::string kernelFile = extractPath.string() + "/" + "image-kernel";
        command("echo \"image-kernel file \" > " + kernelFile);

        std::string rofsFile = extractPath.string() + "/" + "image-rofs";
        command("echo \"image-rofs file \" > " + rofsFile);

        std::string rwfsFile = extractPath.string() + "/" + "image-rwfs";
        command("echo \"image-rwfs file \" > " + rwfsFile);

        std::string ubootFile = extractPath.string() + "/" + "image-u-boot";
        command("echo \"image-u-boot file \" > " + ubootFile);

        std::string pkeyFile = extractPath.string() + "/" + "private.pem";
        command("openssl genrsa  -out " + pkeyFile + " 2048");

        std::string pubkeyFile = extractPath.string() + "/" + "publickey";
        command("openssl rsa -in " + pkeyFile + " -outform PEM " +
                "-pubout -out " + pubkeyFile);

        command("cp " + pubkeyFile + " " + signedConfOpenBMCPath.string());
        command(opensslCmd + pkeyFile + " -out " + kernelFile + ".sig " +
                kernelFile);

        command(opensslCmd + pkeyFile + " -out " + manifestFile + ".sig " +
                manifestFile);
        command(opensslCmd + pkeyFile + " -out " + rofsFile + ".sig " +
                rofsFile);
        command(opensslCmd + pkeyFile + " -out " + rwfsFile + ".sig " +
                rwfsFile);
        command(opensslCmd + pkeyFile + " -out " + ubootFile + ".sig " +
                ubootFile);
        command(opensslCmd + pkeyFile + " -out " + pubkeyFile + ".sig " +
                pubkeyFile);

#ifdef WANT_SIGNATURE_FULL_VERIFY
        std::string fullFile = extractPath.string() + "/" + "image-full";
        command("cat " + kernelFile + ".sig " + rofsFile + ".sig " + rwfsFile +
                ".sig " + ubootFile + ".sig " + manifestFile + ".sig " +
                pubkeyFile + ".sig > " + fullFile);
        command(opensslCmd + pkeyFile + " -out " + fullFile + ".sig " +
                fullFile);
#endif

        signature = std::make_unique<Signature>(extractPath, signedConfPath);
    }
    virtual void TearDown()
    {
        command("rm -rf " + std::string(testPath));
    }

    std::unique_ptr<Signature> signature;
    fs::path extractPath;
    fs::path signedConfPath;
    fs::path signedConfOpenBMCPath;
};

/** @brief Test for success scenario*/
TEST_F(SignatureTest, TestSignatureVerify)
{
    EXPECT_TRUE(signature->verify());
}

/** @brief Test failure scenario with corrupted signature file*/
TEST_F(SignatureTest, TestCorruptSignatureFile)
{
    // corrupt the image-kernel.sig file and ensure that verification fails
    std::string kernelFile = extractPath.string() + "/" + "image-kernel";
    command("echo \"dummy data\" > " + kernelFile + ".sig ");
    EXPECT_FALSE(signature->verify());
}

/** @brief Test failure scenario with no public key in the image*/
TEST_F(SignatureTest, TestNoPublicKeyInImage)
{
    // Remove publickey file from the image and ensure that verify fails
    std::string pubkeyFile = extractPath.string() + "/" + "publickey";
    command("rm " + pubkeyFile);
    EXPECT_FALSE(signature->verify());
}

/** @brief Test failure scenario with invalid hash function value*/
TEST_F(SignatureTest, TestInvalidHashValue)
{
    // Change the hashfunc value and ensure that verification fails
    std::string hashFile = signedConfOpenBMCPath.string() + "/hashfunc";
    command("echo \"HashType=md5\" > " + hashFile);
    EXPECT_FALSE(signature->verify());
}

/** @brief Test for failure scenario with no config file in system*/
TEST_F(SignatureTest, TestNoConfigFileInSystem)
{
    // Remove the conf folder in the system and ensure that verify fails
    command("rm -rf " + signedConfOpenBMCPath.string());
    EXPECT_FALSE(signature->verify());
}

class FileTest : public testing::Test
{
  protected:
    std::string readFile(fs::path path)
    {
        std::ifstream f(path, std::ios::in);
        const auto sz = fs::file_size(path);
        std::string result(sz, '\0');
        f.read(result.data(), sz);

        return result;
    }

    void command(const std::string& cmd)
    {
        auto val = std::system(cmd.c_str());
        if (val)
        {
            std::cout << "COMMAND Error: " << val << std::endl;
        }
    }

    virtual void SetUp()
    {
        // Create test base directory.
        tmpDir = fs::temp_directory_path() / "testFileXXXXXX";
        if (!mkdtemp(tmpDir.data()))
        {
            throw "Failed to create tmp dir";
        }

        std::string file1 = tmpDir + "/file1";
        std::string file2 = tmpDir + "/file2";
        command("echo \"File Test1\n\n\" > " + file1);
        command("echo \"FileTe st2\n\nte st2\" > " + file2);

        srcFiles.push_back(file1);
        srcFiles.push_back(file2);
    }

    virtual void TearDown()
    {
        fs::remove_all(tmpDir);
    }

    std::vector<std::string> srcFiles;
    std::string tmpDir;
};

TEST_F(FileTest, TestMergeFiles)
{
    std::string retFile = tmpDir + "/retFile";
    for (auto file : srcFiles)
    {
        command("cat " + file + " >> " + retFile);
    }

    std::string dstFile = tmpDir + "/dstFile";
    utils::mergeFiles(srcFiles, dstFile);

    ASSERT_NE(fs::file_size(retFile), static_cast<uintmax_t>(-1));
    ASSERT_NE(fs::file_size(dstFile), static_cast<uintmax_t>(-1));
    ASSERT_EQ(fs::file_size(retFile), fs::file_size(dstFile));

    std::string ssRetFile = readFile(fs::path(retFile));
    std::string ssDstFile = readFile(fs::path(dstFile));
    ASSERT_EQ(ssRetFile, ssDstFile);
}

TEST(ExecTest, TestConstructArgv)
{
    auto name = "/bin/ls";
    auto arg1 = "-a";
    auto arg2 = "-l";
    auto arg3 = "-t";
    auto arg4 = "-rS";
    auto argV = utils::internal::constructArgv(name, arg1, arg2, arg3, arg4);
    char** charArray = argV.data();
    EXPECT_EQ(argV.size(), 6);
    EXPECT_EQ(charArray[0], name);
    EXPECT_EQ(charArray[1], arg1);
    EXPECT_EQ(charArray[2], arg2);
    EXPECT_EQ(charArray[3], arg3);
    EXPECT_EQ(charArray[4], arg4);
    EXPECT_EQ(charArray[5], nullptr);

    name = "/usr/bin/du";
    argV = utils::internal::constructArgv(name);
    charArray = argV.data();
    EXPECT_EQ(argV.size(), 2);
    EXPECT_EQ(charArray[0], name);
    EXPECT_EQ(charArray[1], nullptr);

    name = "/usr/bin/hexdump";
    arg1 = "-C";
    arg2 = "/path/to/filename";
    argV = utils::internal::constructArgv(name, arg1, arg2);
    charArray = argV.data();
    EXPECT_EQ(argV.size(), 4);
    EXPECT_EQ(charArray[0], name);
    EXPECT_EQ(charArray[1], arg1);
    EXPECT_EQ(charArray[2], arg2);
    EXPECT_EQ(charArray[3], nullptr);
}
