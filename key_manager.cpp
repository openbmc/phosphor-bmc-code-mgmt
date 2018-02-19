#include <fstream>
#include "key_manager.hpp"
#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <experimental/filesystem>
#include <xyz/openbmc_project/Common/error.hpp>

namespace phosphor
{
namespace software
{
namespace manager
{
using namespace phosphor::logging;
namespace fs = std::experimental::filesystem;
using InternalFailure =
        sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

constexpr auto keyTypeTag = "key_type";
constexpr auto hashFunctionTag = "hash_Function";
constexpr auto md4 = "MD4";
constexpr auto md5 = "MD5";
constexpr auto sha = "SHA";
constexpr auto sha1 = "SHA1";
constexpr auto sha224 = "SHA224";
constexpr auto sha256 = "SHA256";
constexpr auto sha384 = "SHA384";
constexpr auto sha512 = "SHA512";

constexpr auto OpenBMC = "OpenBMC";
constexpr auto GA= "GA";
constexpr auto GHE= "GHE";
constexpr auto Witherspoon= "Witherspoon";

HashType KeyManager::getHashTypeFromManifest()
{
    auto value = std::move(getValue(manifestFile, hashFunctionTag));
    return getHashTypeEnum(value);
}

KeyType KeyManager::getKeyTypeFromManifest()
{
    auto value = std::move(getValue(manifestFile, keyTypeTag));
    return getKeyTypeEnum(value);
}

HashType KeyManager::getHashTypeFromSystem()
{
    auto value = std::move(getValue(systemHashFile, hashFunctionTag));
    return getHashTypeEnum(value);
}

void KeyManager::saveHashTypeToSystem()
{
    std::string hashStr{};
    HashType hash = getHashTypeFromManifest();
    switch(hash)
    {
        case HashType::md4:
            hashStr = md4;
            break;
        case HashType::md5:
            hashStr = md5;
            break;
        case HashType::sha:
            hashStr = sha;
            break;
        case HashType::sha1:
            hashStr = sha1;
            break;
        case HashType::sha224:
            hashStr = sha224;
            break;
        case HashType::sha256:
            hashStr = sha256;
            break;
        case HashType::sha384:
            hashStr = sha384;
            break;
        case HashType::sha512:
            hashStr = sha512;
            break;
        default:
            hashStr = sha512;
    };
    fs::path path = systemHashFile;
    fs::create_directories(path.parent_path());

    std::fstream outStream(systemHashFile, std::fstream::out);
    outStream << hashFunctionTag << "=" << hashStr << "\n" ;
}

void KeyManager::savePublicKeyToSystem(std::string& srcPath)
{
    fs::path srcFileName = srcPath;
    srcFileName /= publicKeyFileName;
    if(fs::exists(srcFileName))
    {
        log<level::ERR>("Error failed to find the source file",
                        entry("SRC_KEY_FILE=%s", srcPath.c_str()));
        elog<InternalFailure>();
    }

    KeyType type = getKeyTypeFromManifest();
    std::string keyTypeStr {};
    switch(type)
    {
        case KeyType::GA:
            keyTypeStr = GA;
            break;
        case KeyType::GHE:
            keyTypeStr = GHE;
            break;
        case KeyType::Witherspoon:
            keyTypeStr = Witherspoon;
            break;
        default:
            keyTypeStr = OpenBMC;
            break;
    };

    fs::path pubPath = publicKeysPath;
    pubPath /= keyTypeStr;
    fs::create_directories(pubPath);

    fs::path destFile = pubPath;
    destFile /= publicKeyFileName;

    fs::copy_file(srcFileName, destFile);
}

AvailKeysPath KeyManager::getAvailKeysPathFromSystem()
{
    AvailKeysPath keysPath;
    fs::path pubKeyPath(publicKeysPath);
    if (!fs::is_directory(pubKeyPath))
    {
        log<level::ERR>("Public key path not found in the system");
        return keysPath;
    }

    for(const auto& p: fs::recursive_directory_iterator(pubKeyPath))
    {
        if(endsWith(p.path().string(), publicKeyFileName))
        {
            keysPath.emplace_back(p.path().string());
        }
    }

    return keysPath;
}

Value KeyManager::getValue(const std::string& file, const std::string& keyTag)
{
    Value keyValue{};

    // open file
    std::fstream stream(file, std::fstream::in);
    if (!stream.is_open())
    {
        log<level::ERR>("Error failed to open configuration file",
                        entry("CONFIG_FILE=%s", file.c_str()));
        elog<InternalFailure>();
    }

    std::string searchTag = keyTag + "=";
    auto length = searchTag.length();
    for (std::string line; std::getline(stream, line);)
    {
        if (line.compare(0, length, searchTag) == 0)
        {
            keyValue = line.substr(length);
        }
    }
    return keyValue;
}

HashType KeyManager::getHashTypeEnum(const std::string& type)
{
    if (type == md4)
    {
        return HashType::md4;
    }
    if (type == md5)
    {
        return  HashType::md5;
    }
    if (type == sha)
    {
        return  HashType::sha;
    }
    if (type == sha1)
    {
        return  HashType::sha1;
    }
    if (type == sha224)
    {
        return  HashType::sha224;
    }
    if (type == sha256)
    {
        return  HashType::sha256;
    }
    if (type == sha384)
    {
        return  HashType::sha384;
    }

    return  HashType::sha512;
}

KeyType KeyManager::getKeyTypeEnum(const std::string& type)
{
    if (type == GA)
    {
        return KeyType::GA;
    }
    if (type == GHE)
    {
        return KeyType::GHE;
    }
    if (type == Witherspoon)
    {
        return KeyType::Witherspoon;
    }
    return KeyType::OpenBMC;
}

bool KeyManager::endsWith(
        const std::string &mainStr, const std::string &toMatch)
{
    if(mainStr.size() >= toMatch.size() &&
        mainStr.compare(mainStr.size() - toMatch.size(),
                        toMatch.size(), toMatch) == 0)
    {
        return true;
    }
    return false;
}

} // namespace manager
} // namespace software
} // namespace phosphor
