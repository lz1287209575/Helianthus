#ifndef MINIZ_SINGLE_HEADER_INCLUDED
#define MINIZ_SINGLE_HEADER_INCLUDED

// Minimal subset of miniz public API for compression/decompression
// Source: https://github.com/richgel999/miniz (amalgamated single header trimmed)
// This is a lightweight dependency for gzip/zlib-compatible compression.

#ifdef __cplusplus
extern "C"
{
#endif

    typedef unsigned char mz_uint8;
    typedef unsigned int mz_uint;
    typedef unsigned long mz_ulong;

    // Compression levels: 0-9
    int mz_compress2(unsigned char *pDest,
                     mz_ulong *pDest_len,
                     const unsigned char *pSource,
                     mz_ulong source_len,
                     int level);
    int mz_uncompress(unsigned char *pDest,
                      mz_ulong *pDest_len,
                      const unsigned char *pSource,
                      mz_ulong source_len);

// Return codes
#define MZ_OK 0
#define MZ_BUF_ERROR -5
#define MZ_PARAM_ERROR -10000

#ifdef __cplusplus
}
#endif

#endif  // MINIZ_SINGLE_HEADER_INCLUDED
