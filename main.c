/**
 * main.c — CapStash CPU Miner
 * capstash-miner v1.0
 *
 * Supports:
 *   Solo mining  — getblocktemplate via RPC
 *   Pool mining  — stratum+tcp protocol
 *
 * Usage:
 *   Solo:  capstash-miner --url http://node:8332 --user rpcuser --pass rpcpass --address cap1q...
 *   Pool:  capstash-miner --url stratum+tcp://pool:port --user wallet.worker --pass x --address cap1q...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <stdatomic.h>
#include <unistd.h>

#include "miner.h"
#include "rpc.h"

#define MINER_VERSION "1.0.0"

// ── ANSI colors ────────────────────────────────────────────────────────
#define COL_GREEN  "\033[38;5;82m"
#define COL_AMBER  "\033[38;5;214m"
#define COL_RED    "\033[38;5;196m"
#define COL_DIM    "\033[2m"
#define COL_RESET  "\033[0m"

static volatile int g_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    printf("\n%s[capstash-miner]%s shutting down...\n", COL_AMBER, COL_RESET);
    g_running = 0;
    miner_stop();
}

// ── Auto-detect optimal thread count ──────────────────────────────────
static int detect_optimal_threads(void) {
    int cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (cores <= 0) cores = 4;
    int optimal = cores / 2;
    if (optimal < 1) optimal = 1;
    return optimal;
}

// ── Parse URL → mode, host, port ──────────────────────────────────────
// Returns 0=solo(http), 1=pool(stratum), -1=error
static int parse_url(const char *url, char *host, int *port) {
    const char *p = url;

    if (strncmp(p, "stratum+tcp://", 14) == 0) {
        p += 14;
        // host:port
        const char *colon = strrchr(p, ':');
        if (!colon) return -1;
        size_t hlen = (size_t)(colon - p);
        if (hlen >= 63) return -1;
        strncpy(host, p, hlen);
        host[hlen] = '\0';
        *port = atoi(colon + 1);
        return 1; // stratum
    }

    if (strncmp(p, "http://", 7) == 0) p += 7;
    const char *colon = strrchr(p, ':');
    const char *slash = strchr(p, '/');
    if (!colon) {
        size_t hlen = slash ? (size_t)(slash - p) : strlen(p);
        if (hlen >= 63) return -1;
        strncpy(host, p, hlen);
        host[hlen] = '\0';
        *port = 8332;
        return 0; // solo
    }
    size_t hlen = (size_t)(colon - p);
    if (hlen >= 63) return -1;
    strncpy(host, p, hlen);
    host[hlen] = '\0';
    char port_str[8] = {0};
    const char *ps = colon + 1;
    size_t plen = slash ? (size_t)(slash - ps) : strlen(ps);
    if (plen >= 7) return -1;
    strncpy(port_str, ps, plen);
    *port = atoi(port_str);
    return 0; // solo
}

// ── Callbacks ──────────────────────────────────────────────────────────
static void on_hashrate(double hr, void *ud) {
    (void)ud;
    char buf[32];
    if      (hr >= 1e9) snprintf(buf, sizeof(buf), "%.2f GH/s", hr / 1e9);
    else if (hr >= 1e6) snprintf(buf, sizeof(buf), "%.2f MH/s", hr / 1e6);
    else if (hr >= 1e3) snprintf(buf, sizeof(buf), "%.2f KH/s", hr / 1e3);
    else                snprintf(buf, sizeof(buf), "%.0f H/s",  hr);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("%s[%02d:%02d:%02d]%s hashrate %s%s%s\n",
           COL_DIM, t->tm_hour, t->tm_min, t->tm_sec, COL_RESET,
           COL_GREEN, buf, COL_RESET);
    fflush(stdout);
}

static void on_block(uint32_t height, const char *hash, void *ud) {
    (void)ud;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("\n%s[%02d:%02d:%02d] ★ BLOCK FOUND! height=%u%s\n",
           COL_AMBER, t->tm_hour, t->tm_min, t->tm_sec, height, COL_RESET);
    printf("%s        %s%s\n\n", COL_DIM, hash, COL_RESET);
    fflush(stdout);
}

static void on_error(const char *msg, void *ud) {
    (void)ud;
    fprintf(stderr, "%s[ERROR] %s%s\n", COL_RED, msg, COL_RESET);
    fflush(stderr);
}

// ── Usage ──────────────────────────────────────────────────────────────
static void usage(const char *prog) {
    printf("\n%scapstash-miner v%s%s — Whirlpool-512 CPU miner\n\n",
           COL_GREEN, MINER_VERSION, COL_RESET);
    printf("Solo mining (direct node connection):\n");
    printf("  %s --url http://NODE_IP:8332 --user rpcuser --pass rpcpass --address cap1q...\n\n", prog);
    printf("Pool mining (stratum):\n");
    printf("  %s --url stratum+tcp://pool:port --user WALLET.worker --pass x --address cap1q...\n\n", prog);
    printf("Options:\n");
    printf("  --url       <url>     Node RPC URL or stratum pool URL\n");
    printf("  --user      <user>    RPC username or wallet.worker\n");
    printf("  --pass      <pass>    RPC password or pool password (usually x)\n");
    printf("  --address   <addr>    Reward address (cap1..., C..., or 8...)\n");
    printf("  --threads   <n>       Mining threads (default: auto = cores/2)\n");
    printf("  --duty-on   <secs>    Mine duration per cycle (0 = continuous)\n");
    printf("  --duty-off  <secs>    Rest duration per cycle\n");
    printf("  --version             Print version\n");
    printf("  --help                This help\n\n");
}

static void print_banner(const miner_config_t *cfg, int mode, int auto_t) {
    const char *mode_str = mode == 1 ? "POOL (stratum)" : "SOLO (RPC)";
    printf("\n");
    printf("%s╔═══════════════════════════════════════════╗%s\n", COL_GREEN, COL_RESET);
    printf("%s║    CAPSTASH MINER v%-22s║%s\n", COL_GREEN, MINER_VERSION, COL_RESET);
    printf("%s║    Whirlpool-512 · CapStash Network       ║%s\n", COL_GREEN, COL_RESET);
    printf("%s╚═══════════════════════════════════════════╝%s\n", COL_GREEN, COL_RESET);
    printf("\n");
    printf("  %sMode:%s    %s\n",     COL_DIM, COL_RESET, mode_str);
    printf("  %sNode:%s    %s:%d\n",  COL_DIM, COL_RESET, cfg->host, cfg->port);
    printf("  %sAddress:%s %s\n",     COL_DIM, COL_RESET, cfg->address);
    printf("  %sThreads:%s %d%s\n",   COL_DIM, COL_RESET, cfg->threads,
           auto_t ? " (auto)" : "");
    printf("\n  %sCtrl+C to stop%s\n\n", COL_DIM, COL_RESET);
}

// ── Main ───────────────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    miner_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    char url[256] = {0};
    int  auto_threads = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")    || !strcmp(argv[i], "-h")) { usage(argv[0]); return 0; }
        if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-v")) {
            printf("capstash-miner v%s\n", MINER_VERSION); return 0;
        }
        #define STR_ARG(flag, dst) \
            if (!strcmp(argv[i], flag) && i+1 < argc) { strncpy(dst, argv[++i], sizeof(dst)-1); continue; }
        #define INT_ARG(flag, dst) \
            if (!strcmp(argv[i], flag) && i+1 < argc) { dst = atoi(argv[++i]); continue; }

        STR_ARG("--url",      url)
        STR_ARG("--user",     cfg.user)
        STR_ARG("--pass",     cfg.pass)
        STR_ARG("--address",  cfg.address)
        INT_ARG("--threads",  cfg.threads)
        INT_ARG("--duty-on",  cfg.duty_cycle_on)
        INT_ARG("--duty-off", cfg.duty_cycle_off)

        fprintf(stderr, "Unknown argument: %s\n", argv[i]);
        usage(argv[0]); return 1;
    }

    // Validate
    if (!url[0])           { fprintf(stderr, "Error: --url required\n");     return 1; }
    if (!cfg.user[0])      { fprintf(stderr, "Error: --user required\n");    return 1; }
    if (!cfg.pass[0])      { fprintf(stderr, "Error: --pass required\n");    return 1; }
    if (!cfg.address[0])   { fprintf(stderr, "Error: --address required\n"); return 1; }

    // Parse URL
    int mode = parse_url(url, cfg.host, &cfg.port);
    if (mode < 0) { fprintf(stderr, "Error: invalid URL\n"); return 1; }

    // Pool mode: store pool URL and user in config for stratum handler
    cfg.pool_mode = mode;  // 0=solo, 1=stratum

    // Auto threads
    if (cfg.threads <= 0) {
        cfg.threads  = detect_optimal_threads();
        auto_threads = 1;
    }

    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    print_banner(&cfg, mode, auto_threads);

    miner_callbacks_t cbs = {
        .on_hashrate = on_hashrate,
        .on_block    = on_block,
        .on_error    = on_error,
        .userdata    = NULL,
    };

    if (miner_start(&cfg, &cbs) != 0) {
        fprintf(stderr, "%sError: failed to start — check node/pool and address%s\n",
                COL_RED, COL_RESET);
        return 1;
    }

    printf("%s[capstash-miner]%s mining on %d thread%s\n",
           COL_GREEN, COL_RESET, cfg.threads,
           cfg.threads == 1 ? "" : "s");

    while (g_running && miner_is_running()) sleep(1);

    if (miner_is_running()) miner_stop();

    miner_stats_t stats;
    miner_get_stats(&stats);
    printf("\n%sSummary:%s hashes=%llu blocks=%u\n\n",
           COL_DIM, COL_RESET,
           (unsigned long long)stats.total_hashes,
           stats.blocks_found);
    return 0;
}
