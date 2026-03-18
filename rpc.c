/**
 * rpc.c — Minimal HTTP RPC client for CapStash miner
 * v3.0 — CLI version (Android deps removed)
 */

#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  #define close(s) closesocket(s)
  typedef int socklen_t;
#else
  #include <unistd.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
#endif

#define LOG_INFO(fmt, ...)  fprintf(stdout, "[rpc] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[rpc] ERROR: " fmt "\n", ##__VA_ARGS__)

#define RPC_TIMEOUT_SEC  10
#define RPC_RECV_BUF     65536
#define RPC_SEND_BUF     262144

// ── Base64 ────────────────────────────────────────────────────────────────
static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const char *in, size_t in_len, char *out) {
    size_t i, j;
    for (i = 0, j = 0; i < in_len;) {
        uint32_t a = i < in_len ? (uint8_t)in[i++] : 0;
        uint32_t b = i < in_len ? (uint8_t)in[i++] : 0;
        uint32_t c = i < in_len ? (uint8_t)in[i++] : 0;
        uint32_t t = (a << 16) | (b << 8) | c;
        out[j++] = B64[(t >> 18) & 0x3F];
        out[j++] = B64[(t >> 12) & 0x3F];
        out[j++] = B64[(t >>  6) & 0x3F];
        out[j++] = B64[(t      ) & 0x3F];
    }
    int pad = in_len % 3;
    if (pad == 1) { out[j-2] = '='; out[j-1] = '='; }
    if (pad == 2) { out[j-1] = '='; }
    out[j] = '\0';
}

// ── TCP connect ───────────────────────────────────────────────────────────
static int rpc_connect(const char *host, int port) {
#ifdef _WIN32
    static int wsa_init = 0;
    if (!wsa_init) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2,2), &wsa);
        wsa_init = 1;
    }
#endif
    struct addrinfo hints = {0}, *res;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        LOG_ERROR("getaddrinfo(%s) failed", host);
        return -1;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { freeaddrinfo(res); return -1; }

#ifdef _WIN32
    int timeout = RPC_TIMEOUT_SEC * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
#else
    struct timeval tv = { RPC_TIMEOUT_SEC, 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

    if (connect(sock, res->ai_addr, (socklen_t)res->ai_addrlen) < 0) {
        LOG_ERROR("connect(%s:%d) failed: %s", host, port, strerror(errno));
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return sock;
}

// ── HTTP POST ─────────────────────────────────────────────────────────────
static char* rpc_post(const rpc_config_t *cfg, const char *json_body) {
    char auth_plain[256], auth_b64[512];
    char *request = NULL;
    char *recv_buf = NULL;
    char *response = NULL;
    int total_recv = 0, n;

    snprintf(auth_plain, sizeof(auth_plain), "%s:%s", cfg->user, cfg->pass);
    base64_encode(auth_plain, strlen(auth_plain), auth_b64);

    int body_len = (int)strlen(json_body);
    request = (char*)malloc(RPC_SEND_BUF);
    if (!request) return NULL;

    snprintf(request, RPC_SEND_BUF,
        "POST / HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Authorization: Basic %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        cfg->host, cfg->port, auth_b64, body_len, json_body
    );

    int sock = rpc_connect(cfg->host, cfg->port);
    if (sock < 0) { free(request); return NULL; }

    if (send(sock, request, strlen(request), 0) < 0) {
        LOG_ERROR("send() failed: %s", strerror(errno));
        close(sock); free(request);
        return NULL;
    }
    free(request);

    recv_buf = (char*)malloc(RPC_RECV_BUF + 1);
    if (!recv_buf) { close(sock); return NULL; }
    memset(recv_buf, 0, RPC_RECV_BUF + 1);

    while ((n = recv(sock, recv_buf + total_recv,
                     RPC_RECV_BUF - total_recv, 0)) > 0) {
        total_recv += n;
        if (total_recv >= RPC_RECV_BUF) break;
    }
    close(sock);

    char *body = strstr(recv_buf, "\r\n\r\n");
    if (!body) { free(recv_buf); return NULL; }
    body += 4;
    response = strdup(body);
    free(recv_buf);
    return response;
}

// ── getblocktemplate ──────────────────────────────────────────────────────
int rpc_getblocktemplate(const rpc_config_t *cfg, block_template_t *tmpl) {
    // CapStash requires segwit rule string even though !segwit is active
    const char *req =
        "{\"jsonrpc\":\"1.0\",\"id\":\"miner\",\"method\":\"getblocktemplate\","
        "\"params\":[{\"rules\":[\"segwit\"]}]}";

    char *resp = rpc_post(cfg, req);
    if (!resp) { LOG_ERROR("getblocktemplate RPC failed"); return -1; }

    int parsed = rpc_parse_template(resp, tmpl);
    free(resp);
    if (parsed != 0) { LOG_ERROR("getblocktemplate parse failed"); return -1; }

    LOG_INFO("template: height=%u bits=%s target=%.8s...",
             tmpl->height, tmpl->bits_hex, tmpl->target_hex);
    return 0;
}

// ── submitblock ───────────────────────────────────────────────────────────
int rpc_submitblock(const rpc_config_t *cfg, const char *block_hex) {
    char *req = (char*)malloc(RPC_SEND_BUF);
    if (!req) return -1;

    snprintf(req, RPC_SEND_BUF,
        "{\"jsonrpc\":\"1.0\",\"id\":\"miner\",\"method\":\"submitblock\","
        "\"params\":[\"%s\"]}", block_hex);

    char *resp = rpc_post(cfg, req);
    free(req);
    if (!resp) { LOG_ERROR("submitblock RPC failed"); return -1; }

    int accepted = (strstr(resp, "\"result\":null") != NULL);
    if (!accepted) LOG_ERROR("submitblock rejected: %s", resp);
    else           LOG_INFO("block accepted!");
    free(resp);
    return accepted ? 0 : -1;
}

// ── getbestblockhash ──────────────────────────────────────────────────────
int rpc_getbestblockhash(const rpc_config_t *cfg, char *out_hash_hex) {
    const char *req =
        "{\"jsonrpc\":\"1.0\",\"id\":\"miner\",\"method\":\"getbestblockhash\","
        "\"params\":[]}";

    char *resp = rpc_post(cfg, req);
    if (!resp) return -1;

    char *start = strstr(resp, "\"result\":\"");
    if (!start) { free(resp); return -1; }
    start += 10;
    char *end = strchr(start, '"');
    if (!end || (end - start) != 64) { free(resp); return -1; }
    strncpy(out_hash_hex, start, 64);
    out_hash_hex[64] = '\0';
    free(resp);
    return 0;
}

// ── JSON template parser ──────────────────────────────────────────────────
static char* json_get_string(const char *json, const char *key,
                              char *out, size_t out_len) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    char *p = strstr(json, search);
    if (!p) return NULL;
    p += strlen(search);
    char *end = strchr(p, '"');
    if (!end) return NULL;
    size_t len = end - p;
    if (len >= out_len) len = out_len - 1;
    strncpy(out, p, len);
    out[len] = '\0';
    return out;
}

static int json_get_uint32(const char *json, const char *key, uint32_t *out) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    char *p = strstr(json, search);
    if (!p) return -1;
    p += strlen(search);
    *out = (uint32_t)strtoul(p, NULL, 10);
    return 0;
}

static int json_get_int64(const char *json, const char *key, int64_t *out) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    char *p = strstr(json, search);
    if (!p) return -1;
    p += strlen(search);
    *out = (int64_t)strtoll(p, NULL, 10);
    return 0;
}

int rpc_parse_template(const char *json, block_template_t *tmpl) {
    memset(tmpl, 0, sizeof(block_template_t));
    if (strstr(json, "\"error\":{") && !strstr(json, "\"error\":null")) {
        LOG_ERROR("RPC returned error in template");
        return -1;
    }
    json_get_uint32(json, "height",    &tmpl->height);
    json_get_uint32(json, "version",   &tmpl->version);
    json_get_uint32(json, "curtime",   &tmpl->curtime);
    json_get_int64 (json, "coinbasevalue", &tmpl->coinbase_value);
    json_get_string(json, "previousblockhash", tmpl->prev_hash_hex, 65);
    json_get_string(json, "bits",              tmpl->bits_hex,      16);
    json_get_string(json, "target",            tmpl->target_hex,    65);
    tmpl->bits = (uint32_t)strtoul(tmpl->bits_hex, NULL, 16);
    return 0;
}
