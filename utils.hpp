#pragma once

// With OpenSSL 1.1.0, some functions were deprecated. Need to abstract them
// to make the code backward compatible with older OpenSSL veresions.
// Reference: https://wiki.openssl.org/index.php/OpenSSL_1.1.0_Changes
#if OPENSSL_VERSION_NUMBER < 0x10100000L

#include <openssl/evp.h>

extern "C" {
EVP_MD_CTX* EVP_MD_CTX_new(void);
void EVP_MD_CTX_free(EVP_MD_CTX* ctx);
}

#endif // OPENSSL_VERSION_NUMBER < 0x10100000L

#include "config.h"

#include <string>
#include <map>
#include <sdbusplus/server.hpp>

namespace utils
{

/**
 * @brief Get the bus service
 *
 * @return the bus service as a string
 **/
std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface);

} // namespace utils
