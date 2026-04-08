#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <vector>

// ============================================================
// Base64 Encoding table
// ============================================================
static const char BASE64_ENCODE_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const uint8_t BASE64_DECODE_TABLE[256] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
     52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
    255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
     15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
    255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
     41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
};

// ============================================================
// Base64 Encoding
// ============================================================
static size_t base64_encode(const uint8_t* src, size_t src_len,
                             char* dst, size_t dst_len)
{
    size_t out_len = ((src_len + 2) / 3) * 4;
    if (dst_len < out_len + 1) return 0;

    const uint8_t* in  = src;
    char*          out = dst;
    size_t         i   = 0;

    for (; i + 3 <= src_len; i += 3) {
        uint32_t v = ((uint32_t)in[i]   << 16) |
                     ((uint32_t)in[i+1] <<  8) |
                      (uint32_t)in[i+2];
        *out++ = BASE64_ENCODE_TABLE[(v >> 18) & 0x3F];
        *out++ = BASE64_ENCODE_TABLE[(v >> 12) & 0x3F];
        *out++ = BASE64_ENCODE_TABLE[(v >>  6) & 0x3F];
        *out++ = BASE64_ENCODE_TABLE[(v      ) & 0x3F];
    }

    size_t rem = src_len - i;
    if (rem == 1) {
        uint32_t v = (uint32_t)in[i] << 16;
        *out++ = BASE64_ENCODE_TABLE[(v >> 18) & 0x3F];
        *out++ = BASE64_ENCODE_TABLE[(v >> 12) & 0x3F];
        *out++ = '=';
        *out++ = '=';
    } else if (rem == 2) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i+1] << 8);
        *out++ = BASE64_ENCODE_TABLE[(v >> 18) & 0x3F];
        *out++ = BASE64_ENCODE_TABLE[(v >> 12) & 0x3F];
        *out++ = BASE64_ENCODE_TABLE[(v >>  6) & 0x3F];
        *out++ = '=';
    }

    *out = '\0';
    return out_len;
}

// ============================================================
// Base64 Decoding
// ============================================================
static size_t base64_decode(const char* src, size_t src_len,
                             uint8_t* dst, size_t dst_len)
{
    if (src_len % 4 != 0) return 0;

    size_t out_len = (src_len / 4) * 3;
    if (src_len >= 4 && src[src_len-1] == '=') out_len--;
    if (src_len >= 4 && src[src_len-2] == '=') out_len--;
    if (dst_len < out_len) return 0;

    const char* in  = src;
    uint8_t*    out = dst;

    for (size_t i = 0; i < src_len; i += 4) {
        uint8_t a = BASE64_DECODE_TABLE[(uint8_t)in[i]];
        uint8_t b = BASE64_DECODE_TABLE[(uint8_t)in[i+1]];
        uint8_t c = BASE64_DECODE_TABLE[(uint8_t)in[i+2]];
        uint8_t d = BASE64_DECODE_TABLE[(uint8_t)in[i+3]];

        uint32_t v = ((uint32_t)a << 18) | ((uint32_t)b << 12) |
                     ((uint32_t)c <<  6) |  (uint32_t)d;

        *out++ = (v >> 16) & 0xFF;
        if (in[i+2] != '=') *out++ = (v >> 8) & 0xFF;
        if (in[i+3] != '=') *out++ =  v       & 0xFF;
    }

    return out_len;
}

// ============================================================
// Timing Utility
// ============================================================
static double now_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

// ============================================================
// Generate Fixed Pseudo-Random Data (Fixed Seed for Consistency)
// ============================================================
static void fill_fixed(uint8_t* buf, size_t len)
{
    uint32_t state = 0xDEADBEEF;
    for (size_t i = 0; i < len; i++) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        buf[i] = (uint8_t)(state & 0xFF);
    }
}

// ============================================================
// main
// ============================================================
int main()
{
    // ---------- Configuration ----------
    static const size_t DATA_SIZE  = 16 * 1024;   // 16 KB
    static const int    ITERATIONS = 100;          // Number of iterations

    // ---------- Buffer Preparation ----------
    const size_t enc_buf_size = ((DATA_SIZE + 2) / 3) * 4 + 4;

    std::vector<uint8_t> src(DATA_SIZE);
    std::vector<char>    enc(enc_buf_size);
    std::vector<uint8_t> dec(DATA_SIZE + 4);

    // Generate fixed content (fixed seed for consistency)
    fill_fixed(src.data(), DATA_SIZE);

    printf("================================================\n");
    printf("  Base64 Benchmark  |  16 KB  |  %d iterations\n", ITERATIONS);
    printf("================================================\n\n");

    // ---------- Timing ----------
    double encode_total_ms = 0.0;
    double decode_total_ms = 0.0;
    size_t enc_len = 0;
    size_t dec_len = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        // -- Encoding --
        double t0 = now_ms();
        enc_len = base64_encode(src.data(), DATA_SIZE,
                                enc.data(), enc_buf_size);
        double t1 = now_ms();
        encode_total_ms += (t1 - t0);

        // -- Decoding --
        double t2 = now_ms();
        dec_len = base64_decode(enc.data(), enc_len,
                                dec.data(), dec.size());
        double t3 = now_ms();
        decode_total_ms += (t3 - t2);

        printf("  [%3d/%d]  encode: %.4f ms  decode: %.4f ms\n",
               i + 1, ITERATIONS, t1 - t0, t3 - t2);
    }

    // ---------- Correctness Verification ----------
    bool correct = (dec_len == DATA_SIZE) &&
                   (memcmp(src.data(), dec.data(), DATA_SIZE) == 0);

    // ---------- Statistics ----------
    double encode_avg_ms = encode_total_ms / ITERATIONS;
    double decode_avg_ms = decode_total_ms / ITERATIONS;
    double data_mb       = (double)DATA_SIZE / (1024.0 * 1024.0);

    double encode_throughput = data_mb / (encode_avg_ms / 1000.0); // MB/s
    double decode_throughput = data_mb / (decode_avg_ms / 1000.0); // MB/s

    printf("\n================================================\n");
    printf("  Results\n");
    printf("================================================\n");
    printf("  Data size        : %zu bytes (16 KB)\n",  DATA_SIZE);
    printf("  Encoded size     : %zu bytes\n",           enc_len);
    printf("  Iterations       : %d\n",                  ITERATIONS);
    printf("  Correctness      : %s\n",                  correct ? "PASS ✓" : "FAIL ✗");
    printf("------------------------------------------------\n");
    printf("  Encode total     : %.4f ms\n",             encode_total_ms);
    printf("  Encode avg       : %.4f ms\n",             encode_avg_ms);
    printf("  Encode throughput: %.2f MB/s\n",           encode_throughput);
    printf("------------------------------------------------\n");
    printf("  Decode total     : %.4f ms\n",             decode_total_ms);
    printf("  Decode avg       : %.4f ms\n",             decode_avg_ms);
    printf("  Decode throughput: %.2f MB/s\n",           decode_throughput);
    printf("================================================\n\n");

    return correct ? 0 : 1;
}