/**
 * rpc.h — RPC client header for CapStash NDK miner
 * v2.0
 *
 * Place at: android/app/src/main/cpp/rpc.h
 */

#ifndef CAPSTASH_RPC_H
#define CAPSTASH_RPC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── RPC connection config ─────────────────────────────────────────────────────
typedef struct {
    char host[64];
    int  port;
    char user[64];
    char pass[64];
} rpc_config_t;

// ── Block template from getblocktemplate ─────────────────────────────────────
typedef struct {
    uint32_t version;
    uint32_t height;
    uint32_t curtime;
    uint32_t bits;
    int64_t  coinbase_value;       // satoshis
    char     prev_hash_hex[65];    // 32 bytes as 64 hex chars
    char     bits_hex[16];         // compact target hex
    char     target_hex[65];       // full 32-byte target hex
    // Transaction data for merkle root (simplified — coinbase only for solo)
    char     coinbase_hex[512];    // serialized coinbase tx hex
    char     merkle_root_hex[65];  // computed merkle root
} block_template_t;

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * rpc_getblocktemplate — fetch a new block template from the node
 * Returns 0 on success, -1 on error
 */
int rpc_getblocktemplate(const rpc_config_t *cfg, block_template_t *tmpl);

/**
 * rpc_submitblock — submit a solved block
 * @block_hex: full serialized block as hex string
 * Returns 0 if accepted, -1 if rejected or error
 */
int rpc_submitblock(const rpc_config_t *cfg, const char *block_hex);

/**
 * rpc_getbestblockhash — get current chain tip hash
 * Used to detect when a new block arrives (stale template)
 * @out_hash_hex: output buffer, must be at least 65 bytes
 * Returns 0 on success, -1 on error
 */
int rpc_getbestblockhash(const rpc_config_t *cfg, char *out_hash_hex);

/**
 * rpc_parse_template — parse getblocktemplate JSON response
 * Called internally but exposed for unit testing
 */
int rpc_parse_template(const char *json, block_template_t *tmpl);

#ifdef __cplusplus
}
#endif

#endif /* CAPSTASH_RPC_H */