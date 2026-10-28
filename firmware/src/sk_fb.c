/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* SKYLYNNE's screen: an indexed framebuffer, 240 x 240 bytes, one palette index a pixel.
 *
 * Every frame the scene draws the whole picture; sk_present() hashes it in 16 x 16 tiles and sends only the tiles that
 * changed, as RGB565 through two buffers (one converts while the other goes out). The panel's link moves a full screen in
 * ~77 ms, so a still picture costs nothing and the scene's moving parts are what travel. A palette change (a
 * scene's time of day) resends everything. The platform provides sk_lcd(x, y, w, h, pixels).
 *
 * The palette is the look: dusk lavenders and a pink horizon, or muted blue-greys and a dusty rose haze, silhouettes in deep slate, soft
 * pastel fireflies. Three themes share the indexes; a biome picks one and the palette blends into it over seconds. */

#include "sk_fonts.h"

static void sk_lcd(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px);   /* the platform */
static void sk_pump(void);

#define FB_W 240
#define FB_H 240
#define FB_TS 16
#define FB_TX (FB_W / FB_TS)
#define FB_TY (FB_H / FB_TS)
#define FB_PAL 64

/* the palette's indexes */
enum {
    C_BG = 0,
    C_S0, C_S1, C_S2, C_S3, C_S4, C_S5,                        /* the sky, the top down to the horizon */
    C_W0, C_W1, C_W2, C_W3, C_W4, C_W5,                        /* the water, the horizon down */
    C_F0, C_F1, C_F2, C_F3,                                    /* the land: the darkest, the trees, the stone, its lit edge */
    C_MOON, C_MOON2, C_STAR, C_STAR2,
    C_EMBER, C_PINK, C_CYAN, C_GREEN, C_YELLOW,                /* the pops of colour */
    C_INK, C_INK2, C_PANEL, C_PANEL2, C_BAR, C_ACC,            /* the interface */
    C_MIST, C_SCARF,
    C_N0, C_N1, C_N2, C_N3,                                    /* the city's neon: the only vivid colours */
    C_G0, C_G1                                                 /* the city's glow in the sky and on the water */
};

#define FB_NTHEME 3
/* three muted palettes after a flat blue-grey night: sky and water within a few steps of each other, near-black slate
 * land, a pale moon. They are close in value so that they blend into each other without a jolt. The pops of colour are
 * dusty. Only the city on the far shore is vivid: four neon colours and a faint glow of its own. */
static const uint32_t SK_THEME[FB_NTHEME][40] = {
    {   /* slate night (the reference) */
        0x0E131C,
        0x4F6376, 0x546879, 0x586C7C, 0x5C707F, 0x607482, 0x657886,
        0x637783, 0x5F7380, 0x5B6F7C, 0x576B78, 0x536774, 0x4F6370,
        0x10192B, 0x182133, 0x28344A, 0x44526A,
        0xCDCDDA, 0xA4ACBC, 0xC8D2DE, 0x8C9CAC,
        0x9C8A7C, 0x9C8C98, 0x86A0AC, 0x88A094, 0xA8A48C,
        0xD0DCE6, 0x8C98AA, 0x161E2C, 0x273244, 0x9AA8C0, 0x9C7482,
        0x8499A8, 0xA06E7C,
        0xFF2E9A, 0x22E6FF, 0x9A5CFF, 0xFFB030,
        0x6A6A88, 0x5A5C7A,
    },
    {   /* dusk haze */
        0x120F1A,
        0x565A6E, 0x5B5F72, 0x606376, 0x666878, 0x6C6D7C, 0x73727F,
        0x6F6F7C, 0x6B6B78, 0x666775, 0x626372, 0x5D5E6E, 0x585A6A,
        0x14172A, 0x1C2032, 0x2C3248, 0x48516A,
        0xD6D2DC, 0xA8A8BA, 0xCCCCDA, 0x9096AC,
        0xA08E80, 0xA08E9C, 0x88A0AE, 0x8CA296, 0xA8A68E,
        0xD6D8E6, 0x8F95AA, 0x181A2C, 0x2A3046, 0x9CA6C0, 0xA07888,
        0x8A98AA, 0xA47282,
        0xFF2E9A, 0x22E6FF, 0x9A5CFF, 0xFFB030,
        0x7A6E88, 0x645E7C,
    },
    {   /* green-grey mist */
        0x0C1418,
        0x4A5F66, 0x4F646B, 0x54696F, 0x596D74, 0x5E7278, 0x64777D,
        0x62767C, 0x5E7278, 0x5A6E74, 0x566A70, 0x52666C, 0x4E6268,
        0x0F1A22, 0x16222C, 0x263642, 0x42525E,
        0xC8D4D6, 0x9CAEB4, 0xC0D0D2, 0x8AA0A6,
        0x9A8C7E, 0x9A8E96, 0x86A2AA, 0x88A496, 0xA6A68C,
        0xCCDADC, 0x8A9CA2, 0x121C22, 0x24323A, 0x96ACB6, 0x9C7882,
        0x84989E, 0xA07480,
        0xFF2E9A, 0x22E6FF, 0x9A5CFF, 0xFFB030,
        0x64708A, 0x56627A,
    },
};

static uint8_t fb[FB_W * FB_H] SK_POOL;
static uint16_t fb_lut[FB_PAL];                  /* index -> RGB565, byte-swapped as the panel takes it */
static uint32_t fb_hash[FB_TX * FB_TY];
static uint16_t fb_dma[2][FB_W * FB_TS] SK_POOL;
static uint8_t fb_full = 1;                       /* resend every tile at the next present */
static int16_t fb_cx0, fb_cy0, fb_cx1 = FB_W, fb_cy1 = FB_H;   /* the clip rectangle */

static uint8_t fb_cur[40][3], fb_tgt[40][3];     /* the palette now, and where it is going: R G B */
static uint8_t fb_fade_on, fb_started;

static void fb_lut_set(uint32_t i)
{
    uint32_t v = ((uint32_t)(fb_cur[i][0] & 0xF8u) << 8) | ((uint32_t)(fb_cur[i][1] & 0xFCu) << 3) | (fb_cur[i][2] >> 3);
    uint16_t s = (uint16_t)(((v >> 8) & 0xFFu) | ((v & 0xFFu) << 8));
    if (fb_lut[i] != s) {
        fb_lut[i] = s;
        fb_full = 1;
    }
}

/* a theme becomes the target; the first one is taken at once, the next ones are blended in by fb_fade() */
static void fb_theme(uint32_t t)
{
    uint32_t i;
    const uint32_t *rgb = SK_THEME[t % FB_NTHEME];
    for (i = 0; i < 40u; i++) {
        fb_tgt[i][0] = (uint8_t)(rgb[i] >> 16);
        fb_tgt[i][1] = (uint8_t)(rgb[i] >> 8);
        fb_tgt[i][2] = (uint8_t)rgb[i];
        if (!fb_started) {
            fb_cur[i][0] = fb_tgt[i][0];
            fb_cur[i][1] = fb_tgt[i][1];
            fb_cur[i][2] = fb_tgt[i][2];
            fb_lut_set(i);
        }
    }
    fb_started = 1;
    fb_fade_on = 1;
}

static void fb_theme_snap(uint32_t t)            /* at once (the crash and update screens, the splash) */
{
    fb_started = 0;
    fb_theme(t);
}

/* every few frames: each colour moves a part of the way to its target (about 1/24, at least one step), so a change of
 * mood takes several seconds and never jumps */
static void fb_fade(void)
{
    uint32_t i, k, moving = 0;
    if (!fb_fade_on)
        return;
    for (i = 0; i < 40u; i++) {
        for (k = 0; k < 3u; k++) {
            int d = (int)fb_tgt[i][k] - (int)fb_cur[i][k];
            if (d) {
                int st = d / 24;
                if (!st)
                    st = d > 0 ? 1 : -1;
                fb_cur[i][k] = (uint8_t)(fb_cur[i][k] + st);
                moving = 1;
            }
        }
        fb_lut_set(i);
    }
    fb_fade_on = (uint8_t)moving;
}

static void fb_clip(int x0, int y0, int x1, int y1)
{
    fb_cx0 = (int16_t)(x0 < 0 ? 0 : x0);
    fb_cy0 = (int16_t)(y0 < 0 ? 0 : y0);
    fb_cx1 = (int16_t)(x1 > FB_W ? FB_W : x1);
    fb_cy1 = (int16_t)(y1 > FB_H ? FB_H : y1);
}
static void fb_unclip(void) { fb_clip(0, 0, FB_W, FB_H); }

static inline void fb_pset(int x, int y, uint8_t c)
{
    if (x >= fb_cx0 && x < fb_cx1 && y >= fb_cy0 && y < fb_cy1)
        fb[y * FB_W + x] = c;
}
static inline uint8_t fb_get(int x, int y)
{
    return (x >= 0 && x < FB_W && y >= 0 && y < FB_H) ? fb[y * FB_W + x] : 0;
}

static void fb_rect(int x, int y, int w, int h, uint8_t c)
{
    int x1 = x + w, y1 = y + h, i, j;
    if (x < fb_cx0)
        x = fb_cx0;
    if (y < fb_cy0)
        y = fb_cy0;
    if (x1 > fb_cx1)
        x1 = fb_cx1;
    if (y1 > fb_cy1)
        y1 = fb_cy1;
    for (j = y; j < y1; j++) {
        uint8_t *p = fb + j * FB_W;
        for (i = x; i < x1; i++)
            p[i] = c;
    }
}
static void fb_fill(uint8_t c) { fb_rect(0, 0, FB_W, FB_H, c); }
static void fb_hline(int x, int y, int w, uint8_t c) { fb_rect(x, y, w, 1, c); }
static void fb_vline(int x, int y, int h, uint8_t c) { fb_rect(x, y, 1, h, c); }
static void fb_frame(int x, int y, int w, int h, uint8_t c)
{
    fb_hline(x, y, w, c);
    fb_hline(x, y + h - 1, w, c);
    fb_vline(x, y, h, c);
    fb_vline(x + w - 1, y, h, c);
}
/* a pixel-art rounded box: the corners cut (r 1) or stepped (r 2) */
static void fb_rbox(int x, int y, int w, int h, int r, uint8_t c)
{
    if (r <= 0 || w < 4 || h < 4) {
        fb_rect(x, y, w, h, c);
        return;
    }
    fb_rect(x + r, y, w - 2 * r, h, c);
    fb_rect(x, y + r, r, h - 2 * r, c);
    fb_rect(x + w - r, y + r, r, h - 2 * r, c);
    if (r >= 2) {
        fb_pset(x + 1, y + 1, c);
        fb_pset(x + w - 2, y + 1, c);
        fb_pset(x + 1, y + h - 2, c);
        fb_pset(x + w - 2, y + h - 2, c);
    }
}
static void fb_line(int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        int e2 = 2 * e;
        fb_pset(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) {
            e += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            e += dx;
            y0 += sy;
        }
    }
}
/* a checkered (50 %) rectangle: a dithered shade over what is there */
static void fb_checker(int x, int y, int w, int h, uint8_t c)
{
    int i, j;
    for (j = y; j < y + h; j++)
        for (i = x + ((j ^ x) & 1); i < x + w; i += 2)
            fb_pset(i, j, c);
}
/* a filled disc */
static void fb_disc(int cx, int cy, int r, uint8_t c)
{
    int i, j;
    for (j = -r; j <= r; j++)
        for (i = -r; i <= r; i++)
            if (i * i + j * j <= r * r)
                fb_pset(cx + i, cy + j, c);
}

/* ------------------------------------------------------------------ text --- */
static int fb_glyph(const sk_font_t *f, uint8_t ch, int x, int y, uint8_t c)
{
    const sk_glyph_t *g = &f->g[ch];
    const uint8_t *b;
    int r, i, adv = g->adv;
    if (!adv)
        return 0;
    b = f->bits + g->off;
    for (r = 0; r < f->h; r++) {
        uint32_t row = ((uint32_t)b[2 * r] << 8) | b[2 * r + 1];
        for (i = 0; row; i++, row = (row << 1) & 0xFFFFu)
            if (row & 0x8000u)
                fb_pset(x + i, y + r, c);
    }
    return adv;
}
static int fb_text(int x, int y, const char *s, uint8_t c)
{
    int x0 = x;
    while (*s)
        x += fb_glyph(&SK_FONT, (uint8_t)*s++, x, y, c);
    return x - x0;
}
static int fb_text_w(const char *s)
{
    int w = 0;
    for (; *s; s++)
        w += SK_FONT.g[(uint8_t)*s].adv;
    return w;
}
static int fb_text_c(int cx, int y, const char *s, uint8_t c) { return fb_text(cx - fb_text_w(s) / 2, y, s, c); }
static int fb_text_r(int xr, int y, const char *s, uint8_t c) { return fb_text(xr - fb_text_w(s), y, s, c); }
/* text with a one-pixel halo (readable over the picture) */
static int fb_text_halo(int x, int y, const char *s, uint8_t c, uint8_t halo)
{
    int dx, dy;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (dx || dy)
                fb_text(x + dx, y + dy, s, halo);
    return fb_text(x, y, s, c);
}

/* ------------------------------------------------------------- present --- */
static uint32_t fb_tile_hash(int tx, int ty)
{
    const uint8_t *p = fb + ty * FB_TS * FB_W + tx * FB_TS;
    uint32_t h = 2166136261u;
    int r, k;
    for (r = 0; r < FB_TS; r++, p += FB_W) {
        const uint8_t *q = p;
        for (k = 0; k < FB_TS; k += 4, q += 4) {
            uint32_t w = (uint32_t)q[0] | (uint32_t)q[1] << 8 | (uint32_t)q[2] << 16 | (uint32_t)q[3] << 24;
            h = (h ^ w) * 16777619u;
            h = h << 13 | h >> 19;                  /* (the rotation: a change in a word's high byte must reach the hash) */
        }
    }
    return h;
}

static void sk_present(void)
{
    int tx, ty, which = 0;
    for (ty = 0; ty < FB_TY; ty++) {
        uint16_t dirty = 0;
        for (tx = 0; tx < FB_TX; tx++) {
            uint32_t h = fb_tile_hash(tx, ty);
            if (fb_full || h != fb_hash[ty * FB_TX + tx]) {
                fb_hash[ty * FB_TX + tx] = h;
                dirty |= (uint16_t)(1u << tx);
            }
        }
        tx = 0;
        while (dirty && tx < FB_TX) {
            int t0, t1, x0, w, r, i;
            uint16_t *d;
            if (!((dirty >> tx) & 1u)) {
                tx++;
                continue;
            }
            t0 = tx;
            while (tx < FB_TX && ((dirty >> tx) & 1u))
                tx++;
            t1 = tx;
            x0 = t0 * FB_TS;
            w = (t1 - t0) * FB_TS;
            d = fb_dma[which];
            which ^= 1;
            for (r = 0; r < FB_TS; r++) {
                const uint8_t *s = fb + (ty * FB_TS + r) * FB_W + x0;
                for (i = 0; i < w; i++)
                    *d++ = fb_lut[s[i] & (FB_PAL - 1)];
            }
            sk_lcd((uint32_t)x0, (uint32_t)(ty * FB_TS), (uint32_t)w, FB_TS, fb_dma[which ^ 1]);
        }
        sk_pump();                                  /* the keys between tile rows */
    }
    fb_full = 0;
}
