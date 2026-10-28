/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* SKYLYNNE's own sound: six resonator voices that play themselves.
 *
 * A voice is four parallel two-pole resonators (modes) at the frequency ratios of a material (bell, glass, bar,
 * string, plate, wood), struck by a short burst of filtered noise. A conductor runs on one tempo and one scale: every
 * sixteenth a voice may strike, by its own Euclidean pattern (hits k in n steps, rotated), a chance, and a walk
 * through the scale. The six patterns have different lengths, so together they drift against each other.
 *
 * The audio side: sky_block(n) at the top of every block (cf_out.c cf_audio_block) steps the conductor; sky_run(l, r,
 * n) adds the voices to the master bus (cf_fx.c cf_fx_chain, before the granular layer and the effects, which then
 * act on it). The UI reads sky_ui (which voice just struck, how bright it is still) to light the skyline.
 *
 * Integer only. A mode: y = (b x + a1 y1 + a2 y2) >> 29, the coefficients in Q29 (a1 = 2 r cos w, a2 = -r^2,
 * b = sin w x mode level, so a struck mode peaks at its level whatever its pitch). The device has no 64-bit divide:
 * the coefficients are worked out with multiplies and shifts (a Taylor series at a strike, not per sample). */

#include "sky_tables.h"

#define SKY_NV 6                         /* voices */
#define SKY_NM 4                         /* modes a voice */
#define SKY_PI_Q29 1686629713            /* pi in Q29 */
#define SKY_HALFPI_Q29 843314857
#define SKY_WMAX 1395864371u             /* 2.6 rad: modes above sit too near the Nyquist: left out */

/* the materials: mode frequency ratios (Q10) and mode levels (Q15) */
static const uint16_t SKY_RATIO[6][SKY_NM] = {
    {1024, 2826, 5530, 9144},            /* 0 bell */
    {1024, 2376, 4352, 6789},            /* 1 glass */
    {1024, 4024, 9605, 15360},           /* 2 bar (marimba) */
    {1024, 2051, 3082, 4116},            /* 3 string (slightly stretched) */
    {1024, 1632, 2187, 2717},            /* 4 plate / membrane */
    {1024, 2458, 4198, 6451},            /* 5 wood */
};
static const uint16_t SKY_LEVEL[6][SKY_NM] = {
    {32767, 22000, 15000, 9000},
    {32767, 20000, 12000, 7000},
    {32767, 14000, 6000, 3000},
    {32767, 24000, 17000, 12000},
    {32767, 26000, 20000, 15000},
    {32767, 18000, 9000, 4000},
};

/* one voice's settings and the biome's: what the conductor plays */
typedef struct {
    uint8_t base;                        /* the middle note of its register (MIDI) */
    uint8_t mat;                         /* material 0..5 */
    uint8_t decay;                       /* ring-out 0..23 (SKY_R) */
    uint8_t n, k, rot;                   /* Euclid: k hits in n steps, rotated */
    uint8_t prob;                        /* chance a hit sounds, 0..255 */
    uint8_t vel;                         /* 0..127 */
    int8_t pan;                          /* -64..63 */
    uint8_t hard;                        /* how hard it is struck: brightness 0..127 */
} sky_cfg_t;

typedef struct {
    uint8_t root;                        /* semitones the scale sits above the voices' bases (which are D) */
    uint8_t len;                         /* notes of the scale */
    uint8_t iv[8];                       /* its intervals */
    uint16_t bpm;
    sky_cfg_t v[SKY_NV];
} sky_biome_t;

/* NEON RAIN: D, a minor pentatonic with the sixth, 84 beats a minute */
static const sky_biome_t SKY_BIOME0 = {
    0, 6, {0, 3, 5, 7, 9, 10, 0, 0}, 84,
    {
        {38, 2, 14, 16, 3, 0, 255, 100, -20, 70},
        {50, 3, 12, 16, 5, 2, 200, 84, 22, 60},
        {74, 1, 15, 12, 5, 1, 150, 70, -42, 90},
        {62, 4, 13, 16, 7, 3, 180, 76, 40, 50},
        {62, 5, 5, 16, 9, 5, 130, 60, 0, 100},
        {86, 0, 17, 16, 2, 7, 140, 64, 56, 110},
    },
};

typedef struct {
    int32_t a1[SKY_NM], a2[SKY_NM], b[SKY_NM];   /* Q29 */
    int32_t y1[SKY_NM], y2[SKY_NM];
    int32_t env, envk;                   /* the exciter's envelope (Q15) and its per-sample decay (Q15) */
    int32_t lp, lpk;                     /* its low-pass: the brightness (Q15 coefficient) */
    int32_t gl, gr;                      /* output gain left / right (Q15) */
    int32_t nz;                          /* noise state */
    uint32_t act;                        /* samples the voice still rings (0: idle, no work) */
    uint8_t nm;                          /* modes in use */
    int8_t deg;                          /* the walk's place in the scale */
} sky_v_t;

static sky_v_t sky_v[SKY_NV] __attribute__((section(".pool")));
static struct {
    uint32_t step, clk, rnd;
    uint32_t steplen;                    /* samples a sixteenth */
    uint16_t bpm;
} sky_c;
static const sky_biome_t *sky_b = &SKY_BIOME0;
static int32_t sky_gain = 17000;         /* the voices' master (Q15) */

/* what the screen reads (written by the audio side; single bytes, read-only for the UI) */
static volatile struct {
    uint8_t lit[SKY_NV];                 /* 255 at a strike, falling */
    uint8_t note[SKY_NV];                /* the last note struck */
    uint8_t cnt[SKY_NV];                 /* strikes (wraps) */
    uint8_t step;                        /* the sixteenth now (0..15) */
} sky_ui;

static uint32_t sky_rand(void)
{
    uint32_t x = sky_c.rnd ? sky_c.rnd : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    sky_c.rnd = x;
    return x;
}

/* sin and cos of w (Q29 radians, 0 .. pi) in Q29, by Taylor series after folding into 0 .. pi/2: the series' terms
 * come from one Horner chain in x^2 (constants are 2^29 / n!) */
static void sky_sincos(int32_t w, int32_t *s, int32_t *c)
{
    int64_t x, x2, a;
    int neg = 0;
    if (w > SKY_HALFPI_Q29) {
        w = SKY_PI_Q29 - w;
        neg = 1;
    }
    if (w < 0)
        w = 0;
    x = w;
    x2 = (x * x) >> 29;
    /* cos = 1 - e (1/2 - e (1/24 - e (1/720 - e (1/40320 - e / 3628800)))), e = x^2; the constants are 2^29 / n! */
    a = 148;
    a = 13316 - ((x2 * a) >> 29);
    a = 745654 - ((x2 * a) >> 29);
    a = 22369621 - ((x2 * a) >> 29);
    a = 268435456 - ((x2 * a) >> 29);
    a = 536870912 - ((x2 * a) >> 29);
    *c = (int32_t)(neg ? -a : a);
    /* sin = x (1 - e (1/6 - e (1/120 - e (1/5040 - e / 362880)))) */
    a = 1479;
    a = 106522 - ((x2 * a) >> 29);
    a = 4473924 - ((x2 * a) >> 29);
    a = 89478485 - ((x2 * a) >> 29);
    a = 536870912 - ((x2 * a) >> 29);
    *s = (int32_t)((x * a) >> 29);
}

/* a voice strikes: note, its material, ring-out, velocity and hardness */
static void sky_strike(uint32_t vi, uint32_t note, const sky_cfg_t *c)
{
    sky_v_t *v = &sky_v[vi];
    uint32_t k, nm = 0, tau;
    uint32_t mat = c->mat < 6u ? c->mat : 0u;
    if (note > 127u)
        note = 127u;
    for (k = 0; k < SKY_NM; k++) {
        uint64_t w64 = ((uint64_t)SKY_W[note] * SKY_RATIO[mat][k]) >> 10;
        int32_t s, co, r;
        int idx = (int)c->decay - 2 * (int)k;
        if (w64 >= SKY_WMAX)
            break;
        idx = idx < 0 ? 0 : idx;
        sky_sincos((int32_t)w64, &s, &co);
        r = SKY_R[idx];
        v->a1[k] = (int32_t)(((int64_t)co * 2 * r) >> 29);
        v->a2[k] = -(int32_t)(((int64_t)r * r) >> 29);
        v->b[k] = (int32_t)(((int64_t)s * SKY_LEVEL[mat][k]) >> 15);
        nm++;
    }
    for (k = nm; k < SKY_NM; k++)
        v->b[k] = v->a1[k] = v->a2[k] = 0;
    v->nm = (uint8_t)nm;
    /* the exciter: a burst of noise, harder = shorter and brighter */
    v->env = 12000 + (int32_t)c->vel * 160;
    v->envk = 32768 - 60 - (int32_t)(127u - c->hard) * 5;            /* a burst of ~60 .. 500 samples */
    v->lpk = 3000 + (int32_t)c->hard * 200;                          /* brightness */
    {
        int32_t pan = c->pan + 64;                                   /* 0 .. 127 */
        v->gl = (int32_t)(127 - pan) * 258 / 1;
        v->gr = (int32_t)pan * 258 / 1;
        v->gl = v->gl > 32767 ? 32767 : v->gl;
        v->gr = v->gr > 32767 ? 32767 : v->gr;
    }
    tau = SKY_TAU[c->decay < 24u ? c->decay : 23u];
    v->act = tau * 6u + 2000u;
}

/* the conductor: one sixteenth */
static void sky_step(void)
{
    uint32_t i, st = sky_c.step++;
    const sky_biome_t *b = sky_b;
    sky_ui.step = (uint8_t)(st & 15u);
    for (i = 0; i < SKY_NV; i++) {
        const sky_cfg_t *c = &b->v[i];
        uint32_t n = c->n ? c->n : 16u, hit, r;
        hit = (((st + c->rot) % n) * c->k) % n < c->k;
        if (!hit)
            continue;
        r = sky_rand();
        if ((r & 255u) >= c->prob)
            continue;
        {
            sky_v_t *v = &sky_v[i];
            int deg = v->deg, span = (int)b->len * 2;
            uint32_t note;
            r >>= 8;
            if ((r & 7u) == 0u)                             /* now and then a jump */
                deg = (int)((r >> 4) % (uint32_t)span);
            else
                deg += (int)((r >> 4) % 5u) - 2;
            deg = deg < 0 ? 0 : deg >= span ? span - 1 : deg;
            v->deg = (int8_t)deg;
            note = c->base + 12u * (uint32_t)(deg / b->len) + b->iv[deg % b->len] + b->root;
            sky_strike(i, note, c);
            sky_ui.note[i] = (uint8_t)note;
            sky_ui.lit[i] = 255;
            sky_ui.cnt[i]++;
        }
    }
}

/* the top of every block: the conductor's clock */
static void sky_block(uint32_t n)
{
    uint32_t i;
    if (!sky_c.steplen) {
        sky_c.bpm = sky_b->bpm;
        sky_c.steplen = (2646000u / sky_c.bpm) / 4u;       /* a quarter is 2646000 / bpm samples */
        for (i = 0; i < SKY_NV; i++)
            sky_v[i].deg = (int8_t)sky_b->len;
    }
    sky_c.clk += n;
    while (sky_c.clk >= sky_c.steplen) {
        sky_c.clk -= sky_c.steplen;
        sky_step();
    }
    for (i = 0; i < SKY_NV; i++)                          /* the light falls: ~0.4 s */
        if (sky_ui.lit[i])
            sky_ui.lit[i] = (uint8_t)(sky_ui.lit[i] > 2u ? sky_ui.lit[i] - 2u : 0u);
}

/* add the voices to the master bus */
static void sky_run(int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t vi, i, k;
    for (vi = 0; vi < SKY_NV; vi++) {
        sky_v_t *v = &sky_v[vi];
        int32_t y1[SKY_NM], y2[SKY_NM], a1[SKY_NM], a2[SKY_NM], b[SKY_NM];
        int32_t env, lp, nz, gl, gr, g = sky_gain;
        uint32_t nm = v->nm;
        if (!v->act)
            continue;
        for (k = 0; k < SKY_NM; k++) {
            y1[k] = v->y1[k];
            y2[k] = v->y2[k];
            a1[k] = v->a1[k];
            a2[k] = v->a2[k];
            b[k] = v->b[k];
        }
        env = v->env;
        lp = v->lp;
        nz = v->nz;
        gl = (v->gl * g) >> 15;
        gr = (v->gr * g) >> 15;
        for (i = 0; i < n; i++) {
            int32_t x, o;
            int64_t acc = 0;
            x = (int32_t)(noise32(&nz) >> 16) - 32768;
            x = (x * (env >> 3)) >> 12;                    /* x * env (Q15) with headroom */
            env = (int32_t)(((int64_t)env * v->envk) >> 15);
            lp += (int32_t)(((int64_t)(x - lp) * v->lpk) >> 15);
            for (k = 0; k < nm; k++) {
                int64_t yn = ((int64_t)b[k] * lp + (int64_t)a1[k] * y1[k] + (int64_t)a2[k] * y2[k]) >> 29;
                if (yn > 8388607)
                    yn = 8388607;
                else if (yn < -8388607)
                    yn = -8388607;
                y2[k] = y1[k];
                y1[k] = (int32_t)yn;
                acc += yn;
            }
            o = (int32_t)(acc >> 6);                       /* (the modes' sum, scaled into the bus) */
            l[i] += (o * gl) >> 15;
            r[i] += (o * gr) >> 15;
        }
        for (k = 0; k < SKY_NM; k++) {
            v->y1[k] = y1[k];
            v->y2[k] = y2[k];
        }
        v->env = env;
        v->lp = lp;
        v->nz = nz;
        v->act = v->act > n ? v->act - n : 0u;
        if (!v->act) {
            for (k = 0; k < SKY_NM; k++)
                v->y1[k] = v->y2[k] = 0;
            v->env = v->lp = 0;
        }
    }
}
