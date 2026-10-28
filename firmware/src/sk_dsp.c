/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* SKYLYNNE's voices and conductor: six voices that play themselves.
 *
 * A biome (sk_biomes.c) gives each of the six voices a type, a register, a rhythm and a character, and the whole a
 * tempo and a scale. The conductor runs one clock: every sixteenth, each voice may strike by its own Euclidean
 * pattern (k hits in n steps, rotated), a chance, and a walk through the scale. The patterns have different lengths,
 * so together they drift against each other and the music never repeats.
 *
 * Five kinds of voice, all integer, all cheap while idle:
 *   RESONATOR  four two-pole resonators at the mode ratios of a material (bell, glass, bar, string, plate, wood),
 *              struck by a burst of noise;
 *   PLUCK      a string (Karplus-Strong: a delay line with a loop filter, fractional length, tuned);
 *   FM BELL    two operators, the modulation index dying faster than the note;
 *   PAD        three detuned feedback oscillators through a slow envelope, the pitch gliding between notes;
 *   AIR        noise through a pitched resonant band-pass: breath, wind, whistle.
 * The four macros the player turns (DENSITY, MOTION, COLOUR, SPACE) are read here and in sk_fx.c: how often and how
 * wildly the voices play, how bright they sound, and how much of them goes to the room.
 *
 * Integer only (no 64-bit divide). Included by sk_core.c; the platform (sk_hw.c or the host harness) calls
 * sk_render(). */

#include "sky_tables.h"

#define SK_NV 6                          /* voices */
#define SK_NM 4                          /* modes of a resonator voice */
#define SK_PLN 2048                      /* a string's delay line (a power of two), samples */
#define SK_CTL 32                        /* samples a block */
#define SK_FS 44100

enum { VT_RES, VT_PLUCK, VT_BELL, VT_PAD, VT_AIR, VT_N };
static const char *const SK_TYPE_NAME[VT_N] = {"Resonator", "Pluck", "FM Bell", "Pad", "Air"};
static const char *const SK_MAT_NAME[6] = {"Bell", "Glass", "Bar", "String", "Plate", "Wood"};
static const char *const SK_BELL_NAME[4] = {"Soft", "Glass", "Hollow", "Metal"};

/* the resonator's materials: mode frequency ratios (Q10) and mode levels (Q15) */
static const uint16_t SK_RATIO[6][SK_NM] = {
    {1024, 2826, 5530, 9144},            /* 0 bell */
    {1024, 2376, 4352, 6789},            /* 1 glass */
    {1024, 4024, 9605, 15360},           /* 2 bar (marimba) */
    {1024, 2051, 3082, 4116},            /* 3 string (slightly stretched) */
    {1024, 1632, 2187, 2717},            /* 4 plate / membrane */
    {1024, 2458, 4198, 6451},            /* 5 wood */
};
static const uint16_t SK_LEVEL[6][SK_NM] = {
    {32767, 22000, 15000, 9000},
    {32767, 20000, 12000, 7000},
    {32767, 14000, 6000, 3000},
    {32767, 24000, 17000, 12000},
    {32767, 26000, 20000, 15000},
    {32767, 18000, 9000, 4000},
};
static const uint16_t SK_FMR[4] = {896, 362, 512, 1330};      /* the bell's modulator ratios (Q8): 3.5, 1.41, 2, 5.2 */
#define SK_WMAX 1395864371u              /* 2.6 rad: modes above sit too near the Nyquist: left out */
#define SK_PI_Q29 1686629713
#define SK_HALFPI_Q29 843314857

/* one voice of a biome: what it is and how it plays */
typedef struct {
    uint8_t type, mat;                   /* the kind of voice; its material or variant (0..5 / 0..3) */
    uint8_t base;                        /* the middle note of its register (MIDI) */
    uint8_t n, k, rot;                   /* Euclid: k hits in n steps, rotated */
    uint8_t prob;                        /* the chance a hit sounds, 0..255 */
    uint8_t vel;                         /* 0..127 */
    int8_t pan;                          /* -64..63 */
    uint8_t decay;                       /* ring-out 0..23 (SK_R); for a pad or air: the note's length in sixteenths */
    uint8_t hard;                        /* brightness 0..127 */
    uint8_t rsend, gsend;                /* how much goes to the reverb and the grains, 0..127 */
    uint8_t span;                        /* the walk's reach in scale notes (each side of the base) */
} sk_vcfg_t;

typedef struct {
    const char *name;
    uint8_t root;                        /* the key: semitones above C */
    uint8_t len;                         /* notes of the scale */
    uint8_t iv[8];                       /* its intervals */
    uint8_t bpm;
    uint8_t rv, dl, gr, gl;              /* the room: reverb time (0..31), the delay's sixteenths (0..3), the grains'
                                          * size (0..127) and layers (bit 0 octave up, 1 fifth, 2 octave down) */
    uint8_t mac[4];                      /* the macros it starts with: density, motion, colour, space */
    sk_vcfg_t v[SK_NV];
} sk_biome_t;

typedef struct {
    int32_t a1[SK_NM], a2[SK_NM], b[SK_NM], y1[SK_NM], y2[SK_NM];     /* resonator */
    int32_t env, envk, lp, lpk, nz;
    uint32_t wp, per, exc;                                             /* pluck */
    int32_t pg, plp, plpk, pst;
    uint32_t ph[3], inc[3], tinc[3], phm, incm;                        /* fm: bell and pad */
    int32_t idx, idxk, fb[3];
    int32_t amp, ampk, tgt, sh;                                        /* the amplitude: bell (ampk), pad and air */
    int32_t f1, q, bp[2], lo[2], pk;                                   /* air: the band-pass */
    uint32_t gate;                                                     /* samples to the note's end (pad, air) */
    int32_t gl, gr;                                                    /* pan, Q15 */
    uint32_t act;                                                      /* samples left (0: idle, no work) */
    uint8_t nm, type;
    int8_t deg;
    int8_t panw;                                                       /* the pan's wander */
} sk_voice_t;

#ifndef SK_POOL
#define SK_POOL
#endif
static sk_voice_t sk_v[SK_NV] SK_POOL;
static int16_t sk_pl[SK_NV][SK_PLN] SK_POOL;

/* the live state: what the player has changed */
static struct {
    const sk_biome_t *b;
    uint8_t mac[4];                      /* DENSITY, MOTION, COLOUR, SPACE, 0..127 */
    uint8_t level[SK_NV];                /* 0..100 */
    uint8_t type[SK_NV];                 /* a voice's kind (the biome's until changed) */
    uint8_t mat[SK_NV];
    int8_t pan[SK_NV];
    int8_t decay[SK_NV];                 /* -6 .. +6 steps of ring-out */
    uint8_t mute;                        /* bit v: voice v is silent */
    uint8_t hold;                        /* the conductor waits (the sound rings on) */
    uint8_t bpm;
    uint8_t sel;                         /* the voice the player is editing */
    uint8_t rot[SK_NV];                  /* the patterns' rotation (drifts with MOTION) */
} sk_s;

static struct {
    uint32_t step, clk, rnd, steplen, fall, bar;
} sk_c;

/* what the screen reads (written by the audio side; single bytes) */
static volatile struct {
    uint8_t lit[SK_NV];                  /* 255 at a strike, falling */
    uint8_t note[SK_NV];                 /* the last note struck */
    uint8_t cnt[SK_NV];                  /* strikes (wraps) */
    uint8_t step;                        /* the sixteenth now (0..15) */
} sk_ui;

static uint32_t sk_rand(void)
{
    uint32_t x = sk_c.rnd ? sk_c.rnd : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    sk_c.rnd = x;
    return x;
}

static inline int32_t sk_noise(int32_t *st)           /* white, +-32767 */
{
    uint32_t s = (uint32_t)*st;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    *st = (int32_t)s;
    return (int32_t)(s >> 16) - 32768;
}

static inline int32_t sk_clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

/* sin and cos of w (Q29 radians, 0 .. pi) in Q29, by Taylor series after folding into 0 .. pi/2 (the constants are
 * 2^29 / n!); at a strike, not per sample */
static void sk_sincos(int32_t w, int32_t *s, int32_t *c)
{
    int64_t x, x2, a;
    int neg = 0;
    if (w > SK_HALFPI_Q29) {
        w = SK_PI_Q29 - w;
        neg = 1;
    }
    if (w < 0)
        w = 0;
    x = w;
    x2 = (x * x) >> 29;
    a = 148;
    a = 13316 - ((x2 * a) >> 29);
    a = 745654 - ((x2 * a) >> 29);
    a = 22369621 - ((x2 * a) >> 29);
    a = 268435456 - ((x2 * a) >> 29);
    a = 536870912 - ((x2 * a) >> 29);
    *c = (int32_t)(neg ? -a : a);
    a = 1479;
    a = 106522 - ((x2 * a) >> 29);
    a = 4473924 - ((x2 * a) >> 29);
    a = 89478485 - ((x2 * a) >> 29);
    a = 536870912 - ((x2 * a) >> 29);
    *s = (int32_t)((x * a) >> 29);
}

static inline int32_t sk_sin(uint32_t ph) { return SK_SIN[ph >> 22]; }     /* ph: 2^32 a cycle */

/* ------------------------------------------------------------- the strikes --- */
static void sk_pan(sk_voice_t *v, int pan)             /* -64 .. 63 -> the equal-power gains */
{
    int p = sk_clampi(pan + 64, 0, 127) * 2;           /* 0 .. 254 of a quarter turn (256) */
    v->gr = SK_SIN[p];
    v->gl = SK_SIN[256 - p];
}

static void sk_start_res(sk_voice_t *v, uint32_t note, uint32_t mat, uint32_t decay, uint32_t vel, uint32_t hard)
{
    uint32_t k, nm = 0;
    for (k = 0; k < SK_NM; k++) {
        uint64_t w64 = ((uint64_t)SK_W[note] * SK_RATIO[mat][k]) >> 10;
        int32_t s, co, r;
        int idx = (int)decay - 2 * (int)k;
        if (w64 >= SK_WMAX)
            break;
        idx = idx < 0 ? 0 : idx;
        sk_sincos((int32_t)w64, &s, &co);
        r = SK_R[idx];
        v->a1[k] = (int32_t)(((int64_t)co * 2 * r) >> 29);
        v->a2[k] = -(int32_t)(((int64_t)r * r) >> 29);
        v->b[k] = (int32_t)(((int64_t)s * SK_LEVEL[mat][k]) >> 15);
        nm++;
    }
    for (k = nm; k < SK_NM; k++)
        v->b[k] = v->a1[k] = v->a2[k] = 0;
    v->nm = (uint8_t)nm;
    v->env = 12000 + (int32_t)vel * 160;                              /* the exciter: a burst of noise */
    v->envk = 32768 - 60 - (int32_t)(127u - hard) * 5;                /* ~60 .. 500 samples */
    v->lpk = 3000 + (int32_t)hard * 200;                              /* its brightness */
    v->act = SK_TAU[decay < 24u ? decay : 23u] * 6u + 2000u;
}

static void sk_start_pluck(sk_voice_t *v, uint32_t note, uint32_t decay, uint32_t vel, uint32_t hard, uint32_t vi)
{
    uint32_t per = SK_PER[note < 21u ? 21u : note];                   /* Q16 samples */
    uint32_t tau = SK_TAU[decay < 24u ? decay : 23u], p = per >> 16;
    int32_t lpk0 = 9000 + (int32_t)hard * 160;                        /* the loop's low-pass at 220 Hz: 0.27 .. 0.96 */
    int32_t lpk = 32768 - (int32_t)(((uint32_t)(32768 - lpk0) * (per >> 16)) / 200u);   /* (a high string is filtered less: the pole's
                                                                                       * loss at the fundamental would kill it) */
    int32_t lag;
    lpk = sk_clampi(lpk, 8000, 32000);
    lag = ((32768 - lpk) << 8) / lpk;                                 /* its delay, in 1/256 samples */
    if (p < 8u)
        p = 8u;
    per = per > ((uint32_t)lag << 8) ? per - ((uint32_t)lag << 8) : per;
    if ((per >> 16) >= SK_PLN - 2u)
        per = (SK_PLN - 3u) << 16;
    v->per = per;
    v->pg = (int32_t)(32768u * tau / (tau + p));                      /* the loop gain per pass */
    v->plpk = lpk;
    v->exc = (per >> 16) + 1u;                                        /* the noise burst: one period of the string */
    v->env = 6000 + (int32_t)vel * 110;
    v->act = tau * 5u + 2000u;
    (void)vi;
}

static void sk_start_bell(sk_voice_t *v, uint32_t note, uint32_t mat, uint32_t decay, uint32_t vel, uint32_t hard)
{
    uint64_t im = ((uint64_t)SK_INC[note] * SK_FMR[mat & 3u]) >> 8;
    uint32_t tau = SK_TAU[decay < 24u ? decay : 23u];
    v->inc[0] = SK_INC[note];
    v->incm = (uint32_t)im;
    v->idx = (8000 + (int32_t)hard * 150) << 8;                       /* the index (Q8): 0.5 .. 1.6 cycles */
    v->idxk = SK_R[decay > 6u ? decay - 6u : 0u] >> 14;               /* dies ~2.5 x faster than the note */
    v->amp = (3200 + (int32_t)vel * 55) << 16;                        /* (Q16: the decay's steps stay small) */
    v->ampk = SK_R[decay < 24u ? decay : 23u] >> 14;
    v->act = tau * 6u + 2000u;
    if (v->act < 4000u)
        v->act = 4000u;
}

static const uint32_t SK_DET[3] = {65200, 65536, 65872};              /* a pad's three oscillators: -9, 0, +9 cents */
static void sk_start_pad(sk_voice_t *v, uint32_t note, uint32_t len, uint32_t vel, uint32_t hard, uint32_t steplen)
{
    uint32_t k;
    for (k = 0; k < 3u; k++) {
        v->tinc[k] = (uint32_t)(((uint64_t)SK_INC[note] * SK_DET[k]) >> 16);
        if (!v->act)
            v->inc[k] = v->tinc[k];
    }
    v->idx = 6000 + (int32_t)hard * 120;                              /* the feedback: the saw-ness */
    v->tgt = (5200 + (int32_t)vel * 56) << 8;                         /* (Q23) */
    v->gate = len * steplen;
    v->sh = 13;
    v->act = v->gate + 90000u;
}

static void sk_start_air(sk_voice_t *v, uint32_t note, uint32_t len, uint32_t vel, uint32_t hard, uint32_t steplen)
{
    uint32_t n = note > 90u ? 90u : note;
    v->f1 = (int32_t)(SK_W[n] >> 14);                                 /* the Chamberlin filter's f (Q15) */
    v->q = 3600 - (int32_t)hard * 20;                                 /* damping (Q15): narrow when soft (Q 8 .. 30) */
    {
        int32_t reg = 256 + (note >= 57u ? -6 * ((int32_t)note - 57) : 3 * (57 - (int32_t)note));    /* a constant-Q band's level grows with pitch */
        reg = sk_clampi(reg, 60, 400);
        v->tgt = (((2200 + (int32_t)vel * 30) * reg) >> 8) * 150;       /* (<< 8 and x0.6 in one) */
    }
    v->gate = len * steplen;
    v->sh = 12;
    v->act = v->gate + 70000u;
}

/* ------------------------------------------------------------ the conductor --- */
static void sk_step(void)
{
    uint32_t i, st = sk_c.step++;
    const sk_biome_t *b = sk_s.b;
    uint32_t dens = sk_s.mac[0], mot = sk_s.mac[1], col = sk_s.mac[2];
    uint32_t dens_q8 = 77u + ((dens * 358u) >> 7);                    /* x0.3 .. x1.7 the chance */
    sk_ui.step = (uint8_t)(st & 15u);
    if ((st & 15u) == 15u) {                                          /* a bar ends: the patterns drift with MOTION */
        uint32_t r = sk_rand();
        if ((r & 127u) < (mot >> 1)) {
            uint32_t vi = (r >> 8) % SK_NV;
            sk_s.rot[vi] = (uint8_t)(sk_s.rot[vi] + 1u + ((r >> 12) & 1u));
        }
    }
    for (i = 0; i < SK_NV; i++) {
        const sk_vcfg_t *c = &b->v[i];
        uint32_t n = c->n ? c->n : 16u, hit, r, chance;
        sk_voice_t *v = &sk_v[i];
        uint32_t type = sk_s.type[i];
        int deg, span, dcy;
        uint32_t note, vel, hard, len;
        if (sk_s.mute & (1u << i))
            continue;
        hit = ((((st + c->rot + sk_s.rot[i]) % n) * c->k) % n) < c->k;
        r = sk_rand();
        chance = (c->prob * dens_q8) >> 8;
        chance = chance > 255u ? 255u : chance;
        if (!hit) {                                                   /* a ghost: at the top of DENSITY, between hits */
            if (dens > 100u && (r & 255u) < (dens - 100u) * 3u && (type == VT_RES || type == VT_PLUCK || type == VT_BELL))
                chance = 90u;
            else
                continue;
        }
        if ((r & 255u) >= chance)
            continue;
        r >>= 8;
        deg = v->deg;
        span = (int)b->len * (int)(c->span ? c->span : 2u) / 2;       /* the reach in notes, up and down */
        if (((r >> 4) & 127u) < (mot >> 2))                           /* now and then a jump */
            deg = (int)((r >> 11) % (uint32_t)(span * 2 + 1)) - span;
        else
            deg += (int)((r >> 4) % (mot > 40u ? 5u : 3u)) - (mot > 40u ? 2 : 1);
        deg = deg < -span ? -span : deg > span ? span : deg;
        v->deg = (int8_t)deg;
        {
            int oct = deg >= 0 ? deg / (int)b->len : -((-deg + (int)b->len - 1) / (int)b->len);
            int in = deg - oct * (int)b->len;
            int nn = (int)c->base + 12 * oct + b->iv[in] + b->root;
            note = (uint32_t)sk_clampi(nn, 12, 120);
        }
        vel = (uint32_t)c->vel + ((r >> 16) & 15u) - 8u;              /* a little human */
        vel = vel * (uint32_t)sk_s.level[i] / 80u;
        if (vel > 127u)
            vel = 127u;
        hard = (uint32_t)sk_clampi((int)c->hard + ((int)col - 64) * 2 / 3, 0, 127);
        dcy = sk_clampi((int)c->decay + sk_s.decay[i], 0, 23);
        len = (uint32_t)sk_clampi((int)c->decay + sk_s.decay[i] * 2, 1, 32);
        {
            int pw = (int)v->panw + (int)((r >> 20) % 5u) - 2;        /* the pan wanders with MOTION */
            int lim = (int)(mot >> 1);
            v->panw = (int8_t)sk_clampi(pw, -lim, lim);
            sk_pan(v, sk_s.pan[i] + v->panw);
        }
        v->type = (uint8_t)type;
        switch (type) {
        case VT_PLUCK: sk_start_pluck(v, note, (uint32_t)dcy, vel, hard, i); break;
        case VT_BELL: sk_start_bell(v, note, sk_s.mat[i], (uint32_t)dcy, vel, hard); break;
        case VT_PAD: sk_start_pad(v, note, len, vel, hard, sk_c.steplen); break;
        case VT_AIR: sk_start_air(v, note, len, vel, hard, sk_c.steplen); break;
        default: sk_start_res(v, note, sk_s.mat[i], (uint32_t)dcy, vel, hard); break;
        }
        sk_ui.note[i] = (uint8_t)note;
        sk_ui.lit[i] = 255;
        sk_ui.cnt[i]++;
    }
}

/* a note of voice i's register: scale degree deg (any integer; the scale repeats each octave) */
static uint32_t sk_note_of(uint32_t i, int deg)
{
    const sk_biome_t *b = sk_s.b;
    int oct = deg >= 0 ? deg / (int)b->len : -((-deg + (int)b->len - 1) / (int)b->len);
    int in = deg - oct * (int)b->len;
    int nn = (int)b->v[i].base + 12 * oct + b->iv[in] + b->root;
    return (uint32_t)sk_clampi(nn, 12, 120);
}

/* strike voice i now, from outside the conductor (the physics sequencer's balls): the voice's own sound and settings */
static void sk_play(uint32_t i, uint32_t note, uint32_t vel)
{
    const sk_vcfg_t *c = &sk_s.b->v[i];
    sk_voice_t *v = &sk_v[i];
    uint32_t type = sk_s.type[i], hard, len;
    int dcy;
    if (sk_s.mute & (1u << i))
        return;
    vel = vel * (uint32_t)sk_s.level[i] / 80u;
    if (vel > 127u)
        vel = 127u;
    hard = (uint32_t)sk_clampi((int)c->hard + ((int)sk_s.mac[2] - 64) * 2 / 3, 0, 127);
    dcy = sk_clampi((int)c->decay + sk_s.decay[i], 0, 23);
    len = (uint32_t)sk_clampi((int)c->decay + sk_s.decay[i] * 2, 1, 32);
    sk_pan(v, sk_s.pan[i]);
    v->type = (uint8_t)type;
    switch (type) {
    case VT_PLUCK: sk_start_pluck(v, note, (uint32_t)dcy, vel, hard, i); break;
    case VT_BELL: sk_start_bell(v, note, sk_s.mat[i], (uint32_t)dcy, vel, hard); break;
    case VT_PAD: sk_start_pad(v, note, len, vel, hard, sk_c.steplen); break;
    case VT_AIR: sk_start_air(v, note, len, vel, hard, sk_c.steplen); break;
    default: sk_start_res(v, note, sk_s.mat[i], (uint32_t)dcy, vel, hard); break;
    }
    sk_ui.note[i] = (uint8_t)note;
    sk_ui.lit[i] = 255;
    sk_ui.cnt[i]++;
}

static void sk_steplen_set(void)
{
    sk_c.steplen = 661500u / (sk_s.bpm ? sk_s.bpm : 90u);            /* a sixteenth: 44100 * 60 / (4 bpm) samples */
}

/* a biome begins (the voices keep ringing: nothing is cut) */
static void sk_load(const sk_biome_t *b)
{
    uint32_t i;
    sk_s.b = b;
    for (i = 0; i < 4u; i++)
        sk_s.mac[i] = b->mac[i];
    sk_s.bpm = b->bpm;
    for (i = 0; i < SK_NV; i++) {
        sk_s.type[i] = b->v[i].type;
        sk_s.mat[i] = b->v[i].mat;
        sk_s.pan[i] = b->v[i].pan;
        sk_s.decay[i] = 0;
        sk_s.rot[i] = 0;
        sk_s.level[i] = 80;
        sk_v[i].deg = 0;
        sk_v[i].panw = 0;
        if (!sk_v[i].nz)
            sk_v[i].nz = (int32_t)(0x2545F491u + i * 0x9E3779B1u);          /* (xorshift sticks at 0) */
    }
    sk_s.mute = 0;
    sk_steplen_set();
}

/* the top of every block: the conductor's clock */
static void sk_tick(uint32_t n)
{
    uint32_t i;
    if (!sk_s.hold) {
        sk_c.clk += n;
        while (sk_c.clk >= sk_c.steplen) {
            sk_c.clk -= sk_c.steplen;
            sk_step();
        }
    }
    if (++sk_c.fall >= 2u) {                                          /* the light falls over ~0.5 s */
        sk_c.fall = 0;
        for (i = 0; i < SK_NV; i++)
            if (sk_ui.lit[i])
                sk_ui.lit[i] = (uint8_t)(sk_ui.lit[i] > 3u ? sk_ui.lit[i] - 3u : 0u);
    }
}

/* -------------------------------------------------------------- the voices --- */
/* each renders n samples of mono into o[] (Q15, a struck voice peaks at about +-8000) */
static void sk_run_res(sk_voice_t *v, int32_t *o, uint32_t n)
{
    int32_t y1[SK_NM], y2[SK_NM], a1[SK_NM], a2[SK_NM], b[SK_NM];
    int32_t env = v->env, lp = v->lp, nz = v->nz;
    uint32_t nm = v->nm, i, k;
    for (k = 0; k < SK_NM; k++) {
        y1[k] = v->y1[k];
        y2[k] = v->y2[k];
        a1[k] = v->a1[k];
        a2[k] = v->a2[k];
        b[k] = v->b[k];
    }
    for (i = 0; i < n; i++) {
        int32_t x;
        int64_t acc = 0;
        x = sk_noise(&nz);
        x = (x * (env >> 3)) >> 12;
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
        o[i] = (int32_t)(acc >> 3);
    }
    for (k = 0; k < SK_NM; k++) {
        v->y1[k] = y1[k];
        v->y2[k] = y2[k];
    }
    v->env = env;
    v->lp = lp;
    v->nz = nz;
}

static void sk_run_pluck(sk_voice_t *v, int16_t *buf, int32_t *o, uint32_t n)
{
    uint32_t wp = v->wp, i;
    int32_t st = v->pst, nz = v->nz;
    for (i = 0; i < n; i++) {
        uint32_t rp = (wp << 16) - v->per, ip = rp >> 16, fr = rp & 0xFFFFu;
        int32_t s0 = buf[ip & (SK_PLN - 1u)], s1 = buf[(ip + 1u) & (SK_PLN - 1u)];
        int32_t y = s0 + (int32_t)(((int64_t)(s1 - s0) * (int32_t)fr) >> 16), w;
        st += (int32_t)(((int64_t)(y - st) * v->plpk + 16384) >> 15);
        w = (int32_t)(((int64_t)st * v->pg + 16384) >> 15);               /* (rounded: a truncating loop leaks its tail away) */
        if (v->exc) {
            w += (sk_noise(&nz) * (v->env >> 3)) >> 10;               /* (hot: the line has headroom, the output takes it back) */
            v->exc--;
        }
        w = sk_clampi(w, -32000, 32000);
        buf[wp & (SK_PLN - 1u)] = (int16_t)w;
        wp++;
        v->lp += ((st << 8) - v->lp) >> 9;                             /* (the output carries no DC: the burst is not zero-mean) */
        o[i] = (st - (v->lp >> 8)) >> 2;
    }
    v->wp = wp;
    v->pst = st;
    v->nz = nz;
}

static void sk_run_bell(sk_voice_t *v, int32_t *o, uint32_t n)
{
    uint32_t ph = v->ph[0], pm = v->phm, i;
    int32_t idx = v->idx, amp = v->amp;
    for (i = 0; i < n; i++) {
        int32_t m = sk_sin(pm), c;
        c = sk_sin(ph + (uint32_t)m * (uint32_t)(idx >> 5));
        o[i] = (int32_t)(((int64_t)c * amp) >> 31);
        ph += v->inc[0];
        pm += v->incm;
        idx = (int32_t)(((int64_t)idx * v->idxk) >> 15);
        amp = (int32_t)(((int64_t)amp * v->ampk) >> 15);
    }
    v->ph[0] = ph;
    v->phm = pm;
    v->idx = idx;
    v->amp = amp;
}

static void sk_run_pad(sk_voice_t *v, int32_t *o, uint32_t n)
{
    uint32_t i, k;
    int32_t amp = v->amp, tgt = v->tgt, lp = v->lp, fb[3];
    for (k = 0; k < 3u; k++)
        fb[k] = v->fb[k];
    for (i = 0; i < n; i++) {
        int32_t s = 0;
        for (k = 0; k < 3u; k++) {
            int32_t c;
            v->inc[k] += (int32_t)(v->tinc[k] - v->inc[k]) >> 11;           /* the glide */
            c = sk_sin(v->ph[k] + (uint32_t)fb[k] * (uint32_t)v->idx * 4u);
            fb[k] = (fb[k] + c) >> 1;
            v->ph[k] += v->inc[k];
            s += c;
        }
        amp += (tgt - amp) >> v->sh;
        lp += (int32_t)(((int64_t)(s - lp) * 9000) >> 15);                  /* a gentle low-pass: no fizz */
        o[i] = (int32_t)(((int64_t)lp * (amp >> 8)) >> 17);
        if (v->gate) {
            if (!--v->gate)
                tgt = 0;
        }
    }
    v->amp = amp;
    v->tgt = tgt;
    v->lp = lp;
    for (k = 0; k < 3u; k++)
        v->fb[k] = fb[k];
    if (!v->gate && amp < (60 << 8))
        v->act = 0;
}

static void sk_run_air(sk_voice_t *v, int32_t *o, uint32_t n)
{
    uint32_t i;
    int32_t amp = v->amp, tgt = v->tgt, nz = v->nz;
    int32_t bp0 = v->bp[0], lo0 = v->lo[0], bp1 = v->bp[1], lo1 = v->lo[1];
    for (i = 0; i < n; i++) {
        int32_t x = sk_noise(&nz) >> 1, hp, y;
        lo0 += (int32_t)(((int64_t)v->f1 * bp0) >> 15);
        hp = x - lo0 - (int32_t)(((int64_t)v->q * bp0) >> 15);
        bp0 += (int32_t)(((int64_t)v->f1 * hp) >> 15);
        lo1 += (int32_t)(((int64_t)(v->f1 * 2) * bp1) >> 15);                /* the octave above, thinner */
        hp = x - lo1 - (int32_t)(((int64_t)(v->q * 2) * bp1) >> 15);
        bp1 += (int32_t)(((int64_t)(v->f1 * 2) * hp) >> 15);
        bp0 = sk_clampi(bp0, -3900000, 3900000);
        bp1 = sk_clampi(bp1, -3900000, 3900000);
        amp += (tgt - amp) >> v->sh;
        y = bp0 + (bp1 >> 1);
        o[i] = (int32_t)(((int64_t)y * (amp >> 8)) >> 13);
        if (v->gate) {
            if (!--v->gate)
                tgt = 0;
        }
    }
    v->amp = amp;
    v->tgt = tgt;
    v->nz = nz;
    v->bp[0] = bp0;
    v->bp[1] = bp1;
    v->lo[0] = lo0;
    v->lo[1] = lo1;
    if (!v->gate && amp < (60 << 8))
        v->act = 0;
}

/* the voices of a block: the dry stereo into l[] r[], and each voice's share into the reverb's and the grains' busses */
static void sk_voices(int32_t *l, int32_t *r, int32_t *rv, int32_t *gr, uint32_t n)
{
    uint32_t vi, i;
    int32_t o[SK_CTL];
    uint32_t sp = sk_s.mac[3];
    for (vi = 0; vi < SK_NV; vi++) {
        sk_voice_t *v = &sk_v[vi];
        const sk_vcfg_t *c = &sk_s.b->v[vi];
        int32_t gl, gr_, g, srv, sgr;
        if (!v->act)
            continue;
        switch (v->type) {
        case VT_PLUCK: sk_run_pluck(v, sk_pl[vi], o, n); break;
        case VT_BELL: sk_run_bell(v, o, n); break;
        case VT_PAD: sk_run_pad(v, o, n); break;
        case VT_AIR: sk_run_air(v, o, n); break;
        default: sk_run_res(v, o, n); break;
        }
        g = (int32_t)sk_s.level[vi] * 330;                           /* the voice's level: 0..100 -> up to ~1.0 */
        gl = (v->gl * (g >> 3)) >> 12;
        gr_ = (v->gr * (g >> 3)) >> 12;
        srv = (int32_t)(c->rsend * (32u + sp) >> 4);                 /* the sends follow SPACE: 0..~10 of 128 */
        sgr = (int32_t)(c->gsend * sp >> 6);
        for (i = 0; i < n; i++) {
            int32_t a = (o[i] * (gl)) >> 15, b = (o[i] * (gr_)) >> 15;
            l[i] += a;
            r[i] += b;
            rv[i] += ((a + b) * srv) >> 9;
            gr[i] += ((a + b) * sgr) >> 7;
        }
        if (v->act <= n)
            v->act = 0;
        else if (v->type != VT_PAD && v->type != VT_AIR)
            v->act -= n;
        if (!v->act) {
            uint32_t k;
            for (k = 0; k < SK_NM; k++)
                v->y1[k] = v->y2[k] = 0;
            v->env = v->lp = 0;
            v->amp = 0;
        }
    }
}
