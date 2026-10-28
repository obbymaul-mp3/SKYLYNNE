/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 ChoralRoot FM-1 contributors (a fork of Felucca) */
/* What GRAIN (eng_grain.c) takes from the VA in ChoralRoot's va_common.c and 3IS's eng_va.c does not have: the LFO sync
 * list, the matrix amounts' fine rule. (The filter's type weights, FTYPE's names, the LFO wave names and the VA_R*
 * macros are in eng_va.c here; included after it by eng_grain.c, once.) */
#ifndef VA_COMMON_C
#define VA_COMMON_C
#define VA_NSYNC 42u             /* LFO SYNC since version 4: a synced RATE is an index into N_VA_SYNC (va_lfo_inc) */

/* a synced LFO's RATE (version 4, the owner 2026-10-14): slowest first, so turning the knob up is faster, as the free
 * rate: 8 4 3 2 1 bars, 1/2 .. 1/256, each dotted (D, 3/2), straight, triplet (T, 2/3); past 256T free periods in
 * ms. At most 5 characters (params.c F_ENUM): 1/128's and 1/256's T and D drop the "1/" */
static const char *const N_VA_SYNC[VA_NSYNC + 1u] = {"8BARD", "8BAR", "8BART", "4BARD", "4BAR", "4BART", "3BARD",
    "3BAR", "3BART", "2BARD", "2BAR", "2BART", "1BARD", "1BAR", "1BART", "1/2D", "1/2", "1/2T", "1/4D", "1/4", "1/4T",
    "1/8D", "1/8", "1/8T", "1/16D", "1/16", "1/16T", "1/32D", "1/32", "1/32T", "1/64D", "1/64", "1/64T", "128D",
    "1/128", "128T", "256D", "1/256", "256T", "4ms", "3ms", "2ms", 0};

/* the AMT column's values (hundredths) at or below v, at or above v: tenths between -6 and 6, whole numbers outside */
static int32_t va_amt_floor(int32_t v)
{
    int32_t g = v > -600 && v < 600 ? 10 : 100;
    return v >= 0 ? v / g * g : -((-v + g - 1) / g * g);
}
static int32_t va_amt_ceil(int32_t v) { return -va_amt_floor(-v); }
/* a turn of the AMT column from old to v: a value between two moves on from old to the next one its way (a slow
 * detent's 0.01: a tenth below 6, a whole above), a larger one stops at the last it passed (a quick detent's 6.35: 6,
 * the whole amounts' step of before) */
static int32_t va_amt_q(int32_t v, int32_t old)
{
    int32_t r = v = clamp(v, -6400, 6300);
    if (v > old && (r = va_amt_floor(v)) <= old)
        r = va_amt_ceil(old + 1);
    else if (v < old && (r = va_amt_ceil(v)) >= old)
        r = va_amt_floor(old - 1);
    return clamp(r, -6400, 6300);
}

/* LFO n's RATE with its SYNC on: the list (one item a detent: cr_pages.c kn_list; OPT snaps to the straight ones) */
static const param_desc_t VA_D_SRATE = {"RATE", F_ENUM, 0, VA_NSYNC - 1, 19, N_VA_SYNC, 0};

/* LFO SYNC: N_VA_SYNC's phase increments per control tick (CTL = 32 samples at 44.1 kHz). A division of n / d beats:
 * per BPM, Q8 (2^32 x 32 / (44100 x 60) / beats x 256 = 2^45 d / (2646000 n); the largest, 256T at 1/96 beat, 1.28e9;
 * the smallest, 8BARD at 48 beats, 277k: 1/277k at 1 BPM, exact to 4e-6); x BPM >> 8 in 64 bits (256T at 300 BPM:
 * 1.5e9, 480 Hz). Past 256T a free period of m ms (VA_MS: the increment itself, 2^32 x 32000 / (44100 m); 2 ms is
 * 1.56e9, 500 Hz: under the control rate's half, 689 Hz) */
#define VA_SQ8(n, d) (uint32_t)((35184372088832ull * (d) + 1323000ull * (n)) / (2646000ull * (n)))
#define VA_MS(m) (uint32_t)((137438953472000ull + 22050ull * (m)) / (44100ull * (m)))
#define VA_SB(b) VA_SQ8(6 * (b), 1), VA_SQ8(4 * (b), 1), VA_SQ8(8 * (b), 3)   /* b bars: dotted, straight, triplet */
#define VA_SN(n, d) VA_SQ8(3 * (n), 2 * (d)), VA_SQ8(n, d), VA_SQ8(2 * (n), 3 * (d))   /* a note of n / d beats */
static const uint32_t VA_SYNC_Q8[VA_NSYNC] = {VA_SB(8), VA_SB(4), VA_SB(3), VA_SB(2), VA_SB(1), VA_SN(2, 1),
    VA_SN(1, 1), VA_SN(1, 2), VA_SN(1, 4), VA_SN(1, 8), VA_SN(1, 16), VA_SN(1, 32), VA_SN(1, 64), VA_MS(4), VA_MS(3),
    VA_MS(2)};
#define VA_SYNC_MS0 39u          /* N_VA_SYNC's first free period */


#define VA_AMX(x, mf, k) ((((mf) >> (k)) & 1u) ? ((x) * 205) >> 11 : (x))
#endif
