/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* SKYLYNNE's voices and conductor: six sustained voices that play themselves.
 *
 * A biome (sk_biomes.c) gives each of the six voices a type, a register, a rhythm and a length, and the whole a tempo
 * and a scale. The conductor runs one clock: every sixteenth, each voice may start a note by its own Euclidean pattern
 * (k hits in n steps, rotated), a chance and a walk through the scale. The patterns have different lengths, so
 * together they drift against each other and the music never repeats.
 *
 * Nothing is struck. Every note swells in, holds and fades out over seconds (the length is the SUSTAIN knob; at its
 * top a voice never stops and each new note only glides the pitch). A note that arrives while the voice still sounds
 * does not restart it: the pitch glides and the swell carries on. Five kinds of voice:
 *   DRONE    four slightly detuned oscillators and an octave below, through a low-pass that drifts, the detune breathing;
 *   WHISTLE  a thin high sine with a slow vibrato that grows with the swell, and a narrow band of breath at its pitch;
 *   SUB      a deep sine with a touch of its second harmonic, gliding very slowly;
 *   PAD      three detuned feedback oscillators, wide and soft;
 *   AIR      noise through two pitched resonant bands: wind, mist.
 * The four macros the player turns (DENSITY, MOTION, COLOUR, SPACE) are read here and in sk_fx.c: how often and how
 * far the voices move, how bright they sound, and how much of them goes to the room.
 *
 * Integer only (no 64-bit divide). Included by skylynne.c; the platform (sk_hw.c or the host harness) calls
 * sk_render(). */

#include "sky_tables.h"

#define SK_NV 6                          /* voices */
#define SK_CTL 32                        /* samples a block */
#define SK_FS 44100
#define SK_ENDLESS 0x7FFFFFFFu           /* a gate that does not close */

enum { VT_DRONE, VT_WHISTLE, VT_SUB, VT_PAD, VT_AIR, VT_CHOIR, VT_BRASS, VT_ORGAN, VT_BELL, VT_FORGE, VT_STOMP, VT_DRIP,
       VT_WHALE, VT_GLASS, VT_N };
static const char *const SK_TYPE_NAME[VT_N] = {"Drone", "Whistle", "Sub", "Pad", "Air", "Choir", "Brass", "Organ", "Bell",
                                               "Forge", "Stomp", "Drip", "Whale", "Glass"};
/* the struck kinds: a short attack, then the whole length is the ring-out */
#define SK_PERC(t) ((t) == VT_BELL || (t) == VT_FORGE || (t) == VT_STOMP || (t) == VT_DRIP)
/* SUSTAIN -6..+6: the note's length times 2^(s/3) / 4 (Q8); the top step is endless */
static const uint16_t SK_SUSM[13] = {64, 81, 102, 128, 161, 203, 256, 323, 406, 512, 645, 813, 1024};

/* the scales the SCL page offers: name, notes, intervals */
typedef struct { const char *name; uint8_t len, iv[8]; } sk_scale_t;
#define SK_NSCALE 9u
static const sk_scale_t SK_SCALES[SK_NSCALE] = {
    {"Minor", 7, {0, 2, 3, 5, 7, 8, 10, 0}},
    {"Dorian", 7, {0, 2, 3, 5, 7, 9, 10, 0}},
    {"Phrygian", 7, {0, 1, 3, 5, 7, 8, 10, 0}},
    {"Lydian", 7, {0, 2, 4, 6, 7, 9, 11, 0}},
    {"Major", 7, {0, 2, 4, 5, 7, 9, 11, 0}},
    {"Min Pent", 5, {0, 3, 5, 7, 10, 0, 0, 0}},
    {"Maj Pent", 5, {0, 2, 4, 7, 9, 0, 0, 0}},
    {"Hirajoshi", 5, {0, 2, 3, 7, 8, 0, 0, 0}},
    {"Whole", 6, {0, 2, 4, 6, 8, 10, 0, 0}},
};
static const char *const SK_NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

/* one voice of a biome: what it is and how it plays */
typedef struct {
    uint8_t type;                        /* the kind of voice */
    uint8_t base;                        /* the middle note of its register (MIDI) */
    uint8_t n, k, rot;                   /* Euclid: k hits in n steps, rotated */
    uint8_t prob;                        /* the chance a hit sounds, 0..255 */
    uint8_t vel;                         /* 0..127 */
    int8_t pan;                          /* -64..63 */
    uint8_t len;                         /* the note's length in sixteenths (SUSTAIN 0) */
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
    uint8_t world;                       /* 0 the city, 1 the cathedral, 2 the deep (sk_biomes.c): its sounds and its scene */
} sk_biome_t;

typedef struct {
    uint32_t ph[5], inc[5], tinc[5];                                   /* the oscillators */
    uint32_t lph[2], linc[2];                                          /* two slow movements */
    int32_t fb[4], idx;                                                /* feedback (drone, pad) */
    int32_t lp, lpk;                                                   /* the low-pass */
    int32_t f1, q, bp, lo, br;                                         /* air, whistle: the band-pass; br: breath level */
    int32_t bp2, lo2, nz;
    int32_t bp3, lo3, f2, f3, tf[3], pd;                               /* choir formants; the pitch bend of a struck voice */
    int32_t amp, tgt, astep, rstep, peak;                              /* the swell: amp 0..32767 in Q8, peak: the level */
    uint32_t gate;                                                     /* samples to the note's end */
    int32_t gl, gr;                                                    /* pan, Q15 */
    uint32_t act;                                                      /* 0: idle, no work */
    uint8_t type;
    int8_t deg;
    int8_t panw;                                                       /* the pan's wander */
    int8_t gli;                                                        /* the glide, -6..+6 */
    uint8_t crv;                                                       /* the swell's curve, 0..127 */
} sk_voice_t;

#ifndef SK_POOL
#define SK_POOL
#endif
static sk_voice_t sk_v[SK_NV] SK_POOL;

/* the live state: what the player has changed */
static struct {
    const sk_biome_t *b;
    uint8_t mac[4];                      /* DENSITY, MOTION, COLOUR, SPACE, 0..127 */
    uint8_t level[SK_NV];                /* 0..100 */
    uint8_t type[SK_NV];                 /* a voice's kind (the biome's until changed) */
    int8_t tone[SK_NV];                  /* -6 .. +6: darker .. brighter */
    int8_t pan[SK_NV];
    int8_t sus[SK_NV];                   /* -6 .. +6: SUSTAIN (+6: endless) */
    uint8_t mute;                        /* bit v: voice v is silent */
    uint8_t hold;                        /* the conductor waits (the sound carries on) */
    uint8_t bpm;
    uint8_t sel;                         /* the voice the player is editing */
    uint8_t rot[SK_NV];                  /* the patterns' rotation (drifts with MOTION) */
    uint8_t fx[4];                       /* FX page: ROOM, ECHO, CLOUD, HIGH CUT, 0..127 (64 is the biome's own) */
    uint8_t scl, root, len;              /* SCL page: the scale (SK_NSCALE: a biome's own), the key, the notes */
    uint8_t iv[8];
    int8_t shift, walk;                  /* octaves -2..+2; the walk's reach -3..+3 */
    uint8_t lfo[4];                      /* LFO page: RATE, DEPTH, PAN, VIBRATO, 0..127 */
    int8_t atk[SK_NV], rel[SK_NV];       /* ENV page: the swell and the fade, -6..+6 */
    int8_t gli[SK_NV];
    uint8_t crv[SK_NV];
} sk_s;

static struct {
    uint32_t step, clk, rnd, steplen, fall, bar;
} sk_c;

/* what the screen reads (written by the audio side; single bytes) */
static volatile struct {
    uint8_t lit[SK_NV];                  /* 255 at a note, falling */
    uint8_t note[SK_NV];                 /* the last note */
    uint8_t cnt[SK_NV];                  /* notes (wraps) */
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

static inline int32_t sk_sin(uint32_t ph) { return SK_SIN[ph >> 22]; }     /* ph: 2^32 a cycle */

/* ------------------------------------------------------------- the strikes --- */
static void sk_pan(sk_voice_t *v, int pan)             /* -64 .. 63 -> the equal-power gains */
{
    int p = sk_clampi(pan + 64, 0, 127) * 2;           /* 0 .. 254 of a quarter turn (256) */
    v->gr = SK_SIN[p];
    v->gl = SK_SIN[256 - p];
}

/* ------------------------------------------------------------- the notes --- */
/* a note begins: the swell (its speed from the length), the pitch targets, the colour. A voice that still sounds keeps
 * its phase, its swell and its pitch's place: the pitch glides and the swell goes on. */
static void sk_start(sk_voice_t *v, uint32_t type, uint32_t note, uint32_t len, uint32_t vel, uint32_t hard,
                     uint32_t steplen)
{
    uint32_t k, att, rel, nn = note > 100u ? 100u : note, fresh = !v->act, vi = (uint32_t)(v - sk_v);
    uint32_t lr = 32u + (uint32_t)sk_s.lfo[0] * 5u;                      /* the LFO rate, Q8 (x0.12 .. x3.1) */
    v->gate = len >= 900u ? SK_ENDLESS : len * steplen;
    att = v->gate / 3u;
    att = (att * SK_SUSM[sk_s.atk[vi] + 6]) >> 8;                       /* ENV: x1/4 .. x4 */
    att = att < 2000u ? 2000u : att > 900000u ? 900000u : att;          /* 0.05 .. 20 s */
    rel = v->gate / 2u;
    rel = (rel * SK_SUSM[sk_s.rel[vi] + 6]) >> 8;
    rel = rel < 8000u ? 8000u : rel > 1500000u ? 1500000u : rel;        /* 0.2 .. 34 s */
    if (SK_PERC(type)) {                                                /* struck: a click of an attack, then it rings */
        uint32_t full = len >= 900u ? 1500000u : len * steplen;
        att = 300u;
        v->gate = 400u;
        rel = (full * SK_SUSM[sk_s.rel[vi] + 6]) >> 8;
        rel = rel < 8000u ? 8000u : rel > 1500000u ? 1500000u : rel;
    }
    v->astep = (int32_t)(8388352u / att);
    v->rstep = (int32_t)(8388352u / rel);
    v->tgt = 32767 << 8;
    if (fresh) {
        v->amp = 0;
        v->lp = 0;
        v->bp = v->lo = v->bp2 = v->lo2 = v->bp3 = v->lo3 = 0;
        for (k = 0; k < 4u; k++)
            v->fb[k] = 0;
        v->lph[0] = sk_rand();
        v->lph[1] = sk_rand();
    }
    v->type = (uint8_t)type;
    v->gli = sk_s.gli[vi];
    v->crv = sk_s.crv[vi];
    switch (type) {
    case VT_DRONE: {
        static const uint32_t DET[4] = {64950, 65350, 65750, 66150};     /* about -15, -5, +5, +15 cents */
        for (k = 0; k < 4u; k++)
            v->tinc[k] = (uint32_t)(((uint64_t)SK_INC[nn] * DET[k]) >> 16);
        v->tinc[4] = SK_INC[nn] >> 1;                                    /* the octave below */
        v->idx = 2400 + (int32_t)hard * 40;
        v->lpk = 700 + (int32_t)hard * 110;                              /* the filter's top: dark to open */
        v->peak = 3200 + (int32_t)vel * 48;
        v->linc[0] = 6000u + (sk_rand() & 8191u);                        /* 0.06 .. 0.15 Hz */
        v->linc[1] = 14000u + (sk_rand() & 16383u);                      /* 0.15 .. 0.4 Hz */
        break;
    }
    case VT_PAD: {
        static const uint32_t DET[3] = {65200, 65536, 65872};
        for (k = 0; k < 3u; k++)
            v->tinc[k] = (uint32_t)(((uint64_t)SK_INC[nn] * DET[k]) >> 16);
        v->idx = 4000 + (int32_t)hard * 70;
        v->lpk = 1200 + (int32_t)hard * 130;
        v->peak = 2900 + (int32_t)vel * 45;
        v->linc[0] = 5000u + (sk_rand() & 8191u);
        v->linc[1] = 12000u + (sk_rand() & 16383u);
        break;
    }
    case VT_WHISTLE:
        v->tinc[0] = SK_INC[nn];
        v->f1 = (int32_t)(SK_W[nn > 90u ? 90u : nn] >> 14);
        v->q = 900 - (int32_t)hard * 4;                                  /* a very narrow band: a tone, not a hiss */
        v->br = 250 + (int32_t)hard * 6;
        v->peak = 2400 + (int32_t)vel * 32;
        v->linc[0] = 380000u + (sk_rand() & 65535u);                     /* the vibrato: 4 .. 5.5 Hz */
        v->linc[1] = 9000u + (sk_rand() & 8191u);                        /* the vibrato's own swell */
        break;
    case VT_SUB:
        v->tinc[0] = SK_INC[nn];
        v->tinc[1] = SK_INC[nn] << 1;
        v->peak = 4700 + (int32_t)vel * 57;
        v->linc[0] = 4000u + (sk_rand() & 4095u);
        v->linc[1] = 9000u;
        break;
    case VT_CHOIR: {
        static const uint16_t VOW[5][3] = {{800, 1150, 2900}, {450, 800, 2830}, {400, 1700, 2600}, {325, 700, 2700},
                                           {270, 2100, 3000}};          /* ah oh eh oo ee */
        static const uint32_t DET[3] = {65290, 65536, 65780};
        uint32_t vw = sk_rand() % 5u, f = 4781u;
        for (k = 0; k < 3u; k++)
            v->tinc[k] = (uint32_t)(((uint64_t)SK_INC[nn] * DET[k]) >> 16);
        for (k = 0; k < 3u; k++)
            v->tf[k] = (int32_t)((VOW[vw][k] * f) >> 10);
        if (fresh) {
            v->f1 = v->tf[0];
            v->f2 = v->tf[1];
            v->f3 = v->tf[2];
        }
        v->q = 2600 - (int32_t)hard * 8;                                 /* open vowels when bright, hollow when dark */
        v->br = 40 + (int32_t)hard * 3;                                  /* the breath in the voice */
        v->peak = 2600 + (int32_t)vel * 36;
        v->linc[0] = 330000u + (sk_rand() & 65535u);
        v->linc[1] = 7000u + (sk_rand() & 8191u);
        break;
    }
    case VT_BRASS: {
        static const uint32_t DET[3] = {65480, 65536, 65600};
        for (k = 0; k < 3u; k++)
            v->tinc[k] = (uint32_t)(((uint64_t)SK_INC[nn] * DET[k]) >> 16);
        v->lpk = 900 + (int32_t)hard * 90;
        v->peak = 2700 + (int32_t)vel * 40;
        v->linc[0] = 280000u + (sk_rand() & 32767u);
        v->linc[1] = 6000u + (sk_rand() & 8191u);
        break;
    }
    case VT_ORGAN: {
        for (k = 0; k < 4u; k++)                                         /* 8' 4' 2 2/3' 2' and the 16' */
            v->tinc[k] = SK_INC[nn > 96u ? 96u : nn] * (k + 1u);
        v->tinc[4] = SK_INC[nn] >> 1;
        v->idx = (int32_t)hard;
        v->peak = 1250 + (int32_t)vel * 17;
        v->linc[0] = 30000u + (sk_rand() & 16383u);                      /* the slow shimmer */
        v->linc[1] = 6000u + (sk_rand() & 4095u);
        break;
    }
    case VT_BELL:
    case VT_FORGE: {
        static const uint32_t RB[5] = {32768, 65536, 180879, 353894, 585236};     /* a bell: hum, prime, tierce, and up */
        static const uint32_t RF[5] = {65536, 152044, 278528, 434176, 616000};    /* an anvil's bar */
        const uint32_t *rt = type == VT_BELL ? RB : RF;
        uint32_t nb = nn > 88u ? 88u : nn;
        for (k = 0; k < 5u; k++)
            v->tinc[k] = (uint32_t)(((uint64_t)SK_INC[nb] * rt[k]) >> 16);
        v->br = type == VT_FORGE ? 90 + (int32_t)hard * 3 : 0;           /* the click of the blow */
        v->idx = 0;
        v->peak = 3000 + (int32_t)vel * 40;
        v->lpk = 3000 + (int32_t)hard * 160;                             /* how much of the top rings */
        break;
    }
    case VT_STOMP:
        v->tinc[0] = SK_INC[nn];
        v->tinc[1] = SK_INC[nn] >> 1;
        v->pd = 98304;                                                   /* the pitch starts 1.5x up and falls */
        v->peak = 3200 + (int32_t)vel * 38;
        v->lpk = 500 + (int32_t)hard * 14;                               /* the thud's low-pass */
        break;
    case VT_DRIP:
        v->tinc[0] = SK_INC[nn > 96u ? 96u : nn];
        v->pd = 65536;
        v->peak = 5000 + (int32_t)vel * 60;
        v->lpk = 100000 + (int32_t)hard * 250;                           /* how far the drop rises (Q16) */
        break;
    case VT_WHALE:
        v->tinc[0] = SK_INC[nn];
        v->idx = 7000 + (int32_t)hard * 220;
        v->peak = 3600 + (int32_t)vel * 44;
        v->linc[0] = 60000u + (sk_rand() & 16383u);
        v->linc[1] = 9000u + (sk_rand() & 8191u);
        break;
    case VT_GLASS: {
        static const uint32_t RG[5] = {65536, 65740, 177602, 178040, 337510};
        for (k = 0; k < 5u; k++)
            v->tinc[k] = (uint32_t)(((uint64_t)SK_INC[nn > 92u ? 92u : nn] * RG[k]) >> 16);
        v->idx = (int32_t)hard;
        v->peak = 2500 + (int32_t)vel * 34;
        v->linc[0] = 40000u + (sk_rand() & 16383u);
        v->linc[1] = 9000u + (sk_rand() & 8191u);
        break;
    }
    default:                                                              /* AIR */
        v->f1 = (int32_t)(SK_W[nn > 90u ? 90u : nn] >> 14);
        v->q = 3300 - (int32_t)hard * 16;
        {
            int32_t reg = 256 + (nn >= 57u ? -6 * ((int32_t)nn - 57) : 3 * (57 - (int32_t)nn));
            reg = sk_clampi(reg, 60, 400);
            v->peak = ((1500 + (int32_t)vel * 20) * reg) >> 8;
        }
        v->linc[0] = 7000u + (sk_rand() & 8191u);
        v->linc[1] = 16000u + (sk_rand() & 16383u);
        break;
    }
    v->linc[0] = (v->linc[0] * lr) >> 8;                                 /* the LFO page's rate */
    v->linc[1] = (v->linc[1] * lr) >> 8;
    if (fresh)
        for (k = 0; k < 5u; k++) {
            v->inc[k] = v->tinc[k];
            v->ph[k] = sk_rand();
        }
    if (SK_PERC(type))
        for (k = 0; k < 5u; k++)
            v->inc[k] = v->tinc[k];
    if (type == VT_WHALE)                                                 /* every call starts a fourth below and rises */
        v->inc[0] = (uint32_t)(((uint64_t)v->tinc[0] * 52000u) >> 16);
    if (type == VT_STOMP || type == VT_DRIP)
        v->lph[0] = 0;
    v->act = 1;
}

/* a note of voice i's register: scale degree deg (any integer; the scale repeats each octave) */
static uint32_t sk_note_of(uint32_t i, int deg)
{
    int len = sk_s.len ? sk_s.len : 7;
    int oct = deg >= 0 ? deg / len : -((-deg + len - 1) / len);
    int in = deg - oct * len;
    int nn = (int)sk_s.b->v[i].base + 12 * oct + sk_s.iv[in] + sk_s.root + 12 * sk_s.shift;
    return (uint32_t)sk_clampi(nn, 12, 120);
}

/* the length of voice i's note in sixteenths, from its biome length and its SUSTAIN knob */
static uint32_t sk_len_of(uint32_t i)
{
    int s = sk_s.sus[i];
    uint32_t l;
    if (s >= 6)
        return 900u;                                                  /* endless */
    l = ((uint32_t)sk_s.b->v[i].len * SK_SUSM[s + 6 < 0 ? 0 : s + 6]) >> 8;
    return l < 2u ? 2u : l > 800u ? 800u : l;
}

static uint32_t sk_hard_of(uint32_t i)
{
    return (uint32_t)sk_clampi((int)sk_s.b->v[i].hard + ((int)sk_s.mac[2] - 64) * 2 / 3 + (int)sk_s.tone[i] * 9, 0, 127);
}

/* ------------------------------------------------------------ the conductor --- */
static void sk_step(void)
{
    uint32_t i, st = sk_c.step++;
    const sk_biome_t *b = sk_s.b;
    uint32_t dens = sk_s.mac[0], mot = sk_s.mac[1];
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
        uint32_t n = c->n ? c->n : 16u, r, chance;
        sk_voice_t *v = &sk_v[i];
        int deg, span;
        uint32_t note, vel;
        if (sk_s.mute & (1u << i))
            continue;
        if (!((((st + c->rot + sk_s.rot[i]) % n) * c->k) % n < c->k))
            continue;
        r = sk_rand();
        chance = (c->prob * dens_q8) >> 8;
        chance = chance > 255u ? 255u : chance;
        if ((r & 255u) >= chance)
            continue;
        r >>= 8;
        deg = v->deg;
        span = (int)sk_s.len * (int)(c->span ? c->span : 2u) * (4 + sk_s.walk) / 8;   /* the reach in notes, up and down */
        if (span < 1)
            span = 1;
        if (((r >> 4) & 127u) < (mot >> 2))                           /* now and then a leap */
            deg = (int)((r >> 11) % (uint32_t)(span * 2 + 1)) - span;
        else
            deg += (int)((r >> 4) % (mot > 40u ? 5u : 3u)) - (mot > 40u ? 2 : 1);
        deg = deg < -span ? -span : deg > span ? span : deg;
        v->deg = (int8_t)deg;
        {
            note = sk_note_of(i, deg);
        }
        vel = (uint32_t)c->vel + ((r >> 16) & 15u) - 8u;              /* a little human */
        vel = vel * (uint32_t)sk_s.level[i] / 80u;
        if (vel > 127u)
            vel = 127u;
        {
            int pw = (int)v->panw + (int)((r >> 20) % 5u) - 2;        /* the pan wanders with MOTION */
            int lim = (int)(mot >> 1) * (int)sk_s.lfo[2] >> 6;
            v->panw = (int8_t)sk_clampi(pw, -lim, lim);
            sk_pan(v, sk_s.pan[i] + v->panw);
        }
        sk_start(v, sk_s.type[i], note, sk_len_of(i), vel, sk_hard_of(i), sk_c.steplen);
        sk_ui.note[i] = (uint8_t)note;
        sk_ui.lit[i] = 255;
        sk_ui.cnt[i]++;
    }
}

/* start voice i now, from outside the conductor (the physics sequencer's balls): the voice's own sound and settings,
 * its note a quarter of the length so that the hits stay apart */
static void sk_play(uint32_t i, uint32_t note, uint32_t vel)
{
    sk_voice_t *v = &sk_v[i];
    uint32_t len = sk_len_of(i);
    if (sk_s.mute & (1u << i))
        return;
    vel = vel * (uint32_t)sk_s.level[i] / 80u;
    if (vel > 127u)
        vel = 127u;
    len = len >= 900u ? 64u : (len >> 2) < 3u ? 3u : len >> 2;
    sk_pan(v, sk_s.pan[i]);
    sk_start(v, sk_s.type[i], note, len, vel, sk_hard_of(i), sk_c.steplen);
    sk_ui.note[i] = (uint8_t)note;
    sk_ui.lit[i] = 255;
    sk_ui.cnt[i]++;
}

static void sk_steplen_set(void)
{
    sk_c.steplen = 661500u / (sk_s.bpm ? sk_s.bpm : 90u);            /* a sixteenth: 44100 * 60 / (4 bpm) samples */
}

/* a biome begins (the voices keep sounding: nothing is cut) */
static void sk_load(const sk_biome_t *b)
{
    uint32_t i;
    sk_s.b = b;
    for (i = 0; i < 4u; i++)
        sk_s.mac[i] = b->mac[i];
    sk_s.bpm = b->bpm;
    for (i = 0; i < SK_NV; i++) {
        sk_s.type[i] = b->v[i].type;
        sk_s.pan[i] = b->v[i].pan;
        sk_s.sus[i] = 0;
        sk_s.tone[i] = 0;
        sk_s.rot[i] = 0;
        sk_s.atk[i] = sk_s.rel[i] = sk_s.gli[i] = 0;
        sk_s.crv[i] = 64;
        sk_s.level[i] = 80;
        sk_v[i].deg = 0;
        sk_v[i].panw = 0;
        if (!sk_v[i].nz)
            sk_v[i].nz = (int32_t)(0x2545F491u + i * 0x9E3779B1u);          /* (xorshift sticks at 0) */
    }
    sk_s.mute = 0;
    sk_s.fx[0] = sk_s.fx[1] = sk_s.fx[2] = 64;
    sk_s.fx[3] = 40;                                                  /* a gentle high cut: the top is soft by default */
    sk_s.lfo[0] = 45;
    sk_s.lfo[1] = sk_s.lfo[2] = sk_s.lfo[3] = 64;
    sk_s.shift = sk_s.walk = 0;
    sk_s.root = b->root;
    sk_s.len = b->len;
    sk_s.scl = SK_NSCALE;
    for (i = 0; i < 8u; i++)
        sk_s.iv[i] = b->iv[i];
    for (i = 0; i < SK_NSCALE; i++) {
        uint32_t k, same = SK_SCALES[i].len == b->len;
        for (k = 0; same && k < b->len; k++)
            same = SK_SCALES[i].iv[k] == b->iv[k];
        if (same)
            sk_s.scl = (uint8_t)i;
    }
    sk_steplen_set();
}

/* the SCL page's scale: copy a table scale in (the voices' degrees stay, so the melody keeps its shape) */
static void sk_scale_set(uint32_t i)
{
    uint32_t k;
    i %= SK_NSCALE;
    sk_s.scl = (uint8_t)i;
    sk_s.len = SK_SCALES[i].len;
    for (k = 0; k < 8u; k++)
        sk_s.iv[k] = SK_SCALES[i].iv[k];
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
/* each renders n samples of mono into o[] (Q15, about +-5000 at a normal level). The swell is a linear ramp squared;
 * the gain is interpolated across the block. Returns nothing: when the note has faded out the voice sets act to 0. */
static int32_t sk_swell(sk_voice_t *v, uint32_t n, int32_t *g0, int32_t *dg)
{
    int32_t a0 = v->amp >> 8, a1, g1;
    if (v->gate) {
        v->gate = v->gate > n ? v->gate - n : 0u;
        if (!v->gate)
            v->tgt = 0;
    }
    if (v->amp < v->tgt) {
        v->amp += v->astep * (int32_t)n;
        if (v->amp > v->tgt)
            v->amp = v->tgt;
    } else if (v->amp > v->tgt) {
        v->amp -= v->rstep * (int32_t)n;
        if (v->amp < v->tgt)
            v->amp = v->tgt;
    }
    a1 = v->amp >> 8;
    {
        int32_t c = (int32_t)v->crv, q1 = (a1 * a1) >> 15, q0 = (a0 * a0) >> 15;   /* CURVE: 0 straight .. 127 squared */
        g1 = (a1 * (127 - c) + q1 * c) / 127;
        *g0 = ((a0 * (127 - c) + q0 * c) / 127) << 8;
    }
    *dg = ((g1 << 8) - *g0) / (int32_t)n;
    if (!v->tgt && !v->amp)
        v->act = 0;
    return g1;
}

static inline void sk_glide(sk_voice_t *v, uint32_t k, uint32_t sh)
{
    int32_t d = (int32_t)(v->tinc[k] - v->inc[k]);
    sh = (uint32_t)sk_clampi((int)sh - v->gli / 2 - (v->gli > 0 ? 1 : 0), 4, 15);
    v->inc[k] += (d >> sh) + (d > 0 ? 1 : d < 0 ? -1 : 0);
    if (d > -64 && d < 64)
        v->inc[k] = v->tinc[k];
}

/* the LFO page's DEPTH (64 is as designed, up to twice) on a slow movement */
#define SK_LD(x) (((x) * (int32_t)sk_s.lfo[1]) >> 6)

static void sk_run_drone(sk_voice_t *v, int32_t *o, uint32_t n)
{
    int32_t g, dg, a, b, lpk, lp = v->lp, fb[4], pk = v->peak;
    uint32_t i, k, inc[5];
    sk_swell(v, n, &g, &dg);
    a = SK_LD(SK_SIN[v->lph[0] >> 22]);
    b = SK_LD(SK_SIN[v->lph[1] >> 22]);
    v->lph[0] += v->linc[0] * n;
    v->lph[1] += v->linc[1] * n;
    lpk = v->lpk + (int32_t)(((int64_t)(v->lpk >> 1) * b) >> 15);        /* the filter breathes: +-50% */
    lpk = sk_clampi(lpk, 200, 32000);
    for (k = 0; k < 5u; k++) {
        int32_t dev;
        sk_glide(v, k, 10);
        dev = (((int32_t)(v->inc[k] >> 14) * ((k & 1u) ? a : -a)) >> 9);   /* the detune breathes: +-6 cents */
        inc[k] = v->inc[k] + (uint32_t)dev;
        if (k < 4u)
            fb[k] = v->fb[k];
    }
    for (i = 0; i < n; i++) {
        int32_t s = 0, c;
        for (k = 0; k < 4u; k++) {
            c = SK_SIN[(v->ph[k] + (uint32_t)(fb[k] * v->idx * 4)) >> 22];
            fb[k] = (fb[k] + c) >> 1;
            v->ph[k] += inc[k];
            s += c;
        }
        c = SK_SIN[v->ph[4] >> 22];
        v->ph[4] += inc[4];
        s = (s >> 2) + (c >> 1);
        lp += (int32_t)(((int64_t)(s - lp) * lpk) >> 15);
        g += dg;
        o[i] = (int32_t)((((int64_t)lp * (g >> 8)) >> 15) * pk >> 15);
    }
    v->lp = lp;
    for (k = 0; k < 4u; k++)
        v->fb[k] = fb[k];
}

static void sk_run_pad(sk_voice_t *v, int32_t *o, uint32_t n)
{
    int32_t g, dg, a, b, lpk, lp = v->lp, fb[3], pk = v->peak;
    uint32_t i, k, inc[3];
    sk_swell(v, n, &g, &dg);
    a = SK_LD(SK_SIN[v->lph[0] >> 22]);
    b = SK_LD(SK_SIN[v->lph[1] >> 22]);
    v->lph[0] += v->linc[0] * n;
    v->lph[1] += v->linc[1] * n;
    lpk = v->lpk + (int32_t)(((int64_t)(v->lpk >> 2) * b) >> 15);
    lpk = sk_clampi(lpk, 200, 32000);
    for (k = 0; k < 3u; k++) {
        sk_glide(v, k, 9);
        inc[k] = v->inc[k] + (uint32_t)(((int32_t)(v->inc[k] >> 14) * (k == 1u ? b : (k == 0u ? a : -a))) >> 10);
        fb[k] = v->fb[k];
    }
    for (i = 0; i < n; i++) {
        int32_t s = 0, c;
        for (k = 0; k < 3u; k++) {
            c = SK_SIN[(v->ph[k] + (uint32_t)(fb[k] * v->idx * 4)) >> 22];
            fb[k] = (fb[k] + c) >> 1;
            v->ph[k] += inc[k];
            s += c;
        }
        s /= 3;
        lp += (int32_t)(((int64_t)(s - lp) * lpk) >> 15);
        g += dg;
        o[i] = (int32_t)((((int64_t)lp * (g >> 8)) >> 15) * pk >> 15);
    }
    v->lp = lp;
    for (k = 0; k < 3u; k++)
        v->fb[k] = fb[k];
}

static void sk_run_sub(sk_voice_t *v, int32_t *o, uint32_t n)
{
    int32_t g, dg, a, pk = v->peak;
    uint32_t i, i0, i1;
    sk_swell(v, n, &g, &dg);
    a = SK_LD(SK_SIN[v->lph[0] >> 22]);
    v->lph[0] += v->linc[0] * n;
    sk_glide(v, 0, 11);
    sk_glide(v, 1, 11);
    i0 = v->inc[0];
    i1 = v->inc[1];
    for (i = 0; i < n; i++) {
        int32_t s = SK_SIN[v->ph[0] >> 22] + (SK_SIN[v->ph[1] >> 22] >> 3);
        v->ph[0] += i0;
        v->ph[1] += i1;
        g += dg;
        o[i] = (int32_t)(((((int64_t)s * (g >> 8)) >> 15) * (pk + ((pk >> 4) * a >> 15))) >> 15);   /* a slow tremolo: +-6% */
    }
}

static void sk_run_whistle(sk_voice_t *v, int32_t *o, uint32_t n)
{
    int32_t g, dg, vib, pk = v->peak, br = v->br, nz = v->nz, bp = v->bp, lo = v->lo;
    uint32_t i, inc;
    sk_swell(v, n, &g, &dg);
    vib = SK_SIN[v->lph[0] >> 22];
    v->lph[0] += v->linc[0] * n;
    {
        int32_t depth = (SK_SIN[v->lph[1] >> 22] >> 1) + 16384;       /* the vibrato wanders 0 .. 1 */
        v->lph[1] += v->linc[1] * n;
        depth = (depth * (v->amp >> 8)) >> 15;                         /* and grows with the swell */
        vib = (vib * depth) >> 15;
        vib = (vib * (int32_t)sk_s.lfo[3]) >> 6;                       /* the VIBRATO knob */
    }
    sk_glide(v, 0, 9);
    inc = v->inc[0] + (uint32_t)(((int32_t)(v->inc[0] >> 14) * vib) >> 8);      /* +-0.8% at most */
    for (i = 0; i < n; i++) {
        int32_t x = sk_noise(&nz) >> 1, hp, s;
        lo += (int32_t)(((int64_t)v->f1 * bp) >> 15);
        hp = x - lo - (int32_t)(((int64_t)v->q * bp) >> 15);
        bp += (int32_t)(((int64_t)v->f1 * hp) >> 15);
        bp = sk_clampi(bp, -3000000, 3000000);
        s = SK_SIN[v->ph[0] >> 22] + (SK_SIN[(v->ph[0] << 1) >> 22] >> 4);
        v->ph[0] += inc;
        s = s + (int32_t)(((int64_t)bp * br) >> 14);
        g += dg;
        o[i] = (int32_t)((((int64_t)s * (g >> 8)) >> 15) * pk >> 15);
    }
    v->nz = nz;
    v->bp = bp;
    v->lo = lo;
}

static void sk_run_air(sk_voice_t *v, int32_t *o, uint32_t n)
{
    int32_t g, dg, a, b, f0, f1, nz = v->nz, pk = v->peak;
    int32_t bp0 = v->bp, lo0 = v->lo, bp1 = v->bp2, lo1 = v->lo2;
    uint32_t i;
    sk_swell(v, n, &g, &dg);
    a = SK_LD(SK_SIN[v->lph[0] >> 22]);
    b = SK_LD(SK_SIN[v->lph[1] >> 22]);
    v->lph[0] += v->linc[0] * n;
    v->lph[1] += v->linc[1] * n;
    f0 = v->f1 + (int32_t)(((int64_t)(v->f1 >> 3) * a) >> 15);           /* the bands drift: +-12% */
    f1 = v->f1 * 2 + (int32_t)(((int64_t)(v->f1 >> 2) * b) >> 15);
    for (i = 0; i < n; i++) {
        int32_t x = sk_noise(&nz) >> 1, hp, y;
        lo0 += (int32_t)(((int64_t)f0 * bp0) >> 15);
        hp = x - lo0 - (int32_t)(((int64_t)v->q * bp0) >> 15);
        bp0 += (int32_t)(((int64_t)f0 * hp) >> 15);
        lo1 += (int32_t)(((int64_t)f1 * bp1) >> 15);
        hp = x - lo1 - (int32_t)(((int64_t)(v->q * 2) * bp1) >> 15);
        bp1 += (int32_t)(((int64_t)f1 * hp) >> 15);
        bp0 = sk_clampi(bp0, -3000000, 3000000);
        bp1 = sk_clampi(bp1, -3000000, 3000000);
        y = bp0 + (bp1 >> 1);
        g += dg;
        o[i] = (int32_t)((((int64_t)y * (g >> 8)) >> 15) * pk >> 13);
    }
    v->nz = nz;
    v->bp = bp0;
    v->lo = lo0;
    v->bp2 = bp1;
    v->lo2 = lo1;
}


/* CHOIR: three detuned saws through three formant resonators (the vowel changes with each note and glides), a breath of
 * noise, a slow vibrato that grows with the swell */
static void sk_run_choir(sk_voice_t *v, int32_t *o, uint32_t n)
{
    int32_t g, dg, vib, pk = v->peak, br = v->br, nz = v->nz, q1 = v->q, q2 = v->q + v->q / 3, q3 = v->q * 2;
    int32_t b1 = v->bp, l1 = v->lo, b2 = v->bp2, l2 = v->lo2, b3 = v->bp3, l3 = v->lo3, f1, f2, f3;
    uint32_t i, k, inc[3];
    sk_swell(v, n, &g, &dg);
    v->f1 += (v->tf[0] - v->f1) >> 5;
    v->f2 += (v->tf[1] - v->f2) >> 5;
    v->f3 += (v->tf[2] - v->f3) >> 5;
    f1 = v->f1;
    f2 = v->f2;
    f3 = v->f3;
    vib = SK_SIN[v->lph[0] >> 22];
    v->lph[0] += v->linc[0] * n;
    {
        int32_t depth = (SK_SIN[v->lph[1] >> 22] >> 1) + 16384;
        v->lph[1] += v->linc[1] * n;
        depth = (depth * (v->amp >> 8)) >> 15;
        vib = (vib * depth) >> 15;
        vib = (vib * (int32_t)sk_s.lfo[3]) >> 6;
    }
    for (k = 0; k < 3u; k++) {
        sk_glide(v, k, 9);
        inc[k] = v->inc[k] + (uint32_t)(((int32_t)(v->inc[k] >> 14) * vib) >> 9);
    }
    for (i = 0; i < n; i++) {
        int32_t x, hp, y;
        x = ((int32_t)(v->ph[0] >> 17) + (int32_t)(v->ph[1] >> 17) + (int32_t)(v->ph[2] >> 17)) / 3 - 16384;
        x += (sk_noise(&nz) * br) >> 10;
        v->ph[0] += inc[0];
        v->ph[1] += inc[1];
        v->ph[2] += inc[2];
        l1 += (int32_t)(((int64_t)f1 * b1) >> 15);
        hp = x - l1 - (int32_t)(((int64_t)q1 * b1) >> 15);
        b1 += (int32_t)(((int64_t)f1 * hp) >> 15);
        l2 += (int32_t)(((int64_t)f2 * b2) >> 15);
        hp = x - l2 - (int32_t)(((int64_t)q2 * b2) >> 15);
        b2 += (int32_t)(((int64_t)f2 * hp) >> 15);
        l3 += (int32_t)(((int64_t)f3 * b3) >> 15);
        hp = x - l3 - (int32_t)(((int64_t)q3 * b3) >> 15);
        b3 += (int32_t)(((int64_t)f3 * hp) >> 15);
        b1 = sk_clampi(b1, -3000000, 3000000);
        b2 = sk_clampi(b2, -3000000, 3000000);
        b3 = sk_clampi(b3, -3000000, 3000000);
        y = b1 + ((b2 * 5) >> 3) + (b3 >> 2);
        g += dg;
        o[i] = (int32_t)((((int64_t)y * (g >> 8)) >> 15) * pk * 13 >> 19);
    }
    v->nz = nz;
    v->bp = b1;
    v->lo = l1;
    v->bp2 = b2;
    v->lo2 = l2;
    v->bp3 = b3;
    v->lo3 = l3;
}

/* BRASS: detuned saws through two low-passes that open with the swell: a horn that swells in and brightens as it rises */
static void sk_run_brass(sk_voice_t *v, int32_t *o, uint32_t n)
{
    int32_t g, dg, g1, vib, cut, lp = v->lp, lo = v->lo, pk = v->peak;
    uint32_t i, k, inc[3];
    g1 = sk_swell(v, n, &g, &dg);
    vib = SK_SIN[v->lph[0] >> 22];
    v->lph[0] += v->linc[0] * n;
    {
        int32_t breathe = SK_LD(SK_SIN[v->lph[1] >> 22]);
        v->lph[1] += v->linc[1] * n;
        cut = 700 + (int32_t)(((int64_t)(v->lpk - 700) * g1) >> 15);
        cut += (int32_t)(((int64_t)(cut >> 3) * breathe) >> 15);
        cut = sk_clampi(cut, 300, 24000);
        vib = (vib * ((g1 * (int32_t)sk_s.lfo[3]) >> 6)) >> 15;
    }
    for (k = 0; k < 3u; k++) {
        sk_glide(v, k, 9);
        inc[k] = v->inc[k] + (uint32_t)(((int32_t)(v->inc[k] >> 14) * vib) >> 10);
    }
    for (i = 0; i < n; i++) {
        int32_t x = ((int32_t)(v->ph[0] >> 17) + (int32_t)(v->ph[1] >> 17) + (int32_t)(v->ph[2] >> 17)) / 3 - 16384;
        v->ph[0] += inc[0];
        v->ph[1] += inc[1];
        v->ph[2] += inc[2];
        lp += (int32_t)(((int64_t)(x - lp) * cut) >> 15);
        lo += (int32_t)(((int64_t)(lp - lo) * cut) >> 15);
        g += dg;
        o[i] = (int32_t)((((int64_t)lo * (g >> 8)) >> 15) * pk >> 13);
    }
    v->lp = lp;
    v->lo = lo;
}

/* ORGAN: drawbar sines (16', 8', 4', 2 2/3', 2'); COLOUR pulls the upper bars out; a slow shimmer */
static void sk_run_organ(sk_voice_t *v, int32_t *o, uint32_t n)
{
    int32_t g, dg, a, pk = v->peak, w[5], h = v->idx;
    uint32_t i, k, inc[5];
    sk_swell(v, n, &g, &dg);
    a = SK_LD(SK_SIN[v->lph[0] >> 22]);
    v->lph[0] += v->linc[0] * n;
    w[0] = 21000;
    w[1] = 9000 + h * 60;
    w[2] = 3000 + h * 110;
    w[3] = 1500 + h * 120;
    w[4] = 14000;
    for (k = 0; k < 5u; k++) {
        sk_glide(v, k, 10);
        inc[k] = v->inc[k];
    }
    for (i = 0; i < n; i++) {
        int32_t s = 0;
        for (k = 0; k < 5u; k++) {
            s += (SK_SIN[v->ph[k] >> 22] * w[k]) >> 15;
            v->ph[k] += inc[k];
        }
        g += dg;
        s = (s * (32768 + (a >> 4))) >> 15;
        o[i] = (int32_t)((((int64_t)s * (g >> 8)) >> 15) * pk >> 15);
    }
}

/* BELL and FORGE: five inharmonic partials, the upper ones dying faster, and for the forge a click of the blow */
static void sk_run_bell(sk_voice_t *v, int32_t *o, uint32_t n)
{
    int32_t g, dg, pk = v->peak, br = v->br, nz = v->nz, cl = v->lo, idx = v->idx, top = v->lpk;
    uint32_t i;
    sk_swell(v, n, &g, &dg);
    for (i = 0; i < n; i++) {
        int32_t ga, gb, gc, s, click = 0;
        g += dg;
        ga = g >> 8;
        gb = (ga * ga) >> 15;
        gc = (gb * ga) >> 15;
        s = ((SK_SIN[v->ph[0] >> 22] * (ga >> 1)) >> 15) + ((SK_SIN[v->ph[1] >> 22] * ((ga * 3) >> 2)) >> 15) +
            ((SK_SIN[v->ph[2] >> 22] * (gb >> 1)) >> 15) + ((SK_SIN[v->ph[3] >> 22] * ((gc * top) >> 15) >> 1) >> 15) +
            ((SK_SIN[v->ph[4] >> 22] * ((gc * top) >> 15) >> 2) >> 15);
        v->ph[0] += v->inc[0];
        v->ph[1] += v->inc[1];
        v->ph[2] += v->inc[2];
        v->ph[3] += v->inc[3];
        v->ph[4] += v->inc[4];
        if (br && idx < 600) {
            int32_t x = (sk_noise(&nz) * ((600 - idx) * 27 >> 4)) >> 10;
            cl += (x - cl) >> 1;
            click = (cl * br) >> 7;
            idx++;
        }
        o[i] = (int32_t)(((int64_t)(s + click) * pk) >> 15);
    }
    v->nz = nz;
    v->lo = cl;
    v->idx = idx;
}

/* STOMP: a body that drops from a fifth above to its pitch, a sub-octave boom and a low thud of noise */
static void sk_run_stomp(sk_voice_t *v, int32_t *o, uint32_t n)
{
    int32_t g, dg, pk = v->peak, pd = v->pd, lp = v->lp, nz = v->nz, lpk = v->lpk;
    uint32_t i;
    sk_swell(v, n, &g, &dg);
    for (i = 0; i < n; i++) {
        int32_t ga, gb, s, th;
        uint32_t i0 = (v->tinc[0] >> 8) * (uint32_t)((65536 + pd) >> 8);
        g += dg;
        ga = g >> 8;
        gb = (ga * ga) >> 15;
        pd -= pd >> 11;
        lp += (int32_t)(((int64_t)(sk_noise(&nz) - lp) * lpk) >> 15);
        th = (lp * gb) >> 14;
        s = ((SK_SIN[v->ph[0] >> 22] * gb) >> 15) + ((SK_SIN[v->ph[1] >> 22] * ga) >> 16) + th;
        v->ph[0] += i0;
        v->ph[1] += v->tinc[1];
        o[i] = (int32_t)(((int64_t)s * pk) >> 15);
    }
    v->pd = pd;
    v->lp = lp;
    v->nz = nz;
}

/* DRIP: a sine whose pitch rises quickly, like a drop on water */
static void sk_run_drip(sk_voice_t *v, int32_t *o, uint32_t n)
{
    int32_t g, dg, pk = v->peak, pd = v->pd, top = v->lpk;
    uint32_t i;
    sk_swell(v, n, &g, &dg);
    for (i = 0; i < n; i++) {
        int32_t ga, gb, s;
        uint32_t inc = (v->tinc[0] >> 8) * (uint32_t)(pd >> 8);
        g += dg;
        ga = g >> 8;
        gb = (ga * ga) >> 15;
        pd += (top - pd) >> 11;
        s = SK_SIN[v->ph[0] >> 22] + (SK_SIN[(v->ph[0] << 1) >> 22] >> 3);
        v->ph[0] += inc;
        o[i] = (int32_t)((((int64_t)s * gb) >> 15) * pk >> 15);
    }
    v->pd = pd;
}

/* WHALE: a sine bent by its own phase (more as it swells), starting a fourth below and rising slowly */
static void sk_run_whale(sk_voice_t *v, int32_t *o, uint32_t n)
{
    int32_t g, dg, g1, vib, idx, pk = v->peak;
    uint32_t i, inc;
    g1 = sk_swell(v, n, &g, &dg);
    idx = (int32_t)(((int64_t)v->idx * (16384 + (g1 >> 1))) >> 15);
    vib = SK_SIN[v->lph[0] >> 22];
    v->lph[0] += v->linc[0] * n;
    {
        int32_t depth = (SK_SIN[v->lph[1] >> 22] >> 1) + 16384;
        v->lph[1] += v->linc[1] * n;
        vib = (((vib * depth) >> 15) * (int32_t)sk_s.lfo[3]) >> 6;
    }
    sk_glide(v, 0, 12);
    inc = v->inc[0] + (uint32_t)(((int32_t)(v->inc[0] >> 14) * vib) >> 9);
    for (i = 0; i < n; i++) {
        int32_t sf = SK_SIN[v->ph[0] >> 22], s;
        s = SK_SIN[(v->ph[0] + (uint32_t)(sf * idx)) >> 22];
        v->ph[0] += inc;
        g += dg;
        o[i] = (int32_t)((((int64_t)s * (g >> 8)) >> 15) * pk >> 15);
    }
}

/* GLASS: pairs of near-identical sines that beat slowly, a bowl or a glass harmonica */
static void sk_run_glass(sk_voice_t *v, int32_t *o, uint32_t n)
{
    int32_t g, dg, a, pk = v->peak, w[5], h = v->idx;
    uint32_t i, k, inc[5];
    sk_swell(v, n, &g, &dg);
    a = SK_LD(SK_SIN[v->lph[0] >> 22]);
    v->lph[0] += v->linc[0] * n;
    w[0] = 15000;
    w[1] = 15000;
    w[2] = 2500 + h * 55;
    w[3] = 2500 + h * 55;
    w[4] = h * 40;
    for (k = 0; k < 5u; k++) {
        sk_glide(v, k, 10);
        inc[k] = v->inc[k];
    }
    for (i = 0; i < n; i++) {
        int32_t s = 0;
        for (k = 0; k < 5u; k++) {
            s += (SK_SIN[v->ph[k] >> 22] * w[k]) >> 15;
            v->ph[k] += inc[k];
        }
        g += dg;
        s = (s * (32768 + (a >> 3))) >> 15;
        o[i] = (int32_t)((((int64_t)s * (g >> 8)) >> 15) * pk >> 15);
    }
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
        case VT_WHISTLE: sk_run_whistle(v, o, n); break;
        case VT_SUB: sk_run_sub(v, o, n); break;
        case VT_PAD: sk_run_pad(v, o, n); break;
        case VT_AIR: sk_run_air(v, o, n); break;
        case VT_CHOIR: sk_run_choir(v, o, n); break;
        case VT_BRASS: sk_run_brass(v, o, n); break;
        case VT_ORGAN: sk_run_organ(v, o, n); break;
        case VT_BELL:
        case VT_FORGE: sk_run_bell(v, o, n); break;
        case VT_STOMP: sk_run_stomp(v, o, n); break;
        case VT_DRIP: sk_run_drip(v, o, n); break;
        case VT_WHALE: sk_run_whale(v, o, n); break;
        case VT_GLASS: sk_run_glass(v, o, n); break;
        default: sk_run_drone(v, o, n); break;
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
        if (!v->act) {
            v->lp = 0;
            v->amp = 0;
        }
    }
}
