/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* SKYLYNNE's home scene: a lone figure sits on the edge of a cliff and looks out over the water under a dusk or night sky.
 *
 * Each of the six voices is a part of what they see, and a strike lights its part:
 *   1  aurora curtains          4  mist drifting over the water
 *   2  stars and the moon       5  the swell, and the moon's path on it
 *   3  shooting stars           6  lights on the far shore
 * The sky is drawn above the horizon, then mirrored (darker, rippled) into the water below it, then the foreground goes
 * over that: the cliff, bushes, a tree. Every half minute the figure stands, brushes down and sits again; now and then
 * a cigarette. Turning a voice's knob makes them point at what you are changing. Fireflies drift over everything.
 *
 * The picture ends at SC_H; the knob panel (sk_ui.c) takes the rest. */

#define SC_H 188                         /* the scene's height */
#define SC_HZ 104                        /* the horizon */
#define SC_PX 98                         /* where the figure sits (the hip) */
#define SC_PY 138                        /* the cliff's top there */
#define SC_CX0 128                       /* where the city starts */
#define SC_EDGE 106                      /* the cliff's edge */

static const uint8_t SC_COL[SK_NV] = {C_PINK, C_CYAN, C_YELLOW, C_EMBER, C_GREEN, C_ACC};     /* the voices' colours */
static const int16_t SC_AIM[SK_NV][2] = {{70, 30}, {150, 38}, {196, 50}, {170, 100}, {60, 90}, {150, 150}};
static const uint8_t SC_THEME[8] = {1, 0, 0, 2, 0, 2, 2, 1};      /* a city biome's sky: 0 slate night, 1 dusk haze, 2 mist */
static const uint8_t SC_PAL[16] = {0, 0, 0, 0, 0, 0, 0, 0, 3, 5, 6, 7, 4, 8, 9, 10};   /* a biome's palette (sk_fb.c); 0..7 as SC_THEME */
static const uint8_t SC_VAR[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 0, 1, 2, 3};     /* its look within its world */
static uint8_t sc_var;
static uint8_t sc_world;                 /* the biome's world: 0 the city, 1 the cathedral (palette 3), 2 the deep (palette 4) */

enum { SV_SIT, SV_POINT, SV_TIDY, SV_SMOKE };

static struct {
    uint8_t init, st, aim;
    uint8_t cnt[SK_NV];
    uint32_t hit[SK_NV];                 /* when each voice last struck */
    uint32_t t0, next, touch_t;
    uint8_t refl[FB_PAL];
} sc;
static uint32_t sc_touch_n;              /* the UI bumps this (and sets sc_touch_v) when a voice's knob is turned */
static uint8_t sc_touch_v;
static int sc_e[SK_NV];                  /* what each voice shows now, 0..255 */

static uint32_t sc_hash(uint32_t x)
{
    x ^= x >> 16;
    x *= 2246822519u;
    x ^= x >> 13;
    x *= 3266489917u;
    x ^= x >> 16;
    return x;
}
static int sc_tri(int x)                 /* a triangle wave, 0..255 over 512 */
{
    x &= 511;
    return x < 256 ? x : 511 - x;
}
static int sc_isqrt(int v)
{
    int r = 0, b = 1 << 14;
    while (b > v)
        b >>= 2;
    for (; b; b >>= 2) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
    }
    return r;
}

/* the intro: at power-on the title hangs in the night sky, the camera pans down to the cliff, the figure walks in from the
 * left smoking, flicks the cigarette out over the water and sits. Times in ms from the start. */
#define SI_PAN0 3400
#define SI_PAN1 6400
#define SI_WALK0 5600
#define SI_WALK1 9800
#define SI_FLICK 10050                   /* the cigarette leaves his hand */
#define SI_HALF 10350                    /* he starts to sit */
#define SI_SEAT 10650
#define SI_END 11600
static struct {
    uint8_t on, started;
    uint32_t t0;
} sc_in;

static void sc_theme(uint32_t biome)
{
    uint32_t i;
    sc_world = SK_BIOMES[biome % SK_NBIOME].world;
    sc_var = SC_VAR[biome & 15u];
    fb_theme(sc_world ? SC_PAL[biome & 15u] : SC_THEME[biome & 7u]);
    for (i = 0; i < FB_PAL; i++)
        sc.refl[i] = (uint8_t)i;
    for (i = 0; i < 6; i++)
        sc.refl[C_S0 + i] = (uint8_t)(C_W5 - i);
    sc.refl[C_MOON] = C_MOON2;
    sc.refl[C_STAR] = C_STAR2;
    sc.refl[C_F1] = C_F0;
    sc.refl[C_F2] = C_F0;
    sc.refl[C_F3] = C_F0;
    sc.refl[C_MIST] = C_W0;
    sc.refl[C_G0] = C_G1;
}

static void sc_update(uint32_t now)
{
    int v;
    if (!sc.init) {
        sc.init = 1;
        sc.next = now + 30000u;
        for (v = 0; v < SK_NV; v++) {
            sc.hit[v] = now - 100000u;
            sc.cnt[v] = sk_ui.cnt[v];
        }
    }
    for (v = 0; v < SK_NV; v++) {
        uint32_t age, dur;
        int e, base;
        if (sk_ui.cnt[v] != sc.cnt[v]) {
            sc.cnt[v] = sk_ui.cnt[v];
            sc.hit[v] = now;
        }
        age = now - sc.hit[v];
        dur = 3000u + (uint32_t)(sk_s.sus[v] + 6) * 700u;
        e = age >= dur ? 0 : 255 - (int)(age * 255u / dur);
        base = (sk_s.mute >> v) & 1 ? 0 : sk_s.level[v] * 40 / 100;
        sc_e[v] = base + e * (255 - base) / 255;
    }
    {
        static uint32_t seen;
        if (seen != sc_touch_n) {
            seen = sc_touch_n;
            sc.touch_t = now;
            sc.aim = sc_touch_v % SK_NV;
            if (sc.st == SV_SIT || sc.st == SV_SMOKE || sc.st == SV_POINT) {
                sc.st = SV_POINT;
                sc.t0 = now;
            }
        }
    }
    if (sc_in.on)
        sc.next = now + 60000u;                         /* (no tidying up during the intro) */
    switch (sc.st) {
    case SV_POINT:
        if (now - sc.touch_t > 1800u) {
            sc.st = SV_SIT;
            sc.t0 = now;
        }
        break;
    case SV_TIDY:
        if (now - sc.t0 > 3350u) {
            sc.st = SV_SIT;
            sc.next = now + 26000u + sc_hash(now) % 8000u;
        }
        break;
    case SV_SMOKE:
        if (now - sc.t0 > 8500u) {
            sc.st = SV_SIT;
            sc.next = now + 26000u + sc_hash(now) % 8000u;
        }
        break;
    default:
        if ((int32_t)(now - sc.next) >= 0) {
            sc.st = (sc_hash(now) & 3u) == 0u ? SV_SMOKE : SV_TIDY;
            sc.t0 = now;
        }
        break;
    }
}

/* ------------------------------------------------------------------ sky --- */
static void sc_sky(void)
{
    static const int EDGE[6] = {14, 32, 52, 70, 88, SC_HZ};
    static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    int i, y = 0, x;
    for (i = 0; i < 6; i++) {
        for (; y < EDGE[i]; y++) {
            int zone = EDGE[i] - y;                              /* rows to the next band: blended over the last 14 */
            uint8_t c = (uint8_t)(C_S0 + i), c1 = (uint8_t)(C_S0 + (i < 5 ? i + 1 : i));
            if (i < 5 && zone <= 14) {
                int frac = 16 - zone * 16 / 15;                  /* 1 .. 16 of 16 */
                const uint8_t *bt = BAYER + (y & 3) * 4;
                for (x = 0; x < FB_W; x++)
                    fb[y * FB_W + x] = bt[x & 3] < frac ? c1 : c;
            } else
                fb_rect(0, y, FB_W, 1, c);
        }
    }
}

static void sc_stars(uint32_t now)
{
    int i, e = sc_e[1];
    for (i = 0; i < 34; i++) {
        uint32_t k = sc_hash((uint32_t)i * 2654435761u + 0x51u);
        int x = (int)(k % 236u) + 2, y = 3 + (int)((k >> 9) % 78u);
        uint8_t c = ((k >> 20) & 3u) ? C_STAR2 : C_STAR;
        if (((now >> 7) + (k >> 12)) % 29u == 0u)
            c = C_STAR;
        fb_pset(x, y, c);
        if (e > 120 && i * 127 / 34 < sk_s.mac[0] / 2) {          /* a faint twinkle: a small cross */
            int s = 1, d;
            fb_pset(x, y, C_STAR);
            for (d = 1; d <= s; d++) {
                uint8_t cc = C_STAR2;
                fb_pset(x - d, y, cc);
                fb_pset(x + d, y, cc);
                fb_pset(x, y - d, cc);
                fb_pset(x, y + d, cc);
            }
        }
    }
}

static void sc_moon(void)
{
    int e = sc_e[1], cx = 172, cy = 46, R = 10, gl = 4 + e * 6 / 255, i, j, m = R + gl + 1;
    for (j = -m; j <= m; j++)
        for (i = -m; i <= m; i++) {
            int d2 = i * i + j * j;
            if (d2 <= R * R)
                fb_pset(cx + i, cy + j, d2 > (R - 1) * (R - 1) ? C_MOON2 : C_MOON);
            else if (d2 <= (R + gl) * (R + gl) && ((i + j) & 1) == 0 && d2 < (R + gl / 2) * (R + gl / 2))
                fb_pset(cx + i, cy + j, C_S5);                    /* a faint halo, dithered */
        }
}

static void sc_aurora(uint32_t now)
{
    int x, e = sc_e[0];
    if (e < 40)
        return;
    for (x = 4; x < 170; x += 2) {
        int top = 8 + (sc_tri(x * 2 + (int)(now >> 6)) >> 4);
        int len = 8 + (e * 26 >> 8) + (sc_tri(x * 3 - (int)(now >> 7)) >> 4), k;
        for (k = 0; k < len; k++)
            if (((k + (x >> 1)) & 3) == 0)
                fb_pset(x, top + k, k < len / 2 ? C_S5 : C_S4);
    }
}

static void sc_comet(uint32_t now)
{
    uint32_t age = now - sc.hit[2];
    int p, hx, hy, t, n;
    if (age > 1000u)
        return;
    p = (int)(age * 256u / 1000u);
    hx = 232 - p * 150 / 256;
    hy = 10 + p * 70 / 256;
    n = 8 + sk_s.mac[0] / 12;
    for (t = 0; t < n; t++) {
        if (t > 6 && (t & 1))
            continue;
        fb_pset(hx + t * 2, hy - t, t < 2 ? C_STAR : C_STAR2);
    }
}

static void sc_mist(uint32_t now)
{
    int e = sc_e[4], y, band;
    if (e < 20)
        return;
    for (band = 0; band < 3; band++) {
        int y0 = SC_HZ - 22 + band * 7, h = 3 + e / 60, off = (int)(now >> (6 - band));
        for (y = 0; y < h; y++) {
            int x, ph = (off + band * 90) & 63;
            for (x = (y + band) & 1; x < FB_W; x += 2)
                if (sc_tri(x * 2 + ph * 3 + band * 70) > 150)
                    fb_pset(x, y0 + y, C_MIST);
        }
    }
}

/* the far shore: a low ridge, the city or lighthouse lights */
static void sc_shore(void)
{
    int x;
    for (x = 0; x < SC_CX0; x++) {
        int h = 3 + (int)(sc_hash((uint32_t)(x >> 3) + 0x77u) % 4u) + 3;
        fb_rect(x, SC_HZ - h, 1, h, C_F1);
        if ((x & 3) == 0)
            fb_pset(x, SC_HZ - h, C_F2);
    }
}

/* the city on the far shore, in three depths: a pale haze of far towers, the near skyline (stepped, spired, pitched and
 * slanted roofs; one-pixel windows in rows; lit floors; neon edges, signs, rooftop tanks and masts; a beacon), and a
 * quay of lamps along the water. Voice 4 lights more windows; DENSITY adds a few. Over it a faint glow. This is the only
 * vivid part of the picture. */
static int8_t sc_hmap[FB_W];

static void sc_city_far(void)
{
    int x = SC_CX0 - 14, b = 0;
    while (x < FB_W) {
        uint32_t k = sc_hash((uint32_t)b * 4177u + 0x91u);
        int w = 3 + (int)(k % 5u), h = 10 + (int)((k >> 6) % 26u), i;
        if (x + w > FB_W)
            w = FB_W - x;
        for (i = 0; i < w; i++) {
            int hh = h - ((k >> 14) % 3u == 0u && i == w - 1 ? 3 : 0);          /* a notch now and then */
            fb_rect(x + i, SC_HZ - hh, 1, hh, C_F2);
            if (hh > sc_hmap[x + i])
                sc_hmap[x + i] = (int8_t)hh;
        }
        if ((k >> 18) % 4u == 0u)
            fb_vline(x + w / 2, SC_HZ - h - 4, 4, C_F2);                         /* a far mast */
        {
            int wy;
            for (wy = SC_HZ - h + 3; wy < SC_HZ - 3; wy += 3) {
                uint32_t q = sc_hash((uint32_t)(x * 97 + wy * 13) + 0x3u);
                if (q % 5u == 0u)
                    fb_pset(x + 1 + (int)((q >> 8) % (uint32_t)(w > 2 ? w - 1 : 1)), wy, C_G0);          /* a window in the haze */
            }
        }
        x += w - ((k >> 24) % 2u);
        b++;
    }
}

/* one near building at x, width w, height h; k its random word; col its main neon */
static void sc_tower(int x, int w, int h, uint32_t k, uint8_t col, int lit, uint32_t now)
{
    int top = SC_HZ - h, type = (int)((k >> 4) % 6u), i, wy, wx, roof = top;
    uint8_t col2 = (uint8_t)(C_N0 + ((k >> 9) & 3u));
    /* the body, with a lit left edge and a dark right one */
    fb_rect(x, top, w, h, C_F1);
    fb_vline(x, top, h, C_F2);
    fb_vline(x + w - 1, top, h, C_F0);
    switch (type) {
    case 1:                                                       /* stepped: a narrower upper tier */
        fb_rect(x + 2, top - 5, w - 4, 5, C_F1);
        fb_vline(x + 2, top - 5, 5, C_F2);
        fb_hline(x + 2, top - 5, w - 4, C_F2);
        roof = top - 5;
        break;
    case 2:                                                       /* pitched: a little gable */
        for (i = 0; i < w / 2; i++)
            fb_hline(x + i, top - 1 - i, w - 2 * i, i ? C_F1 : C_F2);
        roof = top - w / 2;
        break;
    case 3:                                                       /* an art-deco spire */
        fb_rect(x + w / 2 - 1, top - 6, 3, 6, C_F1);
        fb_rect(x + w / 2, top - 11, 1, 5, C_F1);
        fb_pset(x + w / 2, top - 12, C_F2);
        roof = top - 12;
        break;
    case 4:                                                       /* a water tank on legs, and a vent */
        fb_rect(x + 1, top - 3, 4, 3, C_F1);
        fb_pset(x + 1, top - 4, C_F2);
        fb_pset(x + 4, top - 4, C_F2);
        fb_pset(x + w - 3, top - 1, C_F2);
        roof = top - 4;
        break;
    case 5:                                                       /* a slanted roof */
        for (i = 0; i < w; i++)
            fb_vline(x + i, top - 1 - (w - 1 - i) / 3, 1 + (w - 1 - i) / 3, C_F1);
        roof = top - 1 - (w - 1) / 3;
        break;
    default:
        fb_hline(x, top, w, C_F2);                                 /* a flat roof with its parapet */
        break;
    }
    for (i = 0; i < w; i++)
        if (SC_HZ - roof > sc_hmap[x + i])
            sc_hmap[x + i] = (int8_t)(SC_HZ - roof);
    /* windows: one pixel, in rows of two columns' pitch; a few floors all lit, some dark panes in a faint tone */
    for (wy = top + 3; wy < SC_HZ - 2; wy += 3) {
        int floor_on = (int)(sc_hash((uint32_t)(wy * 31) + (k & 0xFFFFu)) % 9u) == 0;
        for (wx = x + 2; wx < x + w - 1; wx += 2) {
            uint32_t q = sc_hash((uint32_t)(wx * 131 + wy * 17) + 0x77u);
            int on = floor_on || (int)(q % 100u) < lit;
            if (!on && ((now >> 10) + (q >> 8)) % 113u == 0u)
                on = 1;                                           /* a window now and then */
            if (on)
                fb_pset(wx, wy, ((q >> 14) & 7u) == 0u ? col2 : col);
            else if ((q >> 20) % 3u == 0u)
                fb_pset(wx, wy, C_F2);                            /* a dark pane */
        }
    }
    if ((k >> 12) % 4u == 0u && h > 12) {                         /* a vertical neon edge */
        fb_vline(x + w - 1, top + 2, h - 4, col);
        if (x + w < FB_W)
            for (wy = top + 2; wy < SC_HZ - 2; wy += 2)
                if (fb[wy * FB_W + x + w] >= C_S0 && fb[wy * FB_W + x + w] <= C_S5)
                    fb_pset(x + w, wy, C_G0);
    }
    if ((k >> 16) % 3u == 0u && w >= 7) {                         /* a sign: a bar in dashes and a small framed board */
        int sy = top + 1;
        for (i = 1; i < w - 1; i++)
            if (((i + (int)(k >> 28)) & 3) != 3)
                fb_pset(x + i, sy, col2);
        if (h > 16) {
            fb_hline(x + 1, sy + 3, w - 3, col);
            fb_hline(x + 1, sy + 6, w - 3, col);
            fb_vline(x + 1, sy + 3, 4, col);
            fb_vline(x + w - 3, sy + 3, 4, col);
        }
    }
    if ((k >> 20) % 5u == 0u) {                                   /* a mast with a blinking light */
        int mx = x + w / 2;
        fb_vline(mx, roof - 6, 6, C_F1);
        if (((now >> 9) + (uint32_t)(k & 7u)) % 4u < 2u)
            fb_pset(mx, roof - 7, C_N0);
    }
}

static void sc_city(uint32_t now)
{
    int x = SC_CX0, e = sc_e[3], lit = 20 + (e * 30 >> 8) + (sk_s.mac[0] >> 4), b = 0, i;
    for (i = 0; i < FB_W; i++)
        sc_hmap[i] = 0;
    sc_city_far();
    while (x < FB_W - 2) {
        uint32_t k = sc_hash((uint32_t)b * 7919u + 0x31u);
        int w = 6 + (int)(k % 9u), h = 8 + (int)((k >> 8) % 20u) + (x > 170 && x < 205 ? 7 : 0);
        if (x + w > FB_W - 1)
            w = FB_W - 1 - x;
        if (w >= 4)
            sc_tower(x, w, h, k, (uint8_t)(C_N0 + ((k >> 24) & 3u)), lit, now);
        x += w + (int)((k >> 16) % 2u);
        b++;
    }
    /* the quay: a low wall with a row of lamps, and a few lit boats' lights */
    fb_hline(SC_CX0 - 14, SC_HZ - 1, FB_W - SC_CX0 + 14, C_F0);
    for (x = SC_CX0 - 12; x < FB_W - 1; x += 4)
        if (((x >> 2) + (int)(now >> 11)) % 11u != 0u)
            fb_pset(x, SC_HZ - 2, (x >> 2) % 3u == 0u ? C_N3 : ((x >> 2) % 3u == 1u ? C_N0 : C_N1));
    /* the glow: a faint dithered haze over the roofs, thicker over the tall ones, breathing very slowly */
    {
        int breathe = 2 + (sc_tri((int)(now >> 6)) >> 6);
        for (x = SC_CX0 - 14; x < FB_W - 1; x++) {
            int h = sc_hmap[x], g = 6 + (h >> 2) + breathe + (e >> 6);
            for (i = 1; i <= g; i++) {
                int y = SC_HZ - h - i;
                if (y < 0)
                    break;
                if (((x + y) & 1) == 0 && (i < g / 2 || ((x >> 1) + y) % 3 == 0))
                    if (fb[y * FB_W + x] >= C_S0 && fb[y * FB_W + x] <= C_S5)
                        fb_pset(x, y, C_G0);
            }
        }
    }
}

static void sc_arm(int sx, int sy, int tx, int ty, uint8_t c)
{
    fb_line(sx, sy, tx, ty, c);
    fb_line(sx + 1, sy, tx + 1, ty, c);
}

/* a ember-lit cigarette's smoke rising from (ex, ey) */
static void sc_smoke(int ex, int ey, uint32_t now, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        int ph = (int)((now / 90u + (uint32_t)i * 13u) % 40u);
        int x = ex - ph / 3 + ((sc_tri(ph * 9 + i * 50) - 128) >> 5), y = ey - ph;
        if (ph < 34) {
            fb_pset(x, y, ph < 20 ? C_MIST : C_F3);
            if (ph > 10)
                fb_pset(x + 1, y, C_F3);
        }
    }
}

static int sc_intro_fig(uint32_t now);
static void sc_figure(uint32_t now)
{
    int px = SC_PX, py = SC_PY, age = (int)(now - sc.t0), i;
    int cx = px - 7, fl = sc_tri((int)(now >> 4)) >> 7;       /* the scarf's flutter, 0..1 */
    if (sc_in.on && sc_intro_fig(now))
        return;
    if (sc.st == SV_TIDY && age >= 250 && age < 3050) {      /* standing */
        int sw = age >= 650 && age < 2450;
        fb_rect(cx - 3, py - 10, 3, 10, C_F0);
        fb_rect(cx + 1, py - 10, 3, 10, C_F0);
        fb_rect(cx - 3, py - 22, 7, 12, C_F0);
        fb_rect(cx - 3, py - 28, 6, 6, C_F0);
        fb_vline(cx + 3, py - 21, 10, C_F3);
        fb_vline(cx + 2, py - 27, 4, C_F3);
        fb_pset(cx + 1, py - 25, C_MOON);
        fb_hline(cx - 3, py - 22, 7, C_SCARF);
        for (i = 1; i < 5; i++)
            fb_pset(cx - 3 - i, py - 21 + fl + (i >> 1), C_SCARF);
        if (sw) {
            int t = (age - 650) % 600, hy = py - 19 + t * 13 / 600;
            sc_arm(cx + 1, py - 20, cx + 4, hy, C_F1);
            for (i = 0; i < 5; i++) {
                int ph = (age / 6 + i * 17) % 30;
                fb_pset(cx + 5 + (i & 1) * 2, py - 14 + ph, ph < 20 ? C_MIST : C_F3);
            }
        } else
            sc_arm(cx + 1, py - 20, cx + 2, py - 9, C_F1);
        return;
    }
    if (sc.st == SV_TIDY && ((age >= 0 && age < 250) || (age >= 3050 && age < 3350))) {   /* half up */
        fb_rect(cx - 3, py - 7, 7, 7, C_F0);
        fb_rect(cx - 3, py - 19, 7, 12, C_F0);
        fb_rect(cx - 3, py - 25, 6, 6, C_F0);
        fb_vline(cx + 3, py - 18, 10, C_F3);
        fb_vline(cx + 2, py - 24, 4, C_F3);
        fb_pset(cx + 1, py - 22, C_MOON);
        fb_hline(cx - 3, py - 19, 7, C_SCARF);
        return;
    }
    /* sitting */
    fb_rect(px - 2, py - 3, 10, 3, C_F0);                         /* the thigh */
    fb_hline(px - 1, py - 3, 8, C_F3);
    fb_rect(px + 6, py - 1, 3, 9, C_F0);                          /* the leg over the edge */
    fb_rect(px + 6, py + 7, 5, 2, C_F0);
    fb_pset(px + 10, py + 7, C_F3);
    fb_rect(px - 4, py - 14, 6, 12, C_F0);                        /* the back and chest */
    fb_vline(px + 1, py - 13, 10, C_F3);
    fb_rect(px - 3, py - 20, 6, 6, C_F0);                         /* the head */
    fb_vline(px + 2, py - 19, 4, C_F3);
    fb_hline(px - 2, py - 20, 4, C_F2);
    {
        int up = sc.st == SV_POINT && SC_AIM[sc.aim][1] < 80;
        fb_pset(px + 1, py - (up ? 18 : 17), C_MOON);               /* the eye: he looks up for the high things */
    }
    fb_hline(px - 3, py - 14, 6, C_SCARF);                           /* the scarf */
    for (i = 1; i < 6; i++)
        fb_pset(px - 4 - i, py - 13 + fl + (i >> 1), C_SCARF);
    if (sc.st == SV_POINT) {
        int dx = SC_AIM[sc.aim][0] - (px + 1), dy = SC_AIM[sc.aim][1] - (py - 11);
        int m = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
        int ux = dx * 12 / m, uy = dy * 12 / m, bob = sc_tri(age * 2) >> 7;
        sc_arm(px, py - 11, px + ux, py - 11 + uy - bob, C_F1);
        fb_rect(px + ux, py - 11 + uy - bob - 1, 2, 2, C_MOON);
    } else if (sc.st == SV_SMOKE) {
        int ex, ey;
        if (age < 600 || (age >= 600 && age < 1900)) {          /* the hand at the mouth */
            int ember = age < 300 ? 0 : (age < 600 ? 1 : 2);
            sc_arm(px, py - 11, px + 3, py - 15, C_F1);
            fb_rect(px + 3, py - 17, 2, 2, C_MOON2);
            fb_hline(px + 4, py - 16, 3, C_MOON);
            ex = px + 7;
            ey = py - 16;
            if (age < 300 && (age / 60) & 1)
                fb_rect(ex - 1, ey - 2, 2, 2, C_EMBER);              /* the lighter's flame */
            if (ember >= 1)
                fb_pset(ex, ey, C_EMBER);
            if (ember == 2 && age > 700) {
                fb_checker(ex - 2, ey - 2, 5, 5, C_EMBER);
                fb_pset(ex, ey, C_EMBER);
            }
            sc_smoke(ex, ey - 1, now, age > 600 ? 4 : 0);
        } else {                                                /* the hand rests, the cigarette still burning */
            sc_arm(px, py - 11, px + 5, py - 5, C_F1);
            ex = px + 8;
            ey = py - 6;
            fb_hline(px + 6, py - 6, 2, C_MOON);
            fb_pset(ex, ey, ((now >> 8) & 7u) < 5u ? C_EMBER : C_F3);
            sc_smoke(ex, ey - 1, now, age < 2500 ? 7 : 3);
        }
    } else
        sc_arm(px - 1, py - 11, px + 4, py - 4, C_F1);          /* the arm on the knee */
}


/* pops of colour: a soft glow, brighter with p (0..255) */
static void sc_glow(int x, int y, uint8_t c, int p)
{
    if (p < 40)
        return;
    fb_pset(x, y, c);
    if (p > 110) {
        fb_pset(x + 1, y, c);
        fb_pset(x, y + 1, c);
    }
    if (p > 200) {
        fb_pset(x - 1, y, c);
        fb_pset(x, y - 1, c);
        fb_pset(x + 2, y + 1, c);
        fb_pset(x + 1, y + 2, c);
    }
}

static void sc_fireflies(uint32_t now)
{
    static const uint8_t FC[8] = {C_EMBER, C_PINK, C_EMBER, C_EMBER, C_PINK, C_EMBER, C_YELLOW, C_EMBER};
    int i, v;
    for (i = 0; i < (sc_world ? 0 : 18); i++) {
        uint32_t k = sc_hash((uint32_t)i * 7919u + 0x2Bu);
        int bx = (int)(k % 236u) + 2, by = 80 + (int)((k >> 8) % 104u);
        int x = bx + ((sc_tri((int)(now >> 5) + (int)(k >> 16)) - 128) >> 3);
        int y = by + ((sc_tri((int)(now >> 6) + (int)(k >> 20) * 3) - 128) >> 4) - (int)((now >> 8) + (k >> 4)) % 14u;
        int p = sc_tri((int)(now >> 2) + (int)(k >> 9));
        if (x > 82 && x < 112 && y > 110 && y < 150)
            continue;                                       /* (not over the figure) */
        sc_glow(x, y, FC[i & 7], p);
    }
    for (v = 0; v < SK_NV; v++) {                           /* embers of a voice's colour where it struck */
        uint32_t age = now - sc.hit[v];
        int k;
        if (age > 1600u)
            continue;
        for (k = 0; k < 4; k++) {
            uint32_t h = sc_hash((uint32_t)(v * 31 + k) * 2654435761u + sc.cnt[v]);
            int x = SC_AIM[v][0] + (int)(h % 25u) - 12 + ((sc_tri((int)(age >> 2) + k * 90) - 128) >> 4);
            int y = SC_AIM[v][1] - (int)(age >> 4) - k * 4;
            if (y > 0 && y < SC_H)
                sc_glow(x, y, SC_COL[v], 255 - (int)(age * 255u / 1600u));
        }
    }
}

/* the sky mirrored into the water: darker, rippled, wider ripples with depth */
static void sc_reflect(uint32_t now)
{
    int y;
    for (y = SC_HZ; y < SC_H; y++) {
        int d = y - SC_HZ, sy = SC_HZ - 1 - d, dx, x;
        uint8_t *dst = fb + y * FB_W;
        const uint8_t *src;
        if (sy < 0) {
            fb_rect(0, y, FB_W, SC_H - y, C_W5);
            break;
        }
        src = fb + sy * FB_W;
        dx = ((sc_tri(d * 13 + (int)(now >> 4)) - 128) * (1 + d / 24)) >> 7;
        for (x = 0; x < FB_W; x++) {
            int sx = x + dx;
            uint8_t c;
            sx = sx < 0 ? 0 : (sx >= FB_W ? FB_W - 1 : sx);
            c = sc.refl[src[sx] & (FB_PAL - 1)];
            if ((c == C_MOON2 || c == C_STAR2) && ((x + y) & 1))
                c = C_W2;
            else if (c >= C_N0 && c <= C_N3 && ((x + y) & 1))        /* the neon lies on the water, half as bright */
                c = C_G1;
            dst[x] = c;
        }
    }
}

static void sc_water(uint32_t now)
{
    int y, r, e5 = sc_e[5], e1 = sc_e[1];
    /* the swell: rows of dashes, wider near us, lifting with the sub */
    for (r = 0; r < 14; r++) {
        int py = SC_HZ + 3 + r * r / 3 + r * 2, len = 3 + r / 2, gap = 9 + r, per = len + gap, x;
        int off = (int)((now >> 6) * (uint32_t)(1 + r / 3)) % per;
        uint8_t c = e5 > 110 ? C_W0 : C_W1;
        if (py >= SC_H)
            break;
        for (x = -off + (r * 7) % per; x < FB_W; x += per) {
            int yo = (sc_tri(x * 2 + (int)(now >> 3)) * (e5 >> 5)) >> 9;
            fb_hline(x, py + yo, len, c);
        }
    }
    /* the moon's path, breaking up into glints */
    for (y = SC_HZ + 2; y < SC_H; y += 2) {
        int dp = y - SC_HZ, w = 2 + dp / 6 + (e1 >> 6);
        int wob = ((sc_tri((y << 3) + (int)(now >> 4)) - 128) * (3 + dp / 20)) >> 7;
        if (sc_hash((uint32_t)y * 13u + (now >> 8)) % 3u == 0u)
            fb_hline(172 + wob - w / 2, y, w, C_MOON2);
    }
    /* the far lights' streaks, ripples from the sub's strikes */
    {
        int e3 = sc_e[3];
        uint32_t age = now - sc.hit[5];
        int k, x;
        for (x = SC_CX0; x < FB_W - 1; x += 1) {                     /* the city's light lying on the water */
            uint32_t q = sc_hash((uint32_t)x * 31u + (now >> 9));
            int n = 3 + (int)(q % 9u) + (e3 >> 6), j;
            for (j = 0; j < n; j++)
                if (((x + j) & 1) == 0)
                    fb_pset(x, SC_HZ + 2 + j * 2, C_G1);
        }
        for (k = 0; k < 3; k++) {
            int a = (int)age - k * 350, rr, x;
            if (a <= 0 || a > 2600)
                continue;
            rr = a / 14;
            if (rr > 70)
                continue;
            for (x = -rr; x <= rr; x++) {
                int yy = 160 + sc_isqrt(rr * rr - x * x) / 4;
                int y2 = 160 - sc_isqrt(rr * rr - x * x) / 4;
                if ((x + rr) & 1)
                    continue;
                if (yy < SC_H) {
                    fb_pset(150 + x, yy, k == 0 ? C_W0 : C_W1);
                    fb_pset(150 + x, y2, k == 0 ? C_W0 : C_W1);
                }
            }
        }
    }
}

/* the foreground: the cliff the figure sits on, bushes, a tree on the far side of the bay */
static void sc_bush(int cx, int base, int w, int h, uint32_t seed)
{
    int x;
    for (x = -w; x <= w; x++) {
        int hh = h - (x * x * h) / (w * w + 1) + (int)(sc_hash(seed + (uint32_t)(x >> 1)) % 3u);
        if (hh > 0) {
            fb_rect(cx + x, base - hh, 1, hh, C_F0);
            if ((x & 3) == 0)
                fb_pset(cx + x, base - hh, C_F2);
        }
    }
}

static void sc_foreground(void)
{
    int x;
    for (x = 0; x < SC_EDGE; x++) {
        uint32_t k = sc_hash((uint32_t)(x >> 2) + 0x61u);
        int ty = SC_PY + (x < 30 ? (30 - x) / 3 : 0) + (int)(k % 3u);
        fb_rect(x, ty, 1, SC_H - ty, C_F0);
        fb_pset(x, ty, C_F2);
        if ((k >> 8) % 5u == 0u)
            fb_pset(x, ty - 1, C_F1);                    /* tufts of grass */
    }
    fb_rect(SC_EDGE, SC_PY + 1, 2, SC_H - SC_PY - 1, C_F2);       /* the rim of the face */
    fb_vline(SC_EDGE + 2, SC_PY + 6, 34, C_F1);
    fb_checker(40, SC_PY + 12, 66, 36, C_F1);                      /* the face in shadow */
    fb_checker(SC_EDGE - 14, SC_PY + 28, 14, 10, C_F2);            /* a few lit ledges */
    sc_bush(14, SC_PY + 8, 22, 14, 0x31u);
    sc_bush(42, SC_PY + 3, 12, 8, 0x41u);
    /* the far side of the bay, bottom right */
    sc_bush(186, SC_H, 30, 14, 0x51u);
    sc_bush(228, SC_H, 22, 22, 0x61u);
    fb_rect(214, 122, 3, SC_H - 122, C_F0);                       /* the tree: trunk, a branch, the crown in clumps */
    fb_line(215, 140, 200, 126, C_F0);
    fb_line(215, 134, 232, 120, C_F0);
    sc_bush(200, 128, 12, 9, 0x71u);
    sc_bush(232, 122, 10, 8, 0x72u);
    sc_bush(216, 116, 14, 10, 0x73u);
    fb_checker(204, 108, 24, 8, C_F1);
}

#include "sk_worlds.c"

static void sc_intro_proj(uint32_t now);
static void sc_draw(uint32_t now)
{
    static uint8_t fadet;
    if ((++fadet & 1u) == 0)
        fb_fade();
    fb_unclip();
    sc_sky();
    if (sc_world == 1) {                                  /* the cathedral */
        sc_sun();
        sc_rays(now, 172, 40, -30 - (sc_e[0] >> 4), 26, (sc_var == 2 ? 2 : 4) + (sc_e[0] >> 6), 40 + (sc_e[0] >> 2));
        sc_mist(now);
        sc_shore();
        sc_nave(now);
    } else if (sc_world == 2) {                           /* the deep */
        sc_surface(now);
        sc_rays(now, 180, 0, -10, 30, (sc_var == 2 ? 1 : sc_var == 3 ? 6 : 3) + (sc_e[0] >> 6), 70 + (sc_e[0] >> 2));
        sc_snow(now);
        sc_fish(now);
        sc_mist(now);
        sc_ruins(now);
    } else {
        sc_aurora(now);
        sc_stars(now);
        sc_moon();
        sc_comet(now);
        sc_mist(now);
        sc_shore();
        sc_city(now);
    }
    sc_reflect(now);
    sc_water(now);
    if (sc_world == 1)
        sc_terrace();
    else if (sc_world == 2)
        sc_ledge(now);
    else
        sc_foreground();
    sc_figure(now);
    if (sc_world == 1)
        sc_motes(now);
    else if (sc_world == 2)
        sc_bubbles(now);
    sc_fireflies(now);
    if (sc_in.on)
        sc_intro_proj(now);
}

/* --------------------------------------------------------------- the intro --- */
/* the figure while he is not yet sitting: 1 when this drew him (the sitting figure is then skipped) */
static void sc_stand(int cx, int py, int step, uint32_t now, int arm)
{
    int fl = sc_tri((int)(now >> 4)) >> 7, la = step & 1, i;
    fb_rect(cx - 3, py - 10 + la, 3, 10 - la, C_F0);                  /* the legs: one lifts, then the other */
    fb_rect(cx + 1, py - 10 + (1 - la), 3, 10 - (1 - la), C_F0);
    fb_rect(cx - 3, py - 22, 7, 12, C_F0);
    fb_rect(cx - 3, py - 28, 6, 6, C_F0);
    fb_vline(cx + 3, py - 21, 10, C_F3);
    fb_vline(cx + 2, py - 27, 4, C_F3);
    fb_pset(cx + 1, py - 25, C_MOON);
    fb_hline(cx - 3, py - 22, 7, C_SCARF);
    for (i = 1; i < 5; i++)
        fb_pset(cx - 3 - i, py - 21 + fl + (i >> 1), C_SCARF);
    (void)arm;
}

static int sc_intro_fig(uint32_t now)
{
    int t = (int)(now - sc_in.t0), cx, gy, step;
    if (t < SI_WALK0)
        return 1;                                                     /* not in view yet */
    if (t >= SI_SEAT)
        return 0;                                                     /* he sits: the usual figure */
    if (t >= SI_HALF) {                                               /* half up, about to sit */
        int px = SC_PX, py = SC_PY;
        fb_rect(px - 10, py - 7, 7, 7, C_F0);
        fb_rect(px - 10, py - 19, 7, 12, C_F0);
        fb_rect(px - 10, py - 25, 6, 6, C_F0);
        fb_vline(px - 4, py - 18, 10, C_F3);
        fb_vline(px - 5, py - 24, 4, C_F3);
        fb_pset(px - 6, py - 22, C_MOON);
        fb_hline(px - 10, py - 19, 7, C_SCARF);
        return 1;
    }
    cx = t >= SI_WALK1 ? SC_PX - 7 : -8 + (t - SI_WALK0) * (SC_PX - 7 + 8) / (SI_WALK1 - SI_WALK0);
    gy = SC_PY + (cx < 30 ? (30 - cx) / 3 : 0) + 1;
    step = t >= SI_WALK1 ? 0 : (t - SI_WALK0) / 170;
    sc_stand(cx, gy, step, now, 0);
    if (t < SI_FLICK) {
        int tw = t - SI_WALK0, puff = (t < SI_WALK1 - 400 && (tw % 2300) < 800) || (t >= SI_WALK1 - 400 && t < SI_WALK1 + 250);
        int ex, ey;
        if (t >= SI_WALK1 + 250) {                                    /* the arm swings out and lets go */
            sc_arm(cx + 1, gy - 20, cx + 9, gy - 22, C_F1);
            ex = cx + 10;
            ey = gy - 22;
            fb_pset(ex, ey, C_EMBER);
        } else if (puff) {                                            /* the hand at the mouth */
            sc_arm(cx + 1, gy - 20, cx + 3, gy - 23, C_F1);
            fb_hline(cx + 4, gy - 24, 3, C_MOON);
            ex = cx + 7;
            ey = gy - 24;
            fb_pset(ex, ey, C_EMBER);
            if (((tw / 200) & 1) == 0)
                fb_checker(ex - 2, ey - 2, 5, 5, C_EMBER);
        } else {                                                      /* the hand swings low, the end glowing */
            int sw = ((sc_tri(tw / 2) - 128) >> 5);
            sc_arm(cx + 1, gy - 20, cx + 3 + sw, gy - 10, C_F1);
            ex = cx + 4 + sw;
            ey = gy - 11;
            fb_pset(ex, ey, ((now >> 8) & 7u) < 5u ? C_EMBER : C_F3);
        }
        sc_smoke(ex, ey - 1, now, 5);
    } else
        sc_arm(cx + 1, gy - 20, cx + 3, gy - 12, C_F1);               /* the arm comes back down */
    return 1;
}

/* the cigarette flicked out over the water: an arc, an ember with a short tail, and a ring where it lands */
static void sc_intro_proj(uint32_t now)
{
    int t = (int)(now - sc_in.t0) - SI_FLICK, k;
    int x0 = SC_PX - 7 + 10, y0 = SC_PY + 1 - 22;
    if (t < 0 || t > 1500)
        return;
    for (k = 0; k < 4; k++) {
        int tt = t - k * 45;
        int x, y;
        if (tt < 0)
            continue;
        if (tt > 900)
            tt = 900;
        x = x0 + tt * 60 / 1000;
        y = y0 - tt * 25 / 1000 + tt * tt * 40 / 1000000;
        if (k == 0)
            fb_rect(x, y, 2, 2, C_EMBER);
        else if (t < 900)
            fb_pset(x, y, k < 3 ? C_EMBER : C_F3);
    }
    if (t >= 900) {                                                   /* it lands: two rings widen and fade */
        int a = t - 900, r = a / 40, xx, lx = x0 + 54, ly = y0 - 22 + 36 + 22 * 0;
        ly = y0 + 11;
        for (xx = -r; xx <= r; xx++) {
            int yy = sc_isqrt(r * r - xx * xx) / 4;
            if (((xx + r) & 1) == 0 && a < 1400) {
                fb_pset(lx + xx, ly + yy, a < 700 ? C_MOON2 : C_W0);
                fb_pset(lx + xx, ly - yy, a < 700 ? C_MOON2 : C_W0);
            }
        }
        if (a < 150)
            fb_pset(lx, ly, C_EMBER);                                 /* a last spark */
    }
}

/* the title's sky, above the cliff: a dithered ramp from the night down to the sky's top colour, stars, the name */
static void sc_intro_sky(int d, int p, uint32_t t)
{
    static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    static const uint8_t RAMP[5] = {C_BG, C_F0, C_F1, C_F2, C_S0};
    int y, x, i;
    for (y = 0; y < d; y++) {
        int wy = y + p, q = wy >= SC_H - 4 ? 63 : wy * 63 / (SC_H - 4), lv = q >> 4, fr = q & 15;
        const uint8_t *bt = BAYER + (y & 3) * 4;
        for (x = 0; x < FB_W; x++)
            fb[y * FB_W + x] = bt[x & 3] < fr ? RAMP[lv + 1] : RAMP[lv];
    }
    fb_clip(0, 0, FB_W, d);
    for (i = 0; i < 70; i++) {
        uint32_t k = sc_hash((uint32_t)i * 2654435761u + 0x2Fu);
        int sx = (int)(k % 238u) + 1, sy = 2 + (int)((k >> 9) % 180u) - p;
        uint8_t c = ((k >> 20) & 3u) ? C_STAR2 : C_STAR;
        if (((t >> 7) + (k >> 12)) % 31u == 0u)
            c = C_STAR;
        fb_pset(sx, sy, c);
    }
    {
        static const uint8_t FADE[4] = {C_F3, C_S5, C_INK2, C_INK};
        int a = (int)t - 400, b = (int)t - 1000, ca = a < 0 ? -1 : (a / 300 > 3 ? 3 : a / 300), cb = b < 0 ? -1 : (b / 300 > 3 ? 3 : b / 300);
        if (ca >= 0)
            fb_text2_c(FB_W / 2, 66 - p, "SKYLYNNE", FADE[ca], 4);
        if (cb >= 0)
            fb_text_c(FB_W / 2, 104 - p, "by Obby Maul", FADE[cb]);
    }
    fb_unclip();
}

/* one frame of the intro: the scene (with the walker) pushed down by the pan, and the title sky above it */
static void sc_intro_draw(uint32_t now)
{
    int t = (int)(now - sc_in.t0), p, y, d;
    sc_draw(now);
    if (t < SI_PAN0)
        p = 0;
    else if (t >= SI_PAN1)
        p = SC_H;
    else {
        int u = (t - SI_PAN0) * 256 / (SI_PAN1 - SI_PAN0);
        p = SC_H * ((u * u * (768 - 2 * u)) >> 16) >> 8;
    }
    d = SC_H - p;
    if (d > 0) {
        for (y = SC_H - 1; y >= d; y--) {
            const uint8_t *s = fb + (y - d) * FB_W;
            uint8_t *dst = fb + y * FB_W;
            int x;
            for (x = 0; x < FB_W; x++)
                dst[x] = s[x];
        }
        sc_intro_sky(d, p, (uint32_t)t);
    }
}
