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
 * pastel fireflies. The themes share the indexes; a biome picks one and the palette blends into it over seconds. */

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

#define FB_NTHEME 11
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
    {   /* the cathedral: a low gold sun in a smoky mauve sky, a sea of cloud; the stone is near-black umber, the windows gold */
        0x16110F,
        0x564C58, 0x65535A, 0x7A5E5A, 0x926C5C, 0xAB7F62, 0xC29A6E,
        0x9C7C64, 0x8A6E5C, 0x7A6054, 0x6A5248, 0x5A443E, 0x4A3834,
        0x1A1314, 0x2A1E20, 0x4A3A3A, 0x7C6450,
        0xF2E2B8, 0xD2B27A, 0xE8D6B0, 0xB4A082,
        0xD0985A, 0xB89090, 0x8CA4A6, 0x9CA474, 0xE0C072,
        0xECE0CC, 0xA8987E, 0x1E1618, 0x34292B, 0xC2AA80, 0xC8A25A,
        0xA88C74, 0xA8604C,
        0xFFC24A, 0xFF8A2E, 0xFFE9A8, 0xE0603A,
        0xA67C44, 0x86643C,
    },
    {   /* the deep: filtered sun from above, murky teal, a drowned temple lit aqua by something old */
        0x0A161A,
        0x4A9490, 0x418580, 0x3A7874, 0x346C69, 0x2F6260, 0x2A5857,
        0x2C5A58, 0x265050, 0x204646, 0x1B3C3E, 0x163236, 0x12292D,
        0x0C1A1E, 0x142A2E, 0x24464A, 0x4A7C7A,
        0xDFF5E8, 0xA6D6CC, 0xCFEFE6, 0x8CBFB8,
        0xD8A878, 0xC090A0, 0x78C8C8, 0x80B890, 0xD8D090,
        0xDDF0EE, 0x8CB0AC, 0x0F2024, 0x1E3A3E, 0x8CCAC4, 0x6CD0C0,
        0x5C9C98, 0xC88070,
        0x30F0D8, 0x40C0FF, 0xB0FFF0, 0x60FFA8,
        0x3A9890, 0x2E7C78,
    },
    {   /* furnace dusk: a dim red-brown sky, the sun a smouldering coin */
        0x180F0E,
        0x3A2A30, 0x4A2C2C, 0x5E3430, 0x7A4030, 0x985034, 0xB4663A,
        0x8A5030, 0x76442A, 0x623A24, 0x50301E, 0x402818, 0x321E14,
        0x140E0E, 0x241818, 0x45302A, 0x8A5A3A,
        0xF8C888, 0xD89050, 0xF0C890, 0xB88858,
        0xE07838, 0xB87070, 0x7C9498, 0x98A070, 0xF0B050,
        0xF0DCC0, 0xA88C70, 0x1C1212, 0x302020, 0xC08C60, 0xE08A3C,
        0x8A5A44, 0xC0443A,
        0xFF9A30, 0xFF5A1E, 0xFFD890, 0xFF7A3A,
        0x9A5C2C, 0x7A4624,
    },
    {   /* overcast bronze-grey: stone, a pale disc behind cloud */
        0x121216,
        0x4E5058, 0x585A60, 0x646464, 0x726E6A, 0x827A70, 0x938774,
        0x7A7164, 0x6C645A, 0x5E574F, 0x504A44, 0x433E3A, 0x373432,
        0x16151A, 0x23222A, 0x3E3C44, 0x6C665E,
        0xE6DCC8, 0xBDB19A, 0xDCD2BE, 0xA09886,
        0xC08A58, 0xA08A90, 0x86989E, 0x929C7C, 0xD0B070,
        0xE4DED0, 0x9C9688, 0x1A1A1E, 0x2E2E34, 0xA8A090, 0xC0A060,
        0x908A80, 0xA8584C,
        0xFFB84A, 0xE89040, 0xFFE4A8, 0xD8783C,
        0x8C7850, 0x706040,
    },
    {   /* violet-rose twilight: the sun nearly gone */
        0x14101C,
        0x4A4466, 0x5A4C6C, 0x6E5674, 0x865F76, 0xA06C74, 0xBC8478,
        0x8E6C72, 0x7C5E68, 0x6A5260, 0x5A4658, 0x4A3A4A, 0x3C303E,
        0x161220, 0x241C30, 0x443A52, 0x80687A,
        0xFCEBC8, 0xE8C0A0, 0xF0E0D0, 0xB8A0B0,
        0xD49478, 0xC090A8, 0x90A4B8, 0xA0A88C, 0xE8C888,
        0xF0E4E4, 0xA890A0, 0x1C1624, 0x30283C, 0xC4A0B0, 0xD8A088,
        0xA88898, 0xB85A64,
        0xFFC87A, 0xFF9A6A, 0xFFEAC0, 0xF07C7C,
        0xA87C70, 0x886058,
    },
    {   /* coral lanterns: bright turquoise, warm pink lights */
        0x0A1822,
        0x58B0A8, 0x4CA0A0, 0x429298, 0x3A8490, 0x347888, 0x2E6C80,
        0x3A7C88, 0x327080, 0x2A6478, 0x24586C, 0x1E4C60, 0x184054,
        0x0E2030, 0x183444, 0x2C5468, 0x5C90A0,
        0xF0FAF0, 0xB8E4D8, 0xDCF4EC, 0x98CCC8,
        0xF0A880, 0xE890A0, 0x78D0D8, 0x88D0A0, 0xF0D890,
        0xE4F4F2, 0x90B8BC, 0x102430, 0x203C4C, 0x90D0D0, 0x70D8C8,
        0x60A8AC, 0xE88878,
        0x40FFE0, 0xFF8AB0, 0xC0FFF4, 0xFFC860,
        0x40A0A0, 0x308088,
    },
    {   /* the abyss: near-black blue, the machine the only light */
        0x040A0E,
        0x14303E, 0x112A38, 0x0F2432, 0x0D202C, 0x0B1B27, 0x0A1822,
        0x0E2430, 0x0C202A, 0x0A1B24, 0x09161E, 0x071218, 0x060E14,
        0x050C10, 0x0A1A20, 0x16303A, 0x2C5560,
        0xB8F0F0, 0x6CB8C0, 0xA8E0E0, 0x5C98A0,
        0xC09870, 0x9C7C94, 0x60B0B8, 0x70A890, 0xB8B880,
        0xC8E4E4, 0x6C9498, 0x08141A, 0x142830, 0x5CA8AC, 0x48C8C0,
        0x2C5C68, 0xB06860,
        0x20FFD0, 0x30A8FF, 0xA0FFF0, 0x50FF90,
        0x1E6C70, 0x185458,
    },
    {   /* light through water: pale green and gold, the sun strong */
        0x0A1A18,
        0x7CC4A4, 0x6CB498, 0x5CA48C, 0x4C9482, 0x408878, 0x38786C,
        0x3E7E70, 0x366E64, 0x2E6058, 0x28524C, 0x214540, 0x1B3A36,
        0x0E2420, 0x183830, 0x2E5A50, 0x6C9C80,
        0xFFFCE0, 0xD8F0B8, 0xF0F8D8, 0xA8D0A8,
        0xE0B880, 0xD09CA0, 0x88D0C8, 0x98D098, 0xE8E098,
        0xEEF8E8, 0x98BCA8, 0x10281F, 0x203C34, 0xA0D4B0, 0x88D8A8,
        0x78B49C, 0xD88470,
        0x58FFC8, 0xFFE060, 0xD8FFE0, 0x80FF98,
        0x58A88C, 0x448878,
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
/* the title's letters: 2x, with a wide tracking */
static int fb_glyph2(const sk_font_t *f, uint8_t ch, int x, int y, uint8_t c)
{
    const sk_glyph_t *g = &f->g[ch];
    const uint8_t *b;
    int r, i;
    if (!g->adv)
        return 0;
    b = f->bits + g->off;
    for (r = 0; r < f->h; r++) {
        uint32_t row = ((uint32_t)b[2 * r] << 8) | b[2 * r + 1];
        for (i = 0; row; i++, row = (row << 1) & 0xFFFFu)
            if (row & 0x8000u) {
                fb_pset(x + 2 * i, y + 2 * r, c);
                fb_pset(x + 2 * i + 1, y + 2 * r, c);
                fb_pset(x + 2 * i, y + 2 * r + 1, c);
                fb_pset(x + 2 * i + 1, y + 2 * r + 1, c);
            }
    }
    return 2 * g->adv;
}
static int fb_text2_w(const char *s, int track)
{
    int w = 0;
    for (; *s; s++)
        w += 2 * SK_FONT.g[(uint8_t)*s].adv + track;
    return w - track;
}
static void fb_text2_c(int cx, int y, const char *s, uint8_t c, int track)
{
    int x = cx - fb_text2_w(s, track) / 2;
    while (*s)
        x += fb_glyph2(&SK_FONT, (uint8_t)*s++, x, y, c) + track;
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
