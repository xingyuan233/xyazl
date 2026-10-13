#include "../hashtable.h"

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#elif defined(__aarch64__) || defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace xyu
{
#if defined(__aarch64__) || defined(__ARM_NEON)
    inline uint16 neon_movemask(uint8x16_t v) noexcept
    {
        const uint8x16_t bit_mask = { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
        uint8x16_t minput = vandq_u8(v, bit_mask);
        uint8x8_t tmp = vpadd_u8(vget_low_u8(minput), vget_high_u8(minput));
        tmp = vpadd_u8(tmp, tmp);
        tmp = vpadd_u8(tmp, tmp);
        return vget_lane_u16(vreinterpret_u16_u8(tmp), 0);
    }
#endif

    uint16 msb(const uint8* ctrl) noexcept
    {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        const __m128i metas = _mm_load_si128(reinterpret_cast<const __m128i*>(ctrl));
        return _mm_movemask_epi8(metas);
#elif defined(__aarch64__) || defined(__ARM_NEON)
        const uint8x16_t metas = vld1q_u8(ctrl);
        const uint8x16_t mask = vcltq_s8(vreinterpretq_s8_u8(metas), vdupq_n_s8(0));
        return neon_movemask(mask);
#else
        uint16 mask = 0;
        for (uint i = 0; i < 16; i++)
            if (ctrl[i] & 0x80) mask |= (1 << i);
        return mask;
#endif
    }

    uint16 cmpeq(const uint8* ctrl, uint8 val) noexcept
    {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        const __m128i metas = _mm_load_si128(reinterpret_cast<const __m128i*>(ctrl));
        const __m128i cmp = _mm_set1_epi8(val);
        return _mm_movemask_epi8(_mm_cmpeq_epi8(metas, cmp));
#elif defined(__aarch64__) || defined(__ARM_NEON)
        const uint8x16_t metas = vld1q_u8(ctrl);
        const uint8x16_t cmp = vdupq_n_u8(val);
        return neon_movemask(vceqq_u8(metas, cmp));
#else
        uint16 mask = 0;
        for (uint i = 0; i < 16; i++)
            if (ctrl[i] == val) mask |= (1 << i);
        return mask;
#endif
    }
}
