/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* SKYLYNNE's home screen: a night sky over a futuristic city in rain. Six towers, one a voice: when it strikes, its
 * windows light from the street up in the voice's colour, a beam climbs from the roof and the street shines under it;
 * the light fades as the sound rings out. Included by cf_screens.c; the whole area from the bar to the knobs is its. */

#define SKYC_Y0 Y_VIEW                   /* the picture runs from here to the knobs */
#define SKYC_Y1 Y_KNOBS
#define SKYC_GROUND 184                  /* the street's top */

static const uint8_t SKYC_COL[SKY_NV] = {60, 61, 62, 63, 53, 54};       /* the voices' lights */
static const uint8_t SKYC_H[SKY_NV] = {92, 120, 76, 132, 102, 84};       /* tower heights */
static const uint8_t SKYC_STEP[SKY_NV] = {0, 1, 0, 1, 0, 1};             /* a stepped roof or a flat one */

static void skyc_sky(uint32_t now)
{
    /* four bands of the night, dark at the top to the glow at the horizon, each dithered into the next */
    static const uint8_t BAND[4] = {56, 57, 58, 59};
    static const int EDGE[4] = {52, 86, 120, 152};
    int i, y = SKYC_Y0;
    for (i = 0; i < 4; i++) {
        int y1 = i == 3 ? SKYC_GROUND : EDGE[i];
        fb_rect(0, y, FB_W, y1 - y, BAND[i]);
        if (i < 3)
            fb_checker(0, y1 - 4, FB_W, 4, BAND[i + 1]);
        y = y1;
    }
    /* stars: a fixed field, a few glinting */
    for (i = 0; i < 46; i++) {
        uint32_t k = fl_hash32((uint32_t)i * 2654435761u + 0x51u);
        int x = (int)(k % 238u) + 1, yy = SKYC_Y0 + 3 + (int)((k >> 9) % 100u);
        uint8_t c = ((k >> 20) & 3u) ? 55 : 54;
        if (((now >> 7) + (k >> 12)) % 23u == 0u)
            c = 53;
        fb_pset(x, yy, c);
    }
    /* the moon: a crescent */
    for (i = -9; i <= 9; i++) {
        int j;
        for (j = -9; j <= 9; j++) {
            if (i * i + j * j <= 81 && (i - 4) * (i - 4) + (j + 1) * (j + 1) > 64)
                fb_pset(208 + i, SKYC_Y0 + 26 + j, 53);
        }
    }
}

/* the far city: low blocks, mostly dark, a few dim windows */
static void skyc_far(uint32_t now)
{
    int x = 0, n = 0;
    while (x < FB_W) {
        uint32_t k = fl_hash32((uint32_t)n++ * 40503u + 0x77u);
        int w = 9 + (int)(k % 9u), h = 26 + (int)((k >> 8) % 34u), yt = SKYC_GROUND - h, r, c;
        fb_rect(x, yt, w, h, 52);
        fb_hline(x, yt, w, 56);
        for (r = 0; r < h - 6; r += 6)
            for (c = 2; c < w - 2; c += 4) {
                uint32_t q = fl_hash32(k + (uint32_t)(r * 31 + c));
                if (q % 7u == 0u)
                    fb_pset(x + c, yt + 4 + r, ((now >> 9) + q) % 29u == 0u ? 54 : 55);
            }
        if (k % 5u == 0u)
            fb_vline(x + w / 2, yt - 6, 6, 55);
        x += w + (int)((k >> 16) % 3u);
    }
}

static void skyc_tower(int i, uint32_t now)
{
    int x = 7 + i * 39, w = 31, h = SKYC_H[i], yt = SKYC_GROUND - h, rows, r, c, lit;
    uint8_t col = SKYC_COL[i];
    int ln = sky_ui.lit[i];
    fb_rect(x - 1, yt - 1, w + 2, h + 1, 52);                   /* the body, with an edge */
    fb_rect(x, yt, w, h, 56);
    fb_vline(x, yt, h, 55);
    fb_vline(x + w - 1, yt, h, 52);
    fb_hline(x, yt, w, ln > 20 ? col : 55);                       /* the roof's edge: the voice's neon, bright at a strike */
    if (SKYC_STEP[i]) {                                           /* a stepped roof */
        fb_rect(x + 6, yt - 9, w - 12, 9, 52);
        fb_rect(x + 7, yt - 8, w - 14, 8, 56);
        fb_vline(x + 7, yt - 8, 8, 55);
    }
    rows = (h - 12) / 9;
    lit = ln * (rows + 2) / 255;                                  /* rows lit, from the street up */
    for (r = 0; r < rows; r++) {
        int wy = SKYC_GROUND - 12 - r * 9;
        for (c = 0; c < 4; c++) {
            int wx = x + 3 + c * 7;
            uint32_t q = fl_hash32((uint32_t)(i * 977 + r * 31 + c * 7) + 0x33u);
            uint8_t wc = 0;
            if (r < lit)
                wc = col;                                         /* the strike's light */
            else if (q % 6u == 0u)
                wc = (((now >> 10) + q) % 17u == 0u) ? 54 : 55;   /* a quiet window, now and then flickering */
            if (wc) {
                if (r < lit)                                      /* a bloom round a lit window */
                    fb_checker(wx - 1, wy - 1, 6, 7, wc);
                fb_rect(wx, wy, 4, 5, wc);
            }
            else
                fb_rect(wx, wy, 4, 5, 52);                        /* a dark window */
        }
    }
    /* the beacon on the roof, and the beam of a strike climbing from it */
    {
        int by = yt - (SKYC_STEP[i] ? 10 : 2), on = ((now >> 9) + (uint32_t)i * 3u) % 4u == 0u;
        fb_pset(x + w / 2, by, on || ln > 40 ? col : 55);
        if (ln > 8) {
            int bl = ln * 70 / 255, k;
            for (k = 1; k <= bl; k++) {
                if (k < 14 || (k & 1))
                    fb_pset(x + w / 2, by - k, col);
                if (k < 10)
                    fb_pset(x + w / 2 - 1 + (k & 1) * 2, by - k, col);
            }
        }
    }
    /* the street under it: the light spilling and reflecting */
    {
        int rl = 4 + ln * 14 / 255, k;
        for (k = 0; k < rl; k++)
            if (ln > 10 || k < 2)
                fb_checker(x + 2 + k / 2, SKYC_GROUND + 2 + k, w - 4 - k, 1, ln > 10 ? col : 55);
    }
}

static void skyc_rain(uint32_t now)
{
    int i;
    for (i = 0; i < 38; i++) {
        uint32_t k = fl_hash32((uint32_t)i * 2654435761u + 0x2Fu);
        uint32_t sp = 70u + (k >> 24) % 60u;                     /* px per second */
        int x = (int)(k % 238u) + ((int)(now >> 6) & 1) * 0, y = SKYC_Y0 + (int)(((k >> 8) % 160u + now * sp / 1000u) % 170u);
        fb_pset(x, y, 55);
        fb_pset(x, y + 1, 55);
        if (k & 1u)
            fb_pset(x, y + 2, 54);
    }
}

static void draw_sky_city(void)
{
    uint32_t now = cu.now;
    char t[24];
    int i;
    skyc_sky(now);
    skyc_far(now);
    fb_rect(0, SKYC_GROUND, FB_W, SKYC_Y1 - SKYC_GROUND, 52);   /* the street */
    fb_hline(0, SKYC_GROUND, FB_W, 55);
    for (i = 0; i < SKY_NV; i++)
        skyc_tower(i, now);
    skyc_rain(now);
    str_cpy(t, "NEON RAIN  ", sizeof t);
    kint(t + str_len(t), (int)sky_c.bpm, 0);
    fb_text_halo(&CF_FONT_UI, 5, SKYC_Y0 + 3, t, 53, 56, 1);
}
