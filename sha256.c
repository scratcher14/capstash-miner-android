/**
 * sha256.c — SHA-256 and SHA-256d implementation
 * v2.0
 *
 * Place at: android/app/src/main/cpp/sha256.c
 *
 * FIPS 180-4 compliant. No external dependencies.
 * Optimized for ARM64 with -O3 — the compiler will auto-vectorize
 * the message schedule and round loops with NEON.
 */

#include "sha256.h"
#include <string.h>

// ── Round constants ───────────────────────────────────────────────────────────
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

// ── Bit rotation ──────────────────────────────────────────────────────────────
#define ROTR32(x, n)  (((x) >> (n)) | ((x) << (32 - (n))))

// ── SHA-256 round functions ───────────────────────────────────────────────────
#define CH(x, y, z)   (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z)  (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x)        (ROTR32(x,  2) ^ ROTR32(x, 13) ^ ROTR32(x, 22))
#define EP1(x)        (ROTR32(x,  6) ^ ROTR32(x, 11) ^ ROTR32(x, 25))
#define SIG0(x)       (ROTR32(x,  7) ^ ROTR32(x, 18) ^ ((x) >>  3))
#define SIG1(x)       (ROTR32(x, 17) ^ ROTR32(x, 19) ^ ((x) >> 10))

// ── Big-endian read/write ─────────────────────────────────────────────────────
static inline uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

static inline void write_be32(uint8_t *p, uint32_t v) {
    p[0] = (v >> 24) & 0xff;
    p[1] = (v >> 16) & 0xff;
    p[2] = (v >>  8) & 0xff;
    p[3] =  v        & 0xff;
}

// ── Core block transform ──────────────────────────────────────────────────────
static void sha256_transform(sha256_ctx *ctx, const uint8_t *block) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;
    int i;

    // Message schedule
    for (i = 0; i < 16; i++)
        w[i] = read_be32(block + i * 4);
    for (i = 16; i < 64; i++)
        w[i] = SIG1(w[i-2]) + w[i-7] + SIG0(w[i-15]) + w[i-16];

    // Initialize working variables
    a = ctx->state[0]; b = ctx->state[1];
    c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5];
    g = ctx->state[6]; h = ctx->state[7];

    // 64 rounds
    for (i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e, f, g) + K[i] + w[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    // Add to state
    ctx->state[0] += a; ctx->state[1] += b;
    ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f;
    ctx->state[6] += g; ctx->state[7] += h;
}

// ── Public API ────────────────────────────────────────────────────────────────

void sha256_init(sha256_ctx *ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
    memset(ctx->buf, 0, SHA256_BLOCK_SIZE);
}

void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len) {
    size_t buf_used = ctx->count % SHA256_BLOCK_SIZE;
    ctx->count += len;

    // Fill partial buffer first
    if (buf_used > 0) {
        size_t space = SHA256_BLOCK_SIZE - buf_used;
        size_t copy  = len < space ? len : space;
        memcpy(ctx->buf + buf_used, data, copy);
        data += copy;
        len  -= copy;
        buf_used += copy;
        if (buf_used == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx, ctx->buf);
        }
    }

    // Process full blocks directly from input
    while (len >= SHA256_BLOCK_SIZE) {
        sha256_transform(ctx, data);
        data += SHA256_BLOCK_SIZE;
        len  -= SHA256_BLOCK_SIZE;
    }

    // Stash remaining bytes
    if (len > 0) {
        memcpy(ctx->buf, data, len);
    }
}

void sha256_final(sha256_ctx *ctx, uint8_t *digest) {
    uint8_t  pad[SHA256_BLOCK_SIZE * 2];
    size_t   buf_used = ctx->count % SHA256_BLOCK_SIZE;
    size_t   pad_len;
    uint64_t bit_count = ctx->count * 8;

    // Build padding: 0x80 + zeros + 64-bit big-endian bit count
    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;

    // Padding brings us to 56 mod 64 (leaving 8 bytes for length)
    pad_len = (buf_used < 56)
              ? (56 - buf_used)
              : (64 + 56 - buf_used);

    // Append big-endian bit count after padding
    uint8_t len_bytes[8];
    len_bytes[0] = (bit_count >> 56) & 0xff;
    len_bytes[1] = (bit_count >> 48) & 0xff;
    len_bytes[2] = (bit_count >> 40) & 0xff;
    len_bytes[3] = (bit_count >> 32) & 0xff;
    len_bytes[4] = (bit_count >> 24) & 0xff;
    len_bytes[5] = (bit_count >> 16) & 0xff;
    len_bytes[6] = (bit_count >>  8) & 0xff;
    len_bytes[7] =  bit_count        & 0xff;

    sha256_update(ctx, pad, pad_len);
    sha256_update(ctx, len_bytes, 8);

    // Serialize state to digest (big-endian)
    for (int i = 0; i < 8; i++)
        write_be32(digest + i * 4, ctx->state[i]);
}

void sha256(const uint8_t *data, size_t len, uint8_t *digest) {
    sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}

void sha256d(const uint8_t *data, size_t len, uint8_t *digest) {
    uint8_t tmp[SHA256_DIGEST_SIZE];
    sha256(data, len, tmp);
    sha256(tmp, SHA256_DIGEST_SIZE, digest);
}

void sha256d_two(const uint8_t *a, size_t a_len,
                 const uint8_t *b, size_t b_len,
                 uint8_t *digest) {
    // Hash a||b in a single pass — no allocation needed
    uint8_t tmp[SHA256_DIGEST_SIZE];
    sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, a, a_len);
    sha256_update(&ctx, b, b_len);
    sha256_final(&ctx, tmp);
    // Second pass
    sha256(tmp, SHA256_DIGEST_SIZE, digest);
}
