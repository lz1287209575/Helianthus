#ifndef TINY_AES_H
#define TINY_AES_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

#define AES_BLOCKLEN 16

    struct AES_ctx
    {
        uint8_t RoundKey[240];
        uint8_t Iv[AES_BLOCKLEN];
    };

    void AES_init_ctx_iv(struct AES_ctx* ctx, const uint8_t* key, const uint8_t* iv);
    void AES_CTR_xcrypt_buffer(struct AES_ctx* ctx, uint8_t* buf, size_t length);

#ifdef __cplusplus
}
#endif

#endif  // TINY_AES_H
