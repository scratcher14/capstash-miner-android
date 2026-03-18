/**
 * miner.h — CapStash miner public API
 * v1.0 — Termux/Android CLI version
 */

#ifndef CAPSTASH_MINER_H
#define CAPSTASH_MINER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char host[64];
    int  port;
    char user[64];
    char pass[64];
    char address[128];
    int  threads;
    int  duty_cycle_on;
    int  duty_cycle_off;
    int  pool_mode;      // 0 = solo (RPC), 1 = pool (stratum)
} miner_config_t;

typedef struct {
    double   hashrate;
    uint64_t total_hashes;
    uint32_t blocks_found;
    uint32_t shares_submitted;
    int      running;
    int      thread_count;
} miner_stats_t;

typedef void (*miner_hashrate_cb)(double hashrate, void *userdata);
typedef void (*miner_block_cb)(uint32_t height, const char *hash, void *userdata);
typedef void (*miner_error_cb)(const char *message, void *userdata);

typedef struct {
    miner_hashrate_cb  on_hashrate;
    miner_block_cb     on_block;
    miner_error_cb     on_error;
    void              *userdata;
} miner_callbacks_t;

int  miner_start(const miner_config_t *config, const miner_callbacks_t *callbacks);
void miner_stop(void);
void miner_get_stats(miner_stats_t *stats);
int  miner_is_running(void);
void miner_set_threads(int threads);

#ifdef __cplusplus
}
#endif

#endif
