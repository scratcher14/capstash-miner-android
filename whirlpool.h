/**
 * whirlpool.h — Whirlpool-512 hash engine for CapStash NDK miner
 * v2.0
 *
 * Place at: android/app/src/main/cpp/whirlpool.h
 *
 * The lookup tables T0-T7 and round constants RC are defined in
 * whirlpool_tables.h — copy directly from the CUDA tables the dev
 * provided, changing only:
 *   __device__ __constant__ uint64_t T0[256]  →  static const uint64_t T0[256]
 *   __device__ __constant__ uint64_t RC[10]   →  static const uint64_t RC[10]
 */

#ifndef CAPSTASH_WHIRLPOOL_H
#define CAPSTASH_WHIRLPOOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Constants ─────────────────────────────────────────────────────────────────
#define WHIRLPOOL_DIGEST_SIZE   64   // 512 bits
#define WHIRLPOOL_BLOCK_SIZE    64   // 512-bit internal block
#define WHIRLPOOL_ROUNDS        10
#define CAPSTASH_HASH_SIZE      32   // final PoW hash = XOR fold of 64 bytes → 32

// ── Whirlpool state ───────────────────────────────────────────────────────────
typedef struct {
    uint64_t state[8];          // 512-bit hash state (8 x 64-bit words)
    uint64_t block[8];          // current message block
    uint8_t  buffer[64];        // byte buffer for partial blocks
    uint64_t bit_length[4];     // total message length in bits (256-bit counter)
    int      buffer_bits;       // number of bits in buffer
    int      buffer_pos;        // current position in buffer
} whirlpool_ctx;

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * whirlpool_init — initialize a fresh Whirlpool context
 */
void whirlpool_init(whirlpool_ctx *ctx);

/**
 * whirlpool_update — feed data into the hash
 * @ctx:    hash context
 * @data:   input bytes
 * @len:    number of bytes
 */
void whirlpool_update(whirlpool_ctx *ctx, const uint8_t *data, size_t len);

/**
 * whirlpool_final — finalize and output 64-byte digest
 * @ctx:    hash context (consumed)
 * @digest: output buffer — must be WHIRLPOOL_DIGEST_SIZE (64) bytes
 */
void whirlpool_final(whirlpool_ctx *ctx, uint8_t *digest);

/**
 * whirlpool512 — single-call convenience wrapper
 * @data:   input
 * @len:    input length in bytes
 * @digest: 64-byte output
 */
void whirlpool512(const uint8_t *data, size_t len, uint8_t *digest);

/**
 * capstash_hash — CapStash PoW hash
 * Computes Whirlpool-512 over 80-byte block header, then
 * XOR-folds the 64-byte digest into a 32-byte PoW hash:
 *   out[i] = digest[i] ^ digest[i + 32]   for i in 0..31
 *
 * @header: 80-byte block header (little-endian serialized)
 * @out:    32-byte PoW hash output
 */
void capstash_hash(const uint8_t *header, uint8_t *out);

/**
 * capstash_hash_meets_target — check if PoW hash beats target
 * Compares 32-byte hash against 32-byte target (big-endian, leading zeros)
 * Returns 1 if hash <= target (valid PoW), 0 otherwise
 */
int capstash_hash_meets_target(const uint8_t *hash, const uint8_t *target);

// ── Block header helpers ──────────────────────────────────────────────────────

/**
 * write_le32 — write uint32 in little-endian to buffer at offset
 */
static inline void write_le32(uint8_t *buf, int offset, uint32_t val) {
    buf[offset + 0] = (val >>  0) & 0xff;
    buf[offset + 1] = (val >>  8) & 0xff;
    buf[offset + 2] = (val >> 16) & 0xff;
    buf[offset + 3] = (val >> 24) & 0xff;
}

/**
 * read_le32 — read little-endian uint32 from buffer at offset
 */
static inline uint32_t read_le32(const uint8_t *buf, int offset) {
    return ((uint32_t)buf[offset + 0])       |
           ((uint32_t)buf[offset + 1] <<  8) |
           ((uint32_t)buf[offset + 2] << 16) |
           ((uint32_t)buf[offset + 3] << 24);
}

/**
 * set_nonce — write nonce into header at bytes 76-79
 */
static inline void set_nonce(uint8_t *header, uint32_t nonce) {
    write_le32(header, 76, nonce);
}

/**
 * hex_to_bytes — decode hex string to bytes (big-endian)
 * Returns number of bytes written, -1 on error
 */
int hex_to_bytes(const char *hex, uint8_t *out, size_t out_len);

/**
 * bytes_to_hex — encode bytes to hex string
 * @out must be at least len*2 + 1 bytes
 */
void bytes_to_hex(const uint8_t *bytes, size_t len, char *out);

#ifdef __cplusplus
}
#endif

#endif /* CAPSTASH_WHIRLPOOL_H */