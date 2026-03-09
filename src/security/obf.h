#pragma once
#include <string>
#include <array>
#include <cstdint>

#ifdef _MSC_VER
#pragma constexpr_depth(512)
#pragma constexpr_steps(1000000)
#endif

namespace _OBF {

  
    constexpr uint32_t _S1 = (__TIME__[0] ^ __TIME__[1] ^ __TIME__[3]) | 0x01;
    constexpr uint32_t _S2 = (__TIME__[4] ^ __TIME__[6] ^ __TIME__[7]) | 0x03;
    constexpr uint32_t _S3 = (__DATE__[0] ^ __DATE__[2] ^ __DATE__[4]) | 0x07;
    constexpr uint32_t _S4 = (__DATE__[5] ^ __DATE__[7] ^ __DATE__[9]) | 0x0F;
    constexpr uint32_t _S5 = (_S1 ^ _S2 ^ _S3 ^ _S4) | 0x1F;
    
    
    constexpr uint32_t _prng(uint32_t seed, size_t round) {
        uint32_t s = seed;
        for (size_t i = 0; i < round + 1; i++) {
            s ^= s << 13;
            s ^= s >> 17;
            s ^= s << 5;
        }
        return s;
    }

    
    constexpr unsigned char _SBOX[256] = {
        0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
        0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
        0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
        0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
        0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
        0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
        0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
        0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
        0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
        0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
        0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
        0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
        0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
        0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
        0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
        0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
    };
    
    constexpr unsigned char _INV_SBOX[256] = {
        0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
        0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
        0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
        0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
        0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
        0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
        0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
        0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
        0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
        0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
        0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
        0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
        0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
        0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
        0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
        0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
    };

    
    constexpr uint32_t XTEA_DELTA = 0x9E3779B9;
    
    
    constexpr void _deriveXteaKey(uint32_t seed, size_t idx, uint32_t key[4]) {
        key[0] = _prng(seed ^ 0xDEADBEEF, idx);
        key[1] = _prng(key[0] ^ 0xCAFEBABE, idx + 1);
        key[2] = _prng(key[1] ^ 0x8BADF00D, idx + 2);
        key[3] = _prng(key[2] ^ 0xFEEDFACE, idx + 3);
    }
    
    
    constexpr void _xteaEncrypt(uint32_t v[2], const uint32_t key[4]) {
        uint32_t v0 = v[0], v1 = v[1];
        uint32_t sum = 0;
        
        #define XTEA_ENC_ROUND() \
            v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]); \
            sum += XTEA_DELTA; \
            v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3])
        
        
        XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND();
        XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND();
        XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND();
        XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND();
        XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND();
        XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND();
        XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND();
        XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND(); XTEA_ENC_ROUND();
        
        #undef XTEA_ENC_ROUND
        
        v[0] = v0;
        v[1] = v1;
    }
    
    
    constexpr void _xteaDecrypt(uint32_t v[2], const uint32_t key[4]) {
        uint32_t v0 = v[0], v1 = v[1];
        uint32_t sum = XTEA_DELTA * 32;
        
        #define XTEA_DEC_ROUND() \
            v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]); \
            sum -= XTEA_DELTA; \
            v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3])
        
        
        XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND();
        XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND();
        XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND();
        XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND();
        XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND();
        XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND();
        XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND();
        XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND(); XTEA_DEC_ROUND();
        
        #undef XTEA_DEC_ROUND
        
        v[0] = v0;
        v[1] = v1;
    }

    
    
    constexpr char _e1(char c, size_t idx, uint32_t seed) {
        
        uint32_t s0 = seed ^ 0xB16B00B5 ^ (uint32_t)idx;
        uint32_t s1 = seed * 0x9E3779B9 ^ (uint32_t)(idx * 0x85EBCA6B);
        
        
        s0 += s1; s1 = ((s1 << 7) | (s1 >> 25)); s1 ^= s0;
        s0 += s1; s1 = ((s1 << 9) | (s1 >> 23)); s1 ^= s0;
        s0 += s1; s1 = ((s1 << 13) | (s1 >> 19)); s1 ^= s0;
        s0 += s1; s1 = ((s1 << 18) | (s1 >> 14)); s1 ^= s0;
        
        
        unsigned char k0 = (unsigned char)(s0 & 0xFF);
        unsigned char k1 = (unsigned char)((s0 >> 8) & 0xFF);
        unsigned char k2 = (unsigned char)((s1 >> 16) & 0xFF);
        
        
        unsigned char state = (unsigned char)c;
        state ^= k0;
        state = _SBOX[state];
        state ^= k1;
        state ^= k2;
        state = _SBOX[state];
        
        return (char)state;
    }
    
    constexpr char _d1(char c, size_t idx, uint32_t seed) {
        
        uint32_t s0 = seed ^ 0xB16B00B5 ^ (uint32_t)idx;
        uint32_t s1 = seed * 0x9E3779B9 ^ (uint32_t)(idx * 0x85EBCA6B);
        
        
        s0 += s1; s1 = ((s1 << 7) | (s1 >> 25)); s1 ^= s0;
        s0 += s1; s1 = ((s1 << 9) | (s1 >> 23)); s1 ^= s0;
        s0 += s1; s1 = ((s1 << 13) | (s1 >> 19)); s1 ^= s0;
        s0 += s1; s1 = ((s1 << 18) | (s1 >> 14)); s1 ^= s0;
        
        
        unsigned char k0 = (unsigned char)(s0 & 0xFF);
        unsigned char k1 = (unsigned char)((s0 >> 8) & 0xFF);
        unsigned char k2 = (unsigned char)((s1 >> 16) & 0xFF);
        
        
        unsigned char state = (unsigned char)c;
        state = _INV_SBOX[state];
        state ^= k2;
        state ^= k1;
        state = _INV_SBOX[state];
        state ^= k0;
        
        return (char)state;
    }

    
    
    constexpr char _e2(char c, size_t idx, uint32_t seed) {
        
        uint32_t s0 = seed ^ 0xDEADC0DE ^ (uint32_t)idx;
        uint32_t s1 = seed * 0x85EBCA6B ^ (uint32_t)(idx * 0xC2B2AE35);
        
        
        s0 += s1; s1 = ((s1 << 7) | (s1 >> 25)); s1 ^= s0;
        s0 += s1; s1 = ((s1 << 9) | (s1 >> 23)); s1 ^= s0;
        s0 += s1; s1 = ((s1 << 13) | (s1 >> 19)); s1 ^= s0;
        s0 += s1; s1 = ((s1 << 18) | (s1 >> 14)); s1 ^= s0;
        
        
        unsigned char k0 = (unsigned char)(s0 & 0xFF);
        unsigned char k1 = (unsigned char)((s0 >> 8) & 0xFF);
        unsigned char k2 = (unsigned char)((s0 >> 16) & 0xFF);
        unsigned char k3 = (unsigned char)((s1) & 0xFF);
        unsigned char k4 = (unsigned char)((s1 >> 8) & 0xFF);
        
        
        unsigned char state = (unsigned char)c;
        state ^= k0;
        state = _SBOX[state];
        state ^= k1;
        state = _SBOX[state];
        state ^= k2;
        state ^= k3;
        state = _SBOX[state];
        state ^= k4;
        
        return (char)state;
    }
    
    constexpr char _d2(char c, size_t idx, uint32_t seed) {
        
        uint32_t s0 = seed ^ 0xDEADC0DE ^ (uint32_t)idx;
        uint32_t s1 = seed * 0x85EBCA6B ^ (uint32_t)(idx * 0xC2B2AE35);
        
        
        s0 += s1; s1 = ((s1 << 7) | (s1 >> 25)); s1 ^= s0;
        s0 += s1; s1 = ((s1 << 9) | (s1 >> 23)); s1 ^= s0;
        s0 += s1; s1 = ((s1 << 13) | (s1 >> 19)); s1 ^= s0;
        s0 += s1; s1 = ((s1 << 18) | (s1 >> 14)); s1 ^= s0;
        
        
        unsigned char k0 = (unsigned char)(s0 & 0xFF);
        unsigned char k1 = (unsigned char)((s0 >> 8) & 0xFF);
        unsigned char k2 = (unsigned char)((s0 >> 16) & 0xFF);
        unsigned char k3 = (unsigned char)((s1) & 0xFF);
        unsigned char k4 = (unsigned char)((s1 >> 8) & 0xFF);
        
        
        unsigned char state = (unsigned char)c;
        state ^= k4;
        state = _INV_SBOX[state];
        state ^= k3;
        state ^= k2;
        state = _INV_SBOX[state];
        state ^= k1;
        state = _INV_SBOX[state];
        state ^= k0;
        
        return (char)state;
    }

    
    constexpr char _e3(char c, size_t idx, uint32_t seed) {
        uint32_t r = _prng(seed, idx);
        char k1 = (char)(r & 0xFF);
        char k2 = (char)((r >> 8) & 0xFF);
        char k3 = (char)((r >> 16) & 0xFF);
        
        char result = c;
        result ^= k1;
        result = (char)(((unsigned char)result << 5) | ((unsigned char)result >> 3));
        result ^= k2;
        result = (char)(((unsigned char)result << 2) | ((unsigned char)result >> 6));
        result ^= k3;
        result ^= (char)((idx * idx) & 0xFF);
        return result;
    }
    
    constexpr char _d3(char c, size_t idx, uint32_t seed) {
        uint32_t r = _prng(seed, idx);
        char k1 = (char)(r & 0xFF);
        char k2 = (char)((r >> 8) & 0xFF);
        char k3 = (char)((r >> 16) & 0xFF);
        
        char result = c;
        result ^= (char)((idx * idx) & 0xFF);
        result ^= k3;
        result = (char)(((unsigned char)result >> 2) | ((unsigned char)result << 6));
        result ^= k2;
        result = (char)(((unsigned char)result >> 5) | ((unsigned char)result << 3));
        result ^= k1;
        return result;
    }

    
    constexpr char _e4(char c, size_t idx, uint32_t seed) {
        uint32_t r = _prng(seed ^ 0xDEADBEEF, idx);
        char k1 = (char)(r & 0xFF);
        char k2 = (char)((r >> 8) & 0xFF);
        
        char result = (char)((unsigned char)c - (unsigned char)((idx + 1) * 7));
        result = ~result;
        result ^= k1;
        result = (char)(((unsigned char)result << 6) | ((unsigned char)result >> 2));
        result ^= k2;
        result ^= (char)((idx * 0x9D) & 0xFF);
        return result;
    }
    
    constexpr char _d4(char c, size_t idx, uint32_t seed) {
        uint32_t r = _prng(seed ^ 0xDEADBEEF, idx);
        char k1 = (char)(r & 0xFF);
        char k2 = (char)((r >> 8) & 0xFF);
        
        char result = c;
        result ^= (char)((idx * 0x9D) & 0xFF);
        result ^= k2;
        result = (char)(((unsigned char)result >> 6) | ((unsigned char)result << 2));
        result ^= k1;
        result = ~result;
        result = (char)((unsigned char)result + (unsigned char)((idx + 1) * 7));
        return result;
    }

    
    constexpr char _e5(char c, size_t idx, uint32_t seed) {
        uint32_t r = _prng(seed ^ 0xCAFEBABE, idx);
        
        unsigned char left = ((unsigned char)c >> 4) & 0x0F;
        unsigned char right = (unsigned char)c & 0x0F;
        
        for (int round = 0; round < 4; round++) {
            uint32_t rk = _prng(r, round);
            unsigned char f = (right ^ ((rk >> (round * 4)) & 0x0F)) & 0x0F;
            f = ((f << 1) | (f >> 3)) & 0x0F;
            unsigned char newRight = left ^ f;
            left = right;
            right = newRight;
        }
        
        char result = (char)((left << 4) | right);
        result ^= (char)((idx * 0x73) & 0xFF);
        return result;
    }
    
    constexpr char _d5(char c, size_t idx, uint32_t seed) {
        uint32_t r = _prng(seed ^ 0xCAFEBABE, idx);
        
        char result = c;
        result ^= (char)((idx * 0x73) & 0xFF);
        
        unsigned char left = ((unsigned char)result >> 4) & 0x0F;
        unsigned char right = (unsigned char)result & 0x0F;
        
        for (int round = 3; round >= 0; round--) {
            uint32_t rk = _prng(r, round);
            unsigned char f = (left ^ ((rk >> (round * 4)) & 0x0F)) & 0x0F;
            f = ((f << 1) | (f >> 3)) & 0x0F;
            unsigned char newLeft = right ^ f;
            right = left;
            left = newLeft;
        }
        
        return (char)((left << 4) | right);
    }

    
    
    
    template<size_t N, uint32_t SEED = _S1>
    struct _T1 {
        char d[N];
        uint8_t cs;
        
        constexpr _T1(const char(&str)[N]) : d{}, cs(0) {
            uint8_t checksum = 0x55;
            for (size_t i = 0; i < N; ++i) {
                d[i] = _e1(str[i], i, SEED);
                checksum ^= (uint8_t)str[i];
                checksum = (checksum << 1) | (checksum >> 7);
            }
            cs = checksum ^ 0xAA;
        }
        
        __forceinline std::string _d() const {
            std::string r;
            r.reserve(N - 1);
            for (size_t i = 0; i < N - 1; ++i) {
                r += _d1(d[i], i, SEED);
            }
            return r;
        }
    };

    
    template<size_t N, uint32_t SEED = _S2>
    struct _T2 {
        char d[N];
        
        constexpr _T2(const char(&str)[N]) : d{} {
            for (size_t i = 0; i < N; ++i) {
                d[i] = _e2(str[i], i, SEED);
            }
        }
        
        __forceinline std::string _d() const {
            std::string r;
            r.reserve(N - 1);
            for (size_t i = 0; i < N - 1; ++i) {
                r += _d2(d[i], i, SEED);
            }
            return r;
        }
    };

    
    template<size_t N, uint32_t SEED = _S3>
    struct _T3 {
        char d[N];
        
        constexpr _T3(const char(&str)[N]) : d{} {
            for (size_t i = 0; i < N; ++i) {
                d[i] = _e3(str[i], i, SEED);
            }
        }
        
        __forceinline std::string _d() const {
            std::string r;
            r.reserve(N - 1);
            for (size_t i = 0; i < N - 1; ++i) {
                r += _d3(d[i], i, SEED);
            }
            return r;
        }
    };

    
    template<size_t N, uint32_t SEED = _S4>
    struct _T4 {
        char d[N];
        
        constexpr _T4(const char(&str)[N]) : d{} {
            for (size_t i = 0; i < N; ++i) {
                d[i] = _e4(str[i], i, SEED);
            }
        }
        
        __forceinline std::string _d() const {
            std::string r;
            r.reserve(N - 1);
            for (size_t i = 0; i < N - 1; ++i) {
                r += _d4(d[i], i, SEED);
            }
            return r;
        }
    };

    
    template<size_t N, uint32_t SEED = _S5>
    struct _T5 {
        char d[N];
        
        constexpr _T5(const char(&str)[N]) : d{} {
            for (size_t i = 0; i < N; ++i) {
                d[i] = _e5(str[i], i, SEED);
            }
        }
        
        __forceinline std::string _d() const {
            std::string r;
            r.reserve(N - 1);
            for (size_t i = 0; i < N - 1; ++i) {
                r += _d5(d[i], i, SEED);
            }
            return r;
        }
    };

    
    
    template<size_t N, uint32_t SEED = _S1>
    struct _W1 {
        wchar_t d[N];
        
        constexpr _W1(const wchar_t(&str)[N]) : d{} {
            for (size_t i = 0; i < N; ++i) {
                uint32_t r = _prng(SEED, i);
                d[i] = str[i] ^ (wchar_t)(r & 0xFFFF) ^ (wchar_t)(i * 0x1337);
            }
        }
        
        __forceinline std::wstring _d() const {
            std::wstring r;
            r.reserve(N - 1);
            for (size_t i = 0; i < N - 1; ++i) {
                uint32_t rng = _prng(SEED, i);
                r += d[i] ^ (wchar_t)(rng & 0xFFFF) ^ (wchar_t)(i * 0x1337);
            }
            return r;
        }
    };

    
    
    __forceinline void _sw(std::string& s) {
        volatile char* p = const_cast<char*>(s.data());
        size_t len = s.size();
        for (size_t i = 0; i < len; i++) { p[i] = (char)(i ^ 0xFF); }
        for (size_t i = 0; i < len; i++) { p[i] = 0; }
        s.clear();
    }

    __forceinline void _sw(std::wstring& s) {
        volatile wchar_t* p = const_cast<wchar_t*>(s.data());
        size_t len = s.size();
        for (size_t i = 0; i < len; i++) { p[i] = (wchar_t)(i ^ 0xFFFF); }
        for (size_t i = 0; i < len; i++) { p[i] = 0; }
        s.clear();
    }

    __forceinline void _sw(char* buf, size_t len) {
        volatile char* p = buf;
        for (size_t i = 0; i < len; i++) { p[i] = (char)(i ^ 0xFF); }
        for (size_t i = 0; i < len; i++) { p[i] = 0; }
    }

    
    
    template<size_t N>
    class _SS {
    private:
        char b[N];
        size_t l;
        
    public:
        _SS() : b{}, l(0) {
            for (size_t i = 0; i < N; i++) b[i] = (char)(i & 0xFF);
            for (size_t i = 0; i < N; i++) b[i] = 0;
        }
        
        void _a(char c) { if (l < N - 1) { b[l++] = c; b[l] = '\0'; } }
        void _a(const char* s) { while (*s && l < N - 1) { b[l++] = *s++; } b[l] = '\0'; }
        const char* _c() const { return b; }
        size_t _s() const { return l; }
        void _w() { _sw(b, N); l = 0; }
        ~_SS() { _w(); }
    };

    
    
    constexpr uint64_t _hash(const char* str, size_t len) {
        uint64_t h = 0x5A5A5A5A5A5A5A5AULL;
        for (size_t i = 0; i < len; i++) {
            h ^= (uint64_t)(unsigned char)str[i];
            h *= 0x100000001B3ULL;
            h ^= h >> 33;
        }
        return h;
    }

    template<size_t N>
    constexpr uint64_t _shash(const char(&str)[N]) {
        return _hash(str, N - 1);
    }

}




#define OBF1(str) (_OBF::_T1<sizeof(str)>(str)._d())


#define OBF2(str) (_OBF::_T2<sizeof(str)>(str)._d())


#define OBF3(str) (_OBF::_T3<sizeof(str)>(str)._d())


#define OBF4(str) (_OBF::_T4<sizeof(str)>(str)._d())


#define OBF5(str) (_OBF::_T5<sizeof(str)>(str)._d())


#define OBF(str) OBF3(str)


#define OBFW(str) (_OBF::_W1<sizeof(str)/sizeof(wchar_t)>(str)._d())


#define OBF1_WIPE(str, var) std::string var = OBF1(str); struct _AW1_##var { std::string& s; ~_AW1_##var() { _OBF::_sw(s); } } _aw1_##var{var};
#define OBF2_WIPE(str, var) std::string var = OBF2(str); struct _AW2_##var { std::string& s; ~_AW2_##var() { _OBF::_sw(s); } } _aw2_##var{var};
#define OBF3_WIPE(str, var) std::string var = OBF3(str); struct _AW3_##var { std::string& s; ~_AW3_##var() { _OBF::_sw(s); } } _aw3_##var{var};
#define OBF4_WIPE(str, var) std::string var = OBF4(str); struct _AW4_##var { std::string& s; ~_AW4_##var() { _OBF::_sw(s); } } _aw4_##var{var};
#define OBF5_WIPE(str, var) std::string var = OBF5(str); struct _AW5_##var { std::string& s; ~_AW5_##var() { _OBF::_sw(s); } } _aw5_##var{var};
#define OBF_WIPE(str, var) OBF3_WIPE(str, var)


#define STR_HASH(str) (_OBF::_shash(str))
