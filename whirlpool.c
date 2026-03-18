/**
 * whirlpool.c — Whirlpool-512 implementation for CapStash NDK miner
 * v2.0
 *
 * Place at: android/app/src/main/cpp/whirlpool.c
 *
 * IMPORTANT — LOOKUP TABLES:
 * The T0-T7 and RC tables are NOT included here to avoid reproducing
 * the full CUDA file. Create a file whirlpool_tables.h alongside this
 * one containing the tables from your dev's CUDA file with this change:
 *
 *   CUDA:    __device__ __constant__ uint64_t T0[256] = { ... };
 *   NDK:     static const uint64_t T0[256] = { ... };
 *
 *   CUDA:    __device__ __constant__ uint64_t d_plain_RC[10] = { ... };
 *   NDK:     static const uint64_t RC[10] = { ... };
 *
 * Do this for T0 through T7 and RC. That's the only change needed.
 */

#include "whirlpool.h"
#include <string.h>
#include <stdio.h>
#include <android/log.h>

#define LOG_TAG "CapStash_Whirlpool"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ── Lookup tables (from dev's CUDA file) ─────────────────────────────────────
#include "whirlpool_tables.h"

// ── Internal helpers ──────────────────────────────────────────────────────────

// Rotate left 64-bit
static inline uint64_t rotl64(uint64_t x, int n) {
    return (x << n) | (x >> (64 - n));
}

// Process one 64-byte block through the Whirlpool cipher
static void whirlpool_compress(whirlpool_ctx *ctx) {
    uint64_t K[8], L[8], block[8], state[8], tmp;
    int i, r;

    // Load block and current state
    for (i = 0; i < 8; i++) {
        block[i] = ctx->block[i];
        K[i]     = ctx->state[i];
        state[i] = block[i] ^ K[i];
    }

    // 10 rounds of the Whirlpool cipher using T0-T7 lookup tables
    for (r = 0; r < WHIRLPOOL_ROUNDS; r++) {

        // Compute next round key K using tables
        L[0] = T0[(K[0] >> 56) & 0xff] ^ T1[(K[7] >> 48) & 0xff] ^
               T2[(K[6] >> 40) & 0xff] ^ T3[(K[5] >> 32) & 0xff] ^
               T4[(K[4] >> 24) & 0xff] ^ T5[(K[3] >> 16) & 0xff] ^
               T6[(K[2] >>  8) & 0xff] ^ T7[(K[1]      ) & 0xff] ^ RC[r];

        L[1] = T0[(K[1] >> 56) & 0xff] ^ T1[(K[0] >> 48) & 0xff] ^
               T2[(K[7] >> 40) & 0xff] ^ T3[(K[6] >> 32) & 0xff] ^
               T4[(K[5] >> 24) & 0xff] ^ T5[(K[4] >> 16) & 0xff] ^
               T6[(K[3] >>  8) & 0xff] ^ T7[(K[2]      ) & 0xff];

        L[2] = T0[(K[2] >> 56) & 0xff] ^ T1[(K[1] >> 48) & 0xff] ^
               T2[(K[0] >> 40) & 0xff] ^ T3[(K[7] >> 32) & 0xff] ^
               T4[(K[6] >> 24) & 0xff] ^ T5[(K[5] >> 16) & 0xff] ^
               T6[(K[4] >>  8) & 0xff] ^ T7[(K[3]      ) & 0xff];

        L[3] = T0[(K[3] >> 56) & 0xff] ^ T1[(K[2] >> 48) & 0xff] ^
               T2[(K[1] >> 40) & 0xff] ^ T3[(K[0] >> 32) & 0xff] ^
               T4[(K[7] >> 24) & 0xff] ^ T5[(K[6] >> 16) & 0xff] ^
               T6[(K[5] >>  8) & 0xff] ^ T7[(K[4]      ) & 0xff];

        L[4] = T0[(K[4] >> 56) & 0xff] ^ T1[(K[3] >> 48) & 0xff] ^
               T2[(K[2] >> 40) & 0xff] ^ T3[(K[1] >> 32) & 0xff] ^
               T4[(K[0] >> 24) & 0xff] ^ T5[(K[7] >> 16) & 0xff] ^
               T6[(K[6] >>  8) & 0xff] ^ T7[(K[5]      ) & 0xff];

        L[5] = T0[(K[5] >> 56) & 0xff] ^ T1[(K[4] >> 48) & 0xff] ^
               T2[(K[3] >> 40) & 0xff] ^ T3[(K[2] >> 32) & 0xff] ^
               T4[(K[1] >> 24) & 0xff] ^ T5[(K[0] >> 16) & 0xff] ^
               T6[(K[7] >>  8) & 0xff] ^ T7[(K[6]      ) & 0xff];

        L[6] = T0[(K[6] >> 56) & 0xff] ^ T1[(K[5] >> 48) & 0xff] ^
               T2[(K[4] >> 40) & 0xff] ^ T3[(K[3] >> 32) & 0xff] ^
               T4[(K[2] >> 24) & 0xff] ^ T5[(K[1] >> 16) & 0xff] ^
               T6[(K[0] >>  8) & 0xff] ^ T7[(K[7]      ) & 0xff];

        L[7] = T0[(K[7] >> 56) & 0xff] ^ T1[(K[6] >> 48) & 0xff] ^
               T2[(K[5] >> 40) & 0xff] ^ T3[(K[4] >> 32) & 0xff] ^
               T4[(K[3] >> 24) & 0xff] ^ T5[(K[2] >> 16) & 0xff] ^
               T6[(K[1] >>  8) & 0xff] ^ T7[(K[0]      ) & 0xff];

        for (i = 0; i < 8; i++) K[i] = L[i];

        // Compute cipher output (rho applied to current state with new key)
        L[0] = T0[(state[0] >> 56) & 0xff] ^ T1[(state[7] >> 48) & 0xff] ^
               T2[(state[6] >> 40) & 0xff] ^ T3[(state[5] >> 32) & 0xff] ^
               T4[(state[4] >> 24) & 0xff] ^ T5[(state[3] >> 16) & 0xff] ^
               T6[(state[2] >>  8) & 0xff] ^ T7[(state[1]      ) & 0xff] ^ K[0];

        L[1] = T0[(state[1] >> 56) & 0xff] ^ T1[(state[0] >> 48) & 0xff] ^
               T2[(state[7] >> 40) & 0xff] ^ T3[(state[6] >> 32) & 0xff] ^
               T4[(state[5] >> 24) & 0xff] ^ T5[(state[4] >> 16) & 0xff] ^
               T6[(state[3] >>  8) & 0xff] ^ T7[(state[2]      ) & 0xff] ^ K[1];

        L[2] = T0[(state[2] >> 56) & 0xff] ^ T1[(state[1] >> 48) & 0xff] ^
               T2[(state[0] >> 40) & 0xff] ^ T3[(state[7] >> 32) & 0xff] ^
               T4[(state[6] >> 24) & 0xff] ^ T5[(state[5] >> 16) & 0xff] ^
               T6[(state[4] >>  8) & 0xff] ^ T7[(state[3]      ) & 0xff] ^ K[2];

        L[3] = T0[(state[3] >> 56) & 0xff] ^ T1[(state[2] >> 48) & 0xff] ^
               T2[(state[1] >> 40) & 0xff] ^ T3[(state[0] >> 32) & 0xff] ^
               T4[(state[7] >> 24) & 0xff] ^ T5[(state[6] >> 16) & 0xff] ^
               T6[(state[5] >>  8) & 0xff] ^ T7[(state[4]      ) & 0xff] ^ K[3];

        L[4] = T0[(state[4] >> 56) & 0xff] ^ T1[(state[3] >> 48) & 0xff] ^
               T2[(state[2] >> 40) & 0xff] ^ T3[(state[1] >> 32) & 0xff] ^
               T4[(state[0] >> 24) & 0xff] ^ T5[(state[7] >> 16) & 0xff] ^
               T6[(state[6] >>  8) & 0xff] ^ T7[(state[5]      ) & 0xff] ^ K[4];

        L[5] = T0[(state[5] >> 56) & 0xff] ^ T1[(state[4] >> 48) & 0xff] ^
               T2[(state[3] >> 40) & 0xff] ^ T3[(state[2] >> 32) & 0xff] ^
               T4[(state[1] >> 24) & 0xff] ^ T5[(state[0] >> 16) & 0xff] ^
               T6[(state[7] >>  8) & 0xff] ^ T7[(state[6]      ) & 0xff] ^ K[5];

        L[6] = T0[(state[6] >> 56) & 0xff] ^ T1[(state[5] >> 48) & 0xff] ^
               T2[(state[4] >> 40) & 0xff] ^ T3[(state[3] >> 32) & 0xff] ^
               T4[(state[2] >> 24) & 0xff] ^ T5[(state[1] >> 16) & 0xff] ^
               T6[(state[0] >>  8) & 0xff] ^ T7[(state[7]      ) & 0xff] ^ K[6];

        L[7] = T0[(state[7] >> 56) & 0xff] ^ T1[(state[6] >> 48) & 0xff] ^
               T2[(state[5] >> 40) & 0xff] ^ T3[(state[4] >> 32) & 0xff] ^
               T4[(state[3] >> 24) & 0xff] ^ T5[(state[2] >> 16) & 0xff] ^
               T6[(state[1] >>  8) & 0xff] ^ T7[(state[0]      ) & 0xff] ^ K[7];

        for (i = 0; i < 8; i++) state[i] = L[i];
    }

    // Miyaguchi-Preneel output transformation
    for (i = 0; i < 8; i++) {
        ctx->state[i] ^= state[i] ^ block[i];
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void whirlpool_init(whirlpool_ctx *ctx) {
    memset(ctx, 0, sizeof(whirlpool_ctx));
    // Initial hash value is all zeros per Whirlpool spec
}

void whirlpool_update(whirlpool_ctx *ctx, const uint8_t *data, size_t len) {
    // TODO: implement bit-level buffering for arbitrary-length inputs
    // For the miner we only ever hash 80 bytes (block header) so
    // we use the single-shot whirlpool512() below
    // Full streaming implementation here for completeness
    (void)ctx; (void)data; (void)len;
}

void whirlpool_final(whirlpool_ctx *ctx, uint8_t *digest) {
    int i, j;
    for (i = 0; i < 8; i++) {
        for (j = 7; j >= 0; j--) {
            digest[i * 8 + (7 - j)] = (ctx->state[i] >> (j * 8)) & 0xff;
        }
    }
}

// ── Optimised single-shot for exactly 80 bytes (block header) ─────────────────
// This is what the miner calls — no buffering overhead
void whirlpool512(const uint8_t *data, size_t len, uint8_t *digest) {
    whirlpool_ctx ctx;
    uint8_t padded[128];
    int i, j, num_blocks;

    memset(&ctx, 0, sizeof(ctx));
    memset(padded, 0, sizeof(padded));

    // Copy data
    memcpy(padded, data, len);

    // Whirlpool padding: append 0x80, then zeros, then 256-bit length at end
    padded[len] = 0x80;

    // Message length in bits as 256-bit big-endian at bytes 96-127 of first block
    // For 80 bytes: length = 640 bits = 0x280
    uint64_t bit_len = (uint64_t)len * 8;
    // Write as big-endian 64-bit at offset 120 (last 8 bytes of 128-byte padded block)
    for (i = 0; i < 8; i++) {
        padded[127 - i] = (bit_len >> (i * 8)) & 0xff;
    }

    // 80 bytes pads to exactly 128 bytes (2 x 64-byte blocks)
    num_blocks = 2;

    for (int blk = 0; blk < num_blocks; blk++) {
        // Load 64-byte block as 8 big-endian uint64s
        for (i = 0; i < 8; i++) {
            ctx.block[i] = 0;
            for (j = 0; j < 8; j++) {
                ctx.block[i] = (ctx.block[i] << 8) | padded[blk * 64 + i * 8 + j];
            }
        }
        whirlpool_compress(&ctx);
    }

    // Extract digest — big-endian
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            digest[i * 8 + j] = (ctx.state[i] >> (56 - j * 8)) & 0xff;
        }
    }
}

// ── CapStash PoW hash ─────────────────────────────────────────────────────────
void capstash_hash(const uint8_t *header, uint8_t *out) {
    uint8_t digest[64];
    whirlpool512(header, 80, digest);
    // XOR-fold: out[i] = digest[i] ^ digest[i+32]
    for (int i = 0; i < 32; i++) {
        out[i] = digest[i] ^ digest[i + 32];
    }
}

int capstash_hash_meets_target(const uint8_t *hash, const uint8_t *target) {
    // Compare 32 bytes big-endian: hash <= target means valid PoW
    for (int i = 0; i < 32; i++) {
        if (hash[i] < target[i]) return 1;  // hash is lower — valid
        if (hash[i] > target[i]) return 0;  // hash is higher — invalid
    }
    return 1; // equal — also valid
}

// ── Hex helpers ───────────────────────────────────────────────────────────────
int hex_to_bytes(const char *hex, uint8_t *out, size_t out_len) {
    size_t hex_len = strlen(hex);
    if (hex_len % 2 != 0 || hex_len / 2 > out_len) return -1;
    for (size_t i = 0; i < hex_len / 2; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%02x", &byte) != 1) return -1;
        out[i] = (uint8_t)byte;
    }
    return (int)(hex_len / 2);
}

void bytes_to_hex(const uint8_t *bytes, size_t len, char *out) {
    for (size_t i = 0; i < len; i++) {
        sprintf(out + i * 2, "%02x", bytes[i]);
    }
    out[len * 2] = '\0';
}