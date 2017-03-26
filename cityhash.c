#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

/*
 * Based on the city.cc from https://github.com/google/cityhash/
 * Ported to C99. 64b stuff removed.
 */

static uint32_t UNALIGNED_LOAD32(const char *p) {
    uint32_t result;
    memcpy(&result, p, sizeof(result));
    return result;
}

#ifdef _MSC_VER

#include <stdlib.h>
#define bswap_32(x) _byteswap_ulong(x)

#elif defined(__APPLE__)

// Mac OS X / Darwin features
#include <libkern/OSByteOrder.h>
#define bswap_32(x) OSSwapInt32(x)

#elif defined(__sun) || defined(sun)

#include <sys/byteorder.h>
#define bswap_32(x) BSWAP_32(x)

#elif defined(__FreeBSD__)

#include <sys/endian.h>
#define bswap_32(x) bswap32(x)

#elif defined(__OpenBSD__)

#include <sys/types.h>
#define bswap_32(x) swap32(x)

#elif defined(__NetBSD__)

#include <sys/types.h>
#include <machine/bswap.h>
#if defined(__BSWAP_RENAME) && !defined(__bswap_32)
#define bswap_32(x) bswap32(x)
#endif

#else

#include <byteswap.h>

#endif

#ifdef WORDS_BIGENDIAN
#define uint32_in_expected_order(x) (bswap_32(x))
#else
#define uint32_in_expected_order(x) (x)
#endif

static uint32_t Fetch32(const char *p) {
    return uint32_in_expected_order(UNALIGNED_LOAD32(p));
}

// Magic numbers for 32-bit hashing.  Copied from Murmur3.
static const uint32_t c1 = 0xcc9e2d51;
static const uint32_t c2 = 0x1b873593;

static uint32_t fmix(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

static uint32_t Rotate32(uint32_t val, int shift) {
    // Avoid shifting by 32: doing so yields an undefined result.
    return shift == 0 ? val : ((val >> shift) | (val << (32 - shift)));
}

static uint32_t Mur(uint32_t a, uint32_t h) {
    // Helper from Murmur3 for combining two 32-bit values.
    a *= c1;
    a = Rotate32(a, 17);
    a *= c2;
    h ^= a;
    h = Rotate32(h, 19);
    return h * 5 + 0xe6546b64;
}

static uint32_t Hash32Len0to4(const char *s, size_t len) {
    uint32_t b = 0;
    uint32_t c = 9;
    for (size_t i = 0; i < len; i++) {
        signed char v = s[i];
        b = b * c1 + (unsigned int)v;
        c ^= b;
    }
    return fmix(Mur(b, Mur((uint32_t)len, c)));
}

static uint32_t Hash32Len5to12(const char *s, size_t len) {
    uint32_t a = (uint32_t)len, b = (uint32_t)(len * 5), c = 9, d = b;
    a += Fetch32(s);
    b += Fetch32(s + len - 4);
    c += Fetch32(s + ((len >> 1) & 4));
    return fmix(Mur(c, Mur(b, Mur(a, d))));
}

static uint32_t Hash32Len13to24(const char *s, size_t len) {
    uint32_t a = Fetch32(s - 4 + (len >> 1));
    uint32_t b = Fetch32(s + 4);
    uint32_t c = Fetch32(s + len - 8);
    uint32_t d = Fetch32(s + (len >> 1));
    uint32_t e = Fetch32(s);
    uint32_t f = Fetch32(s + len - 4);
    uint32_t h = (uint32_t)len;

    return fmix(Mur(f, Mur(e, Mur(d, Mur(c, Mur(b, Mur(a, h)))))));
}

uint32_t CityHash32(const char *s, size_t len)
{
    if (len <= 4) {
        return Hash32Len0to4(s, len);
    }
    if (len <= 12) {
        return Hash32Len5to12(s, len);
    }
    if (len <= 24) {
        return Hash32Len13to24(s, len);
    }

    // len > 24
    uint32_t h = (uint32_t)len;
    uint32_t g = (uint32_t)(c1 * len);
    uint32_t f = g;
    {
        uint32_t a0 = Rotate32(Fetch32(s + len - 4) * c1, 17) * c2;
        uint32_t a1 = Rotate32(Fetch32(s + len - 8) * c1, 17) * c2;
        uint32_t a2 = Rotate32(Fetch32(s + len - 16) * c1, 17) * c2;
        uint32_t a3 = Rotate32(Fetch32(s + len - 12) * c1, 17) * c2;
        uint32_t a4 = Rotate32(Fetch32(s + len - 20) * c1, 17) * c2;
        h ^= a0;
        h = Rotate32(h, 19);
        h = h * 5 + 0xe6546b64;
        h ^= a2;
        h = Rotate32(h, 19);
        h = h * 5 + 0xe6546b64;
        g ^= a1;
        g = Rotate32(g, 19);
        g = g * 5 + 0xe6546b64;
        g ^= a3;
        g = Rotate32(g, 19);
        g = g * 5 + 0xe6546b64;
        f += a4;
        f = Rotate32(f, 19);
        f = f * 5 + 0xe6546b64;
    }
    size_t iters = (len - 1) / 20;
    do {
        uint32_t a0 = Rotate32(Fetch32(s) * c1, 17) * c2;
        uint32_t a1 = Fetch32(s + 4);
        uint32_t a2 = Rotate32(Fetch32(s + 8) * c1, 17) * c2;
        uint32_t a3 = Rotate32(Fetch32(s + 12) * c1, 17) * c2;
        uint32_t a4 = Fetch32(s + 16);
        h ^= a0;
        h = Rotate32(h, 18);
        h = h * 5 + 0xe6546b64;
        f += a1;
        f = Rotate32(f, 19);
        f = f * c1;
        g += a2;
        g = Rotate32(g, 18);
        g = g * 5 + 0xe6546b64;
        h ^= a3 + a1;
        h = Rotate32(h, 19);
        h = h * 5 + 0xe6546b64;
        g ^= a4;
        g = bswap_32(g) * 5;
        h += a4 * 5;
        h = bswap_32(h);
        f += a0;

        {
            // PERMUTE3(f, h, g) => g, f, h
            uint32_t swap = g;
            g = h;
            h = f;
            f = swap;
        }

        s += 20;
    } while (--iters != 0);
    g = Rotate32(g, 11) * c1;
    g = Rotate32(g, 17) * c1;
    f = Rotate32(f, 11) * c1;
    f = Rotate32(f, 17) * c1;
    h = Rotate32(h + g, 19);
    h = h * 5 + 0xe6546b64;
    h = Rotate32(h, 17) * c1;
    h = Rotate32(h + f, 19);
    h = h * 5 + 0xe6546b64;
    h = Rotate32(h, 17) * c1;
    return h;
}
