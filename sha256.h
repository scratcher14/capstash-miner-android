/**
 * sha256.h — SHA-256 and SHA-256d for CapStash NDK miner
 * v2.0
 *
 * Place at: android/app/src/main/cpp/sha256.h
 *
 * Used for:
 *   - Coinbase txid (double-SHA256 of serialized coinbase tx)
 *   - Merkle root computation (double-SHA256 of concatenated txids)
 *
 * NOT used for PoW hashing — that uses Whirlpool-512 (whirlpool.h)
 */

#ifndef CAPSTASH_SHA256_H
#define CAPSTASH_SHA256_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHA256_DIGEST_SIZE  32
#define SHA256_BLOCK_SIZE   64

// ── SHA-256 context ───────────────────────────────────────────────────────────
typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buf[SHA256_BLOCK_SIZE];
} sha256_ctx;

// ── Streaming API ─────────────────────────────────────────────────────────────

/**
 * sha256_init — initialize a fresh SHA-256 context
 */
void sha256_init(sha256_ctx *ctx);

/**
 * sha256_update — feed data into the hash
 */
void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len);

/**
 * sha256_final — finalize and output 32-byte digest
 * @digest must be SHA256_DIGEST_SIZE (32) bytes
 */
void sha256_final(sha256_ctx *ctx, uint8_t *digest);

// ── Single-call convenience wrappers ──────────────────────────────────────────

/**
 * sha256 — single SHA-256 pass
 * @data:   input bytes
 * @len:    input length
 * @digest: 32-byte output
 */
void sha256(const uint8_t *data, size_t len, uint8_t *digest);

/**
 * sha256d — double SHA-256 (SHA256(SHA256(data)))
 * Used for Bitcoin-derived txid and merkle root computation
 * @data:   input bytes
 * @len:    input length
 * @digest: 32-byte output
 */
void sha256d(const uint8_t *data, size_t len, uint8_t *digest);

/**
 * sha256d_two — double SHA-256 of two concatenated inputs
 * Avoids a malloc/memcpy when hashing two 32-byte txids for merkle nodes
 * @a:      first input (typically 32 bytes)
 * @a_len:  length of first input
 * @b:      second input (typically 32 bytes)
 * @b_len:  length of second input
 * @digest: 32-byte output
 */
void sha256d_two(const uint8_t *a, size_t a_len,
                 const uint8_t *b, size_t b_len,
                 uint8_t *digest);

#ifdef __cplusplus
}
#endif

#endif /* CAPSTASH_SHA256_H */
