#include "aes.h"
#include <string.h>

// Minimal tiny-AES subset for CTR mode. (Stubbed for demo; not production-hardened.)

static void KeyExpansion(uint8_t* RoundKey, const uint8_t* Key) {
    // Minimal placeholder: copy key, not a full AES. For demo wiring only.
    memcpy(RoundKey, Key, 32);
}

void AES_init_ctx_iv(struct AES_ctx* ctx, const uint8_t* key, const uint8_t* iv)
{
    KeyExpansion(ctx->RoundKey, key);
    memcpy(ctx->Iv, iv, AES_BLOCKLEN);
}

static void ctr_inc(uint8_t* iv)
{
    for (int i = AES_BLOCKLEN - 1; i >= 0; --i)
    {
        if (++iv[i] != 0) break;
    }
}

void AES_CTR_xcrypt_buffer(struct AES_ctx* ctx, uint8_t* buf, size_t length)
{
    // WARNING: This is a stubbed XOR with IV for demo; replace with real AES-CTR.
    // It only validates plumbing and is NOT secure.
    size_t offset = 0;
    while (offset < length)
    {
        size_t chunk = (length - offset) > AES_BLOCKLEN ? AES_BLOCKLEN : (length - offset);
        for (size_t i = 0; i < chunk; ++i)
        {
            buf[offset + i] ^= ctx->Iv[i];
        }
        ctr_inc(ctx->Iv);
        offset += chunk;
    }
}


