/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* SKYLYNNE's space: a reverb, a tempo-synced ping-pong delay, a granular cloud, and the master.
 *
 * The voices send to the room (sk_voices: the reverb and the grain busses); the SPACE macro opens the room: more reverb
 * and a longer tail, more echo, more grains. The reverb is an eight-line feedback delay network (a Hadamard mix, a
 * damping filter in every line, two lines slowly modulated, two allpasses diffusing the input); the delay's time is a
 * number of sixteenths of the biome's tempo and it glides when the tempo moves; the grains are taken from the last
 * three quarters of a second of what the voices sent, at the pitches of the biome's layers (the octave, the fifth),
 * each with a Hann window and its own place in the stereo field. The master limits softly and carries the MASTER pot.
 *
 * Integer only; every buffer is int16 in the pool. Included by sk_core.c after sk_dsp.c. */

#define SK_RVN 8                         /* reverb lines */
static const uint16_t SK_RVL[SK_RVN] = {1049, 1223, 1399, 1621, 1823, 2063, 2273, 2551};
#define SK_RVMAX 2560
#define SK_AP1 142
#define SK_AP2 107
#define SK_DLN 16384                     /* the delay's line, samples (a power of two): 0.37 s */
#define SK_GRN 32768                     /* the grain buffer, samples: 0.74 s */
#define SK_NG 16                         /* grains at once */

static struct {
    int16_t line[SK_RVN][SK_RVMAX];
    int16_t ap1[SK_AP1], ap2[SK_AP2];
    int16_t dl[2][SK_DLN];
    int16_t gb[SK_GRN];
} sk_fxm SK_POOL;

typedef struct {
    uint32_t pos, step;                  /* the read place and its step, Q16 samples */
    uint32_t len, age;
    int32_t gl, gr;
    uint8_t on;
} sk_grain_t;

static struct {
    uint32_t rp[SK_RVN];                 /* the lines' write positions */
    int32_t lpz[SK_RVN];                 /* the damping filters */
    uint32_t ap1p, ap2p, mod;
    int32_t g[SK_RVN];                   /* the lines' gains */
    int32_t damp;                        /* the damping coefficient, Q15 */
    int32_t wet, in;                     /* the return and the send, Q15 */
    uint32_t dwp;                        /* the delay */
    int32_t dcur, dtgt;                  /* its length now and wanted, Q8 samples */
    int32_t dfb, dwet, dlz[2];
    uint32_t gwp, gnext, gseed;          /* the grains */
    int32_t gwet;
    sk_grain_t gr[SK_NG];
    int32_t mlow[2], mlow2[2], dc[2];    /* the master */
    uint32_t master;                     /* MASTER, Q12 (the pot) */
    uint8_t ready;
} sk_f;

/* the SPACE macro (and the biome) to the room's settings; each block */
static void sk_space(void)
{
    uint32_t s = sk_s.mac[3], i;
    int rt = (int)sk_s.b->rv + ((int)s - 64) / 6 + ((int)sk_s.fx[0] - 64) / 4;       /* ROOM: +-16 steps of the tail */
    int32_t g0;
    if (sk_xf) {                                                       /* a hand-over: the old room's tail is cut short, then eased back */
        uint32_t left = sk_xf > SK_XF_LEN / 2u ? SK_XF_LEN / 2u : sk_xf;   /* full for the first 3 s, then fading over the next 3 */
        rt -= (int)(16u * left / (SK_XF_LEN / 2u));
    }
    rt = rt < 0 ? 0 : rt > 31 ? 31 : rt;
    g0 = SK_RVG[rt];
    for (i = 0; i < SK_RVN; i++)                                       /* each line's share of the time */
        sk_f.g[i] = 32767 - ((32767 - g0) * (int32_t)SK_RVL[i]) / 2400;
    sk_f.damp = 26000 - (int32_t)sk_s.mac[2] * 120;                    /* COLOUR also opens the room's top */
    sk_f.wet = 2600 + (int32_t)s * 120;                                /* 0.08 .. 0.5 */
    sk_f.in = 7000;
    sk_f.dfb = (9000 + (int32_t)s * 100) * (64 + (int32_t)sk_s.fx[1]) / 128;   /* ECHO: the feedback 0.27 .. 0.6 */
    if (sk_f.dfb > 24000)
        sk_f.dfb = 24000;
    sk_f.dwet = (int32_t)s * 85 * (int32_t)sk_s.fx[1] >> 6;            /* 0 .. 0.33, x0 .. x2 */
    /* CLOUD: off in the middle (the default); turned up, a faint cloud of the biome's own grains blooms in the room */
    sk_f.gwet = sk_s.fx[2] > 64u ? (int32_t)s * 150 * (((int32_t)sk_s.fx[2] - 64) * 2) >> 6 : 0;
    if (sk_gm.on) {                                                    /* REC: the grains are the instrument */
        int32_t w = 13000 + (int32_t)sk_s.grn[2] * 100;
        if (sk_f.gwet < w)
            sk_f.gwet = w;
    }
    {
        uint32_t n = sk_s.b->dl ? sk_s.b->dl : 1u, t = sk_c.steplen * n;
        while (t >= SK_DLN - 64u)
            t >>= 1;
        sk_f.dtgt = (int32_t)(t << 8);
    }
}

static inline int32_t sk_q15(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b + 16384) >> 15); }

static void sk_reverb(const int32_t *in, int32_t *ol, int32_t *or_, uint32_t n)
{
    uint32_t i, k;
    for (i = 0; i < n; i++) {
        int32_t x = sk_q15(in[i], sk_f.in), t, d[SK_RVN], y;
        int32_t lfo = SK_SIN[(sk_f.mod >> 10) & 1023u];
        sk_f.mod += 11u;                                               /* ~0.4 Hz */
        /* two allpasses spread the input */
        t = sk_fxm.ap1[sk_f.ap1p];
        y = x - ((t * 19660) >> 15);
        sk_fxm.ap1[sk_f.ap1p] = (int16_t)sk_clampi(y, -32767, 32767);
        x = t + ((y * 19660) >> 15);
        if (++sk_f.ap1p >= SK_AP1)
            sk_f.ap1p = 0;
        t = sk_fxm.ap2[sk_f.ap2p];
        y = x - ((t * 19660) >> 15);
        sk_fxm.ap2[sk_f.ap2p] = (int16_t)sk_clampi(y, -32767, 32767);
        x = t + ((y * 19660) >> 15);
        if (++sk_f.ap2p >= SK_AP2)
            sk_f.ap2p = 0;
        for (k = 0; k < SK_RVN; k++) {
            uint32_t rd = sk_f.rp[k] + 1u;                              /* the oldest sample: a line's length ago */
            int32_t v;
            if (k == 1u || k == 4u) {                                   /* these two read at a slowly moving place */
                int32_t m = (lfo * 14) >> 10;                           /* +-14 sixteenth samples... */
                uint32_t a = sk_f.rp[k] + 1u + (uint32_t)((m >> 4) + 16), fr = (uint32_t)m & 15u;
                int32_t s0 = sk_fxm.line[k][a % SK_RVL[k]], s1 = sk_fxm.line[k][(a + 1u) % SK_RVL[k]];
                v = s0 + (((s1 - s0) * (int32_t)fr) >> 4);
            } else {
                v = sk_fxm.line[k][rd % SK_RVL[k]];
            }
            sk_f.lpz[k] += sk_q15(v - sk_f.lpz[k], 32767 - sk_f.damp);  /* the damping */
            d[k] = sk_q15(sk_f.lpz[k], sk_f.g[k]);
        }
        /* a Hadamard mix of the eight */
        {
            int32_t a0 = d[0] + d[1], a1 = d[0] - d[1], a2 = d[2] + d[3], a3 = d[2] - d[3];
            int32_t a4 = d[4] + d[5], a5 = d[4] - d[5], a6 = d[6] + d[7], a7 = d[6] - d[7];
            int32_t b0 = a0 + a2, b1 = a1 + a3, b2 = a0 - a2, b3 = a1 - a3;
            int32_t b4 = a4 + a6, b5 = a5 + a7, b6 = a4 - a6, b7 = a5 - a7;
            d[0] = b0 + b4;
            d[1] = b1 + b5;
            d[2] = b2 + b6;
            d[3] = b3 + b7;
            d[4] = b0 - b4;
            d[5] = b1 - b5;
            d[6] = b2 - b6;
            d[7] = b3 - b7;
        }
        {
            int32_t outl = 0, outr = 0;
            for (k = 0; k < SK_RVN; k++) {
                int32_t w = ((d[k] * 11585) >> 15) + ((k & 1u) ? x : -x) / 2;     /* 1 / sqrt(8), plus the input */
                uint32_t wp = sk_f.rp[k];
                sk_fxm.line[k][wp] = (int16_t)sk_clampi(w, -32767, 32767);
                if (++wp >= SK_RVL[k])
                    wp = 0;
                sk_f.rp[k] = wp;
                if (k & 1u)
                    outr += (k & 2u) ? -sk_fxm.line[k][wp] : sk_fxm.line[k][wp];
                else
                    outl += (k & 2u) ? -sk_fxm.line[k][wp] : sk_fxm.line[k][wp];
            }
            ol[i] += sk_q15(outl, sk_f.wet);
            or_[i] += sk_q15(outr, sk_f.wet);
        }
    }
}

/* the delay: the busses' mono in, ping-pong out */
static void sk_delay(const int32_t *in, int32_t *ol, int32_t *or_, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        uint32_t d, ip, fr;
        int32_t a, b, la, lb;
        sk_f.dcur += (sk_f.dtgt - sk_f.dcur) >> 13;                    /* glides to a new time like a tape */
        d = (uint32_t)sk_f.dcur;
        ip = sk_f.dwp - (d >> 8);
        fr = d & 255u;
        a = sk_fxm.dl[0][(ip - 1u) & (SK_DLN - 1u)] * (int32_t)(256u - fr) + sk_fxm.dl[0][(ip - 2u) & (SK_DLN - 1u)] * (int32_t)fr;
        b = sk_fxm.dl[1][(ip - 1u) & (SK_DLN - 1u)] * (int32_t)(256u - fr) + sk_fxm.dl[1][(ip - 2u) & (SK_DLN - 1u)] * (int32_t)fr;
        a >>= 8;
        b >>= 8;
        sk_f.dlz[0] += sk_q15(a - sk_f.dlz[0], 14000);                 /* each echo a little darker */
        sk_f.dlz[1] += sk_q15(b - sk_f.dlz[1], 14000);
        la = in[i] + sk_q15(sk_f.dlz[1], sk_f.dfb);
        lb = sk_q15(sk_f.dlz[0], sk_f.dfb);
        sk_fxm.dl[0][sk_f.dwp & (SK_DLN - 1u)] = (int16_t)sk_clampi(la, -32767, 32767);
        sk_fxm.dl[1][sk_f.dwp & (SK_DLN - 1u)] = (int16_t)sk_clampi(lb, -32767, 32767);
        sk_f.dwp++;
        ol[i] += sk_q15(a, sk_f.dwet);
        or_[i] += sk_q15(b, sk_f.dwet);
    }
}

/* the grains */
static uint32_t sk_grand(void)
{
    uint32_t x = sk_f.gseed ? sk_f.gseed : 0xA5A5F00Du;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    sk_f.gseed = x;
    return x;
}

/* the pitch ratios for -12 .. +12 semitones, Q15 (32768 is unison) */
static const uint16_t SK_SEMI[25] = {16384, 17358, 18390, 19484, 20642, 21870, 23170, 24548, 26008, 27554, 29193, 30928, 32768,
                                     34716, 36781, 38968, 41285, 43740, 46341, 49097, 52016, 55109, 58386, 61858, 65535};

static void sk_grain_spawn(void)
{
    uint32_t i, r = sk_grand(), live = 0;
    uint32_t size = (uint32_t)sk_clampi((int)sk_s.b->gr + ((int)sk_s.fx[2] - 64), 0, 127), lay = sk_s.b->gl;
    for (i = 0; i < SK_NG; i++)
        live += sk_f.gr[i].on;
    if (sk_gm.on)
        if (live >= 3u + (uint32_t)sk_s.grn[2] / 10u)                  /* DENSITY also sets how many sound at once (3 .. 15) */
            return;
    if (!sk_gm.on && live >= 10u)
        return;
    for (i = 0; i < SK_NG; i++) {
        sk_grain_t *g = &sk_f.gr[i];
        uint32_t pit, len, back, pan, minb;
        if (g->on)
            continue;
        pit = 32768u;
        if (sk_gm.on) {
            int st = ((int)sk_s.grn[3] - 64) * 12 / 63;                 /* PITCH: -12 .. +12, a dead zone around the middle */
            st = st < -12 ? -12 : st > 12 ? 12 : st;
            pit = SK_SEMI[st + 12];
            uint32_t hib;
            len = 700u + (uint32_t)sk_s.grn[1] * 100u + ((r >> 8) & 511u);        /* SIZE: 16 ms .. 290 ms */
            /* the read must not run into the write head (an octave up gains on it by a grain's length) or off the old end
             * (an octave down falls behind it): POSITION spans what is left. Frozen, nothing is written, so only the
             * forward reach counts. */
            minb = 512u + (pit > 32768u ? (len * (pit - 32768u) >> 15) : 0u);
            hib = SK_GRN - 2048u - (pit < 32768u ? (len * (32768u - pit) >> 15) : 0u);
            if (sk_gm.frz) {
                minb = 512u + (len * pit >> 15);
                hib = SK_GRN - 16u;
            }
            if (hib < minb + 64u)
                hib = minb + 64u;
            back = minb + ((uint32_t)sk_s.grn[0] * (hib - minb) >> 7) + ((r >> 12) & 1023u);   /* POSITION */
            if (back > hib)
                back = hib;
        } else {
            if ((r & 7u) < 3u) {                                        /* the biome's layers now and then */
                uint32_t pick = (r >> 4) & 3u;
                if (pick == 0u && (lay & 1u))
                    pit = 65536u;
                else if (pick == 1u && (lay & 2u))
                    pit = 49152u;
                else if (pick == 2u && (lay & 4u))
                    pit = 16384u;
            }
            len = 2200u + size * 90u + ((r >> 8) & 1023u);              /* 50 .. 300 ms */
            back = len * 2u + 2000u + ((r >> 12) & 4095u);              /* behind the write head, clear of it */
        }
        if (back >= SK_GRN - 4u)
            back = SK_GRN - 4u;
        g->pos = (sk_f.gwp - back) << 16;
        g->step = pit << 1;                                             /* Q16 samples a sample */
        g->len = len;
        g->age = 0;
        pan = ((r >> 22) & 127u) * 2u;
        g->gr = SK_SIN[pan];
        g->gl = SK_SIN[256u - pan];
        g->on = 1;
        return;
    }
}

static void sk_grains(const int32_t *in, int32_t *ol, int32_t *or_, uint32_t n)
{
    uint32_t i, k, per = 4200u - (uint32_t)sk_s.mac[3] * 20u;          /* a new grain every ~60..95 ms */
    if (sk_gm.on)
        per = 3400u - (uint32_t)sk_s.grn[2] * 26u;                     /* DENSITY: a grain every 80 ms .. 3 ms */
    for (i = 0; i < n; i++) {
        int32_t sl = 0, sr = 0;
        if (!(sk_gm.on && sk_gm.frz)) {                                 /* frozen: the buffer holds what it has */
            sk_fxm.gb[sk_f.gwp & (SK_GRN - 1u)] = (int16_t)sk_clampi(in[i], -32767, 32767);
            sk_f.gwp++;
        }
        if (sk_f.gnext) {
            sk_f.gnext--;
        } else {
            sk_f.gnext = per / 2u + (sk_grand() % (per ? per : 1u));
            if ((sk_s.mac[3] > 8u && sk_s.fx[2] > 64u) || sk_gm.on)
                sk_grain_spawn();
        }
        for (k = 0; k < SK_NG; k++) {
            sk_grain_t *g = &sk_f.gr[k];
            uint32_t ip;
            int32_t s0, s1, w, y;
            if (!g->on)
                continue;
            ip = g->pos >> 16;
            s0 = sk_fxm.gb[ip & (SK_GRN - 1u)];
            s1 = sk_fxm.gb[(ip + 1u) & (SK_GRN - 1u)];
            y = s0 + (int32_t)(((int64_t)(s1 - s0) * (int32_t)(g->pos & 0xFFFFu)) >> 16);
            w = SK_HANN[(g->age * 256u) / g->len];
            y = (y * w) >> 15;
            sl += (y * g->gl) >> 15;
            sr += (y * g->gr) >> 15;
            g->pos += g->step;
            if (++g->age >= g->len)
                g->on = 0;
        }
        ol[i] += sk_q15(sl, sk_f.gwet);
        or_[i] += sk_q15(sr, sk_f.gwet);
    }
}

/* the master: a low cut (the room's rumble), a soft limiter, and the pot. out: interleaved L R, Q15 */
static inline int32_t sk_soft(int32_t x)
{
    int32_t t, x3;
    x = sk_clampi(x, -49152, 49152);
    t = (x * x) >> 15;
    x3 = (t * x) >> 15;
    return x - ((x3 * 4855) >> 15);
}

/* the fade of the whole output (the intro brings the sound in): the level now and where it is going, Q12 */
static uint32_t sk_fade = 4096, sk_fade_to = 4096;

/* one block: the conductor, the voices, the room, the master */
static void sk_render(int32_t *out, uint32_t n)
{
    int32_t l[SK_CTL], r[SK_CTL], rv[SK_CTL], gb[SK_CTL], dl[SK_CTL], wl[SK_CTL], wr[SK_CTL];
    uint32_t i;
    int32_t hc;
    {                                                                   /* HIGH CUT: 16 kHz down to 600 Hz, exponentially; two poles */
        static const uint16_t HC[17] = {29415, 27635, 25510, 23149, 20673, 18198, 15816, 13594, 11575, 9777, 8203, 6845, 5685, 4703, 3879, 3191, 2685};
        uint32_t f = sk_s.fx[3], k = f >> 3, fr = f & 7u;
        hc = (int32_t)HC[k] + (((int32_t)HC[k + 1u] - (int32_t)HC[k]) * (int32_t)fr >> 3);
    }
    if (!sk_f.ready) {
        sk_f.ready = 1;
        sk_f.dcur = sk_f.dtgt = (int32_t)(sk_c.steplen << 8);
        sk_f.master = 2800;
    }
    sk_tick(n);
    sk_space();
    if (sk_fade != sk_fade_to)                                          /* about 3 s from nothing to full */
        sk_fade = sk_fade < sk_fade_to ? (sk_fade + 3u > sk_fade_to ? sk_fade_to : sk_fade + 3u) : (sk_fade < sk_fade_to + 3u ? sk_fade_to : sk_fade - 3u);
    for (i = 0; i < n; i++)
        l[i] = r[i] = rv[i] = gb[i] = wl[i] = wr[i] = 0;
    sk_voices(l, r, rv, gb, n);
    for (i = 0; i < n; i++)
        dl[i] = (l[i] + r[i]) >> 2;
    sk_grains(gb, wl, wr, n);
    for (i = 0; i < n; i++)
        rv[i] += (wl[i] + wr[i]) >> 2;                                  /* the grains bloom in the room too */
    sk_reverb(rv, wl, wr, n);
    sk_delay(dl, wl, wr, n);
    for (i = 0; i < n; i++) {
        int32_t a = l[i] + wl[i], b = r[i] + wr[i];
        sk_f.dc[0] += (a * 256 - sk_f.dc[0]) >> 10;                     /* a DC and rumble cut (kept in Q8 so that it settles to nothing) */
        sk_f.dc[1] += (b * 256 - sk_f.dc[1]) >> 10;
        a -= sk_f.dc[0] >> 8;
        b -= sk_f.dc[1] >> 8;
        sk_f.mlow[0] += sk_q15(a - sk_f.mlow[0], hc);                  /* HIGH CUT: a one-pole low-pass over the whole mix */
        sk_f.mlow[1] += sk_q15(b - sk_f.mlow[1], hc);
        sk_f.mlow2[0] += sk_q15(sk_f.mlow[0] - sk_f.mlow2[0], hc);
        sk_f.mlow2[1] += sk_q15(sk_f.mlow[1] - sk_f.mlow2[1], hc);
        a = sk_f.mlow2[0];
        b = sk_f.mlow2[1];
        a = (a * (int32_t)sk_f.master) >> 12;
        b = (b * (int32_t)sk_f.master) >> 12;
        a = (a * (int32_t)sk_fade) >> 12;
        b = (b * (int32_t)sk_fade) >> 12;
        out[2u * i] = sk_soft(a);
        out[2u * i + 1u] = sk_soft(b);
    }
}
