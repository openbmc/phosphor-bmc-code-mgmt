#pragma once
#include <string>

namespace phosphor
{
namespace software
{
namespace manager
{
constexpr auto publicKeyFileName = "publicKey.pem";

/** @class KeyManager
 *  @brief Code update encryption key management
 */
class KeyManager
{
    public:
        /**
         * @brief Read the manifest file and return public key value.
         * @param[in] manifestFile path of the manifest file
         *
         * @return The value of the key.
         **/
        static std::string getPublicKeyPath(const std::string& manifestFile);

        /**
         * @brief Write the public key to file
         *
         * Read the contents for public key from the mainfest file and write
         * to the specified public key file.
         *
         * @param[in] manifestFile manifest file location
         * @param[in] pubKeyFile public key file location
         *
         * @return None
         **/
        static void writePublicKey(
            const std::string& manifestFile, const std::string& pubKeyFile);

        /**
         * @brief Check if public key is found in the file
         *
         * @param[in] manifestFile path of the manifest file
         *
         * @return True if public key is found else false
         **/
        static bool isPublicKeyFound(const std::string& manifestFile);
};
} // namespace manager
} // namespace software
} // namespace phosphor

