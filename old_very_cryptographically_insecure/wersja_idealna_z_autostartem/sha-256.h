/*
 *   SHA-256 implementation.
 *
 *   Copyright (c) 2010,2014,2018,2021 by Alain Mosnier <alain.mosnier@gmail.com>
 *   This software is placed in the public domain.
 *
 *   This code is primarily designed for use in environments where the standard C
 *   library is not available, or where a C compiler is not available.
 */
#ifndef _SHA_256_H_
#define _SHA_256_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHA256_DIGEST_SIZE 32

/*
 *   The context for a SHA-256 computation.
 */
typedef struct sha256_context_ {
    uint64_t  total_size;
    uint32_t  hash[8];
    uint8_t   buffer[64];
    uint32_t  buf_size;
    uint32_t  state[8];
} sha256_context;

/*
 *   Initializes a SHA-256 context.
 *
 *   Parameters:
 *      ctx: the context to initialize.
 */
void sha256_init(sha256_context *ctx);

/*
 *   Updates a SHA-256 context with a portion of the message.
 *
 *   Parameters:
 *      ctx: the context to update.
 *      msg: the message.
 *      msg_size: the size of the message.
 */
void sha256_update(sha256_context *ctx, const uint8_t *msg, uint32_t msg_size);

/*
 *   Computes the SHA-256 digest.
 *
 *   Parameters:
 *      ctx: the context.
 *      digest: a buffer to hold the 32-byte digest.
 */
void sha256_final(sha256_context *ctx, uint8_t *digest);


#if !defined(SHA256_NO_IMPLEMENTATION)

#define S0(x) ((((x) >> 2) | ((x) << 30)) ^ (((x) >> 13) | ((x) << 19)) ^ (((x) >> 22) | ((x) << 10)))
#define S1(x) ((((x) >> 6) | ((x) << 26)) ^ (((x) >> 11) | ((x) << 21)) ^ (((x) >> 25) | ((x) << 7)))
#define s0(x) ((((x) >> 7) | ((x) << 25)) ^ (((x) >> 18) | ((x) << 14)) ^ ((x) >> 3))
#define s1(x) ((((x) >> 17) | ((x) << 15)) ^ (((x) >> 19) | ((x) << 13)) ^ ((x) >> 10))

#define Ch(x, y, z) ((x & y) ^ (~x & z))
#define Maj(x, y, z) ((x & y) ^ (x & z) ^ (y & z))

static const uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void sha256_transform(sha256_context *ctx, const uint8_t *block)
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    int i;
    
    for (i = 0; i < 16; i++)
    {
        w[i] =  (((uint32_t)block[i*4+0]) << 24) |
                (((uint32_t)block[i*4+1]) << 16) |
                (((uint32_t)block[i*4+2]) << 8) |
                ( (uint32_t)block[i*4+3]);
    }
    
    for (; i < 64; i++)
        w[i] = s1(w[i-2]) + w[i-7] + s0(w[i-16]) + w[i-15];
        
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];
    
    for (i = 0; i < 64; i++)
    {
        uint32_t t1, t2;
        
        t1 = h + S1(e) + Ch(e,f,g) + k[i] + w[i];
        t2 = S0(a) + Maj(a,b,c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void sha256_init(sha256_context *ctx)
{
    ctx->total_size = 0;
    ctx->buf_size = 0;
    
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

void sha256_update(sha256_context *ctx, const uint8_t *msg, uint32_t msg_size)
{
    if (ctx->buf_size > 0)
    {
        uint32_t missing = 64 - ctx->buf_size;
        
        if (msg_size < missing)
        {
            memcpy(ctx->buffer + ctx->buf_size, msg, msg_size);
            ctx->buf_size += msg_size;
            return;
        }

        memcpy(ctx->buffer + ctx->buf_size, msg, missing);
        sha256_transform(ctx, ctx->buffer);
        msg += missing;
        msg_size -= missing;
        ctx->buf_size = 0;
    }
    
    while (msg_size >= 64)
    {
        sha256_transform(ctx, msg);
        msg += 64;
        msg_size -= 64;
    }
    
    if (msg_size > 0)
    {
        memcpy(ctx->buffer, msg, msg_size);
        ctx->buf_size = msg_size;
    }
}

void sha256_final(sha256_context *ctx, uint8_t *digest)
{
    uint64_t total_size_bits;
    uint32_t i;
    
    total_size_bits = (ctx->total_size + ctx->buf_size) * 8;
    
    ctx->buffer[ctx->buf_size++] = 0x80;
    
    if (ctx->buf_size > 56)
    {
        memset(ctx->buffer + ctx->buf_size, 0, 64 - ctx->buf_size);
        sha256_transform(ctx, ctx->buffer);
        memset(ctx->buffer, 0, 56);
    }
    else
    {
        memset(ctx->buffer + ctx->buf_size, 0, 56 - ctx->buf_size);
    }

    ctx->buffer[56] = (uint8_t)(total_size_bits >> 56);
    ctx->buffer[57] = (uint8_t)(total_size_bits >> 48);
    ctx->buffer[58] = (uint8_t)(total_size_bits >> 40);
    ctx->buffer[59] = (uint8_t)(total_size_bits >> 32);
    ctx->buffer[60] = (uint8_t)(total_size_bits >> 24);
    ctx->buffer[61] = (uint8_t)(total_size_bits >> 16);
    ctx->buffer[62] = (uint8_t)(total_size_bits >> 8);
    ctx->buffer[63] = (uint8_t)(total_size_bits >> 0);
    
    sha256_transform(ctx, ctx->buffer);
    
    for (i = 0; i < 8; i++)
    {
        digest[i*4+0] = (uint8_t)(ctx->state[i] >> 24);
        digest[i*4+1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i*4+2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i*4+3] = (uint8_t)(ctx->state[i] >> 0);
    }
}

#endif /* !SHA256_NO_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif