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

KeyType KeyManager::getKeyTypeFromSystem()
{
    return KeyType::OpenBMC;
}

void KeyManager::updatePublickKeyToSystem()
{
}

void KeyManager::updateHashTypeToSystem()
{
}
        
void KeyManager::updateKeyTypeToSystem()
{
}

AvailableKeys KeyManager::getAllKeysFromSystem()
{
    AvailableKeys keyLocation;
    keyLocation.push_back("/etc/activationdata/Keys/OpenBmc/publicKey.pem");
    keyLocation.push_back("/etc/activationdata/Keys/GA/publicKey.pem");
    return keyLocation;
}

bool KeyManager::isPublicKeyFoundInManifest()
{
    return true;
}

} // namespace manager
} // namespace software
} // namespace phosphor
