#include <fstream>
#include "key_manager.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{
HashType KeyManager::getHashTypeFromManifest()
{
    return HashType::sha512;
}

KeyType KeyManager::getKeyTypeFromManifest()
{
    return KeyType::OpenBMC;
}

HashType KeyManager::getHashTypeFromSystem()
{
    return HashType::sha512;
}

void KeyManager::updatePublickKeyToSystem()
{
}

void KeyManager::updateHashTypeToSystem()
{
}

AvailableKeys KeyManager::getAllKeysFromSystem()
{
    AvailableKeys keys;
    return keys;
}

bool KeyManager::isPublicKeyFoundInManifest()
{
    return true;
}

Value KeyManager::getValue(const std::string& file, const std::string& keyTag)
{
    Value keyValue{};
    return keyValue;
}

} // namespace manager
} // namespace software
} // namespace phosphor
