/* Compact SHA-256 (FIPS 180-4). Validated against NIST vectors in tests/unit/test_crc_sha.c */
#include "sha256.h"
#include <string.h>

static const uint32_t K[64] = {
 0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
 0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
 0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
 0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
 0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
 0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
 0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
 0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u };

#define ROR(x,n) (((x) >> (n)) | ((x) << (32u - (n))))

static void block(sha256_ctx_t *c, const uint8_t *p)
{
    uint32_t w[64];
    for (uint32_t i = 0u; i < 16u; i++) {
        w[i] = ((uint32_t)p[4u*i] << 24) | ((uint32_t)p[4u*i+1u] << 16) |
               ((uint32_t)p[4u*i+2u] << 8) | (uint32_t)p[4u*i+3u];
    }
    for (uint32_t i = 16u; i < 64u; i++) {
        uint32_t s0 = ROR(w[i-15u],7) ^ ROR(w[i-15u],18) ^ (w[i-15u] >> 3);
        uint32_t s1 = ROR(w[i-2u],17) ^ ROR(w[i-2u],19) ^ (w[i-2u] >> 10);
        w[i] = w[i-16u] + s0 + w[i-7u] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],h=c->h[7];
    for (uint32_t i = 0u; i < 64u; i++) {
        uint32_t S1 = ROR(e,6) ^ ROR(e,11) ^ ROR(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = ROR(a,2) ^ ROR(a,13) ^ ROR(a,22);
        uint32_t mj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1; d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d; c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}

void sha256_init(sha256_ctx_t *c)
{
    static const uint32_t iv[8] = {0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                                   0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    memcpy(c->h, iv, sizeof iv);
    c->buflen = 0u; c->total = 0u;
}

void sha256_update(sha256_ctx_t *c, const uint8_t *d, size_t n)
{
    c->total += n;
    while (n > 0u) {
        size_t take = 64u - c->buflen;
        if (take > n) { take = n; }
        memcpy(&c->buf[c->buflen], d, take);
        c->buflen += (uint32_t)take; d += take; n -= take;
        if (c->buflen == 64u) { block(c, c->buf); c->buflen = 0u; }
    }
}

void sha256_final(sha256_ctx_t *c, uint8_t out[32])
{
    uint64_t bits = c->total * 8u;
    uint8_t pad = 0x80u;
    sha256_update(c, &pad, 1u);
    pad = 0u;
    while (c->buflen != 56u) { sha256_update(c, &pad, 1u); }
    uint8_t len[8];
    for (uint32_t i = 0u; i < 8u; i++) { len[i] = (uint8_t)(bits >> (56u - 8u*i)); }
    sha256_update(c, len, 8u);
    for (uint32_t i = 0u; i < 8u; i++) {
        out[4u*i] = (uint8_t)(c->h[i] >> 24); out[4u*i+1u] = (uint8_t)(c->h[i] >> 16);
        out[4u*i+2u] = (uint8_t)(c->h[i] >> 8); out[4u*i+3u] = (uint8_t)c->h[i];
    }
}

void sha256_calc(const uint8_t *d, size_t n, uint8_t out[32])
{
    sha256_ctx_t c; sha256_init(&c); sha256_update(&c, d, n); sha256_final(&c, out);
}
