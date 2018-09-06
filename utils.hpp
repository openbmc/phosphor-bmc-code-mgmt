#if OPENSSL_VERSION_NUMBER < 0x10100000L

#include <openssl/evp.h>

EVP_MD_CTX* EVP_MD_CTX_new(void);
void EVP_MD_CTX_free(EVP_MD_CTX* ctx);

#endif // OPENSSL_VERSION_NUMBER < 0x10100000L
