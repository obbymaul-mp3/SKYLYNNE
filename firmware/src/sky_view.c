/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* SKYLYNNE's home screen: someone sits on the edge of a sea cliff and looks out over a great sea under the night sky.
 * Each of the six voices is a part of what he sees: the sea (swell and ripples), the moon (glow and phase), the stars
 * (flashes), the lights on the horizon (a lighthouse beam), the aurora and the comets. A strike lights its part; the
 * knobs of the selected voice change how big, how many, how long, and what colour. He points at the part you are
 * turning. Every half minute or so he stands, brushes himself down and sits again, and now and then lights a cigarette.
 * Included by cf_screens.c; the whole area from the bar to the knobs is its. */

#define SKYV_Y0 Y_VIEW
#define SKYV_Y1 Y_KNOBS
#define SKYV_HZ 128                      /* the horizon */
#define SKYV_PX 100                      /* where he sits (the hip) */
#define SKYV_PY 142                      /* the cliff's top there */
#define SKYV_EDGE 105                    /* the cliff's edge */

static const uint8_t SKYV_COL[SKY_NV] = {60, 61, 62, 63, 53, 54};       /* the voices' dots */
static const uint8_t SKYV_MAT[6] = {63, 61, 60, 62, 54, 53};            /* a material's colour for lights, aurora, comets */
static const int16_t SKYV_AIM[SKY_NV][2] = {{150, 166}, {172, 56}, {120, 40}, {200, 124}, {80, 40}, {205, 30}};

enum { SV_SIT, SV_POINT, SV_TIDY, SV_SMOKE };

static struct {
    uint8_t init, st;
    uint8_t cnt[SKY_NV];
    uint32_t hit[SKY_NV];                /* when each voice last struck */
    uint32_t t0, next, touch_n, touch_t;
    uint8_t aim;
} skyv;

static int skyv_tri(int x)               /* a triangle wave, 0..255 over 512 */
{
    x &= 511;
    return x < 256 ? x : 511 - x;
}

/* what each voice shows now, 0..255: its own ambient (the level) plus the strike's light dying away (the decay) */
static int skyv_e[SKY_NV];

static void skyv_update(uint32_t now)
{
    int v;
    if (!skyv.init) {
        skyv.init = 1;
        skyv.next = now + 30000u;
        for (v = 0; v < SKY_NV; v++) {
            skyv.hit[v] = now - 100000u;
            skyv.cnt[v] = sky_ui.cnt[v];
        }
        skyv.touch_n = sky_touch_n;
    }
    for (v = 0; v < SKY_NV; v++) {
        uint32_t age, dur;
        int e, base;
        if (sky_ui.cnt[v] != skyv.cnt[v]) {
            skyv.cnt[v] = sky_ui.cnt[v];
            skyv.hit[v] = now;
        }
        age = now - skyv.hit[v];
        dur = 700u + (uint32_t)(sky_live[v].decay + 6) * 220u;
        e = age >= dur ? 0 : 255 - (int)(age * 255u / dur);
        base = sky_live[v].level * 40 / 100;
        skyv_e[v] = base + e * (255 - base) / 255;
    }
    /* the knob turned: he points at what you are changing */
    if (skyv.touch_n != sky_touch_n) {
        skyv.touch_n = sky_touch_n;
        skyv.touch_t = now;
        skyv.aim = sky_touch_v;
        if (skyv.st == SV_SIT || skyv.st == SV_SMOKE || skyv.st == SV_POINT) {
            skyv.st = SV_POINT;
            skyv.t0 = now;
        }
    }
    switch (skyv.st) {
    case SV_POINT:
        if (now - skyv.touch_t > 1800u) {
            skyv.st = SV_SIT;
            skyv.t0 = now;
        }
        break;
    case SV_TIDY:
        if (now - skyv.t0 > 3350u) {
            skyv.st = SV_SIT;
            skyv.next = now + 26000u + fl_hash32(now) % 8000u;
        }
        break;
    case SV_SMOKE:
        if (now - skyv.t0 > 8500u) {
            skyv.st = SV_SIT;
            skyv.next = now + 26000u + fl_hash32(now) % 8000u;
        }
        break;
    default:
        if ((int32_t)(now - skyv.next) >= 0) {
            skyv.st = (fl_hash32(now) & 3u) == 0u ? SV_SMOKE : SV_TIDY;
            skyv.t0 = now;
        }
        break;
    }
}

static void skyv_sky(uint32_t now)
{
    static const uint8_t BAND[4] = {56, 57, 58, 59};
    static const int EDGE[4] = {50, 82, 108, SKYV_HZ};
    int i, y = SKYV_Y0;
    for (i = 0; i < 4; i++) {
        fb_rect(0, y, FB_W, EDGE[i] - y, BAND[i]);
        if (i < 3)
            fb_checker(0, EDGE[i] - 4, FB_W, 4, BAND[i + 1]);
        y = EDGE[i];
    }
    (void)now;
}

static void skyv_stars(uint32_t now)
{
    int i, e = skyv_e[2];
    for (i = 0; i < 46; i++) {
        uint32_t k = fl_hash32((uint32_t)i * 2654435761u + 0x51u);
        int x = (int)(k % 236u) + 2, y = SKYV_Y0 + 4 + (int)((k >> 9) % 96u);
        uint8_t c = ((k >> 20) & 3u) ? 55 : 54;
        if (((now >> 7) + (k >> 12)) % 23u == 0u)
            c = 53;
        fb_pset(x, y, c);
        if (e > 40 && i * 100 / 46 < sky_live[2].dens) {           /* a flash: a cross, as large as the light */
            int s = 1 + e / 90, d;
            fb_pset(x, y, 53);
            for (d = 1; d <= s; d++) {
                uint8_t cc = d == 1 ? 53 : 54;
                fb_pset(x - d, y, cc);
                fb_pset(x + d, y, cc);
                fb_pset(x, y - d, cc);
                fb_pset(x, y + d, cc);
            }
        }
    }
}

static void skyv_aurora(uint32_t now)
{
    int x, e = skyv_e[4];
    uint8_t c0 = SKYV_MAT[sky_live[4].mat];
    if (e < 50)
        return;
    for (x = 4; x < 150; x += 2) {
        int top = SKYV_Y0 + 10 + (skyv_tri(x * 2 + (int)(now >> 5)) >> 4);
        int len = 6 + (e * 30 >> 8) + (skyv_tri(x * 3 - (int)(now >> 6)) >> 4), k;
        for (k = 0; k < len; k++) {
            if (((k + (x >> 1)) & 1) == 0 && k < len / 3 * 2)
                fb_pset(x, top + k, k < len / 3 ? c0 : 59);
            else if (((k + (x >> 1)) & 3) == 0)
                fb_pset(x, top + k, 59);
        }
    }
}

static void skyv_comet(uint32_t now)
{
    uint32_t age = now - skyv.hit[5];
    int p, hx, hy, t, n;
    if (age > 900u)
        return;
    p = (int)(age * 256u / 900u);
    hx = 232 - p * 150 / 256;
    hy = SKYV_Y0 + 14 + p * 70 / 256;
    n = 8 + sky_live[5].dens / 8;
    for (t = 0; t < n; t++) {
        if (t > 6 && (t & 1))
            continue;
        fb_pset(hx + t * 2, hy - t, t < 2 ? 53 : SKYV_MAT[sky_live[5].mat]);
    }
}

static void skyv_moon(void)
{
    int e = skyv_e[1], cx = 172, cy = 56, R = 8 + sky_live[1].level / 14;
    int cutx = R / 2 + sky_live[1].mat * (3 * R / 2 + 2) / 5, gl = 2 + e * 10 / 255, i, j, m = R + gl + 1;
    for (j = -m; j <= m; j++)
        for (i = -m; i <= m; i++) {
            int d2 = i * i + j * j;
            if (d2 > R * R && d2 <= (R + gl) * (R + gl) && ((i + j) & 1) == 0)
                fb_pset(cx + i, cy + j, d2 < (R + gl / 2) * (R + gl / 2) ? 55 : 58);
            else if (d2 <= R * R && (i - cutx) * (i - cutx) + (j + 1) * (j + 1) > (R - 1) * (R - 1))
                fb_pset(cx + i, cy + j, 53);
        }
}

static void skyv_sea(uint32_t now)
{
    int y, r, e0 = skyv_e[0], e1 = skyv_e[1];
    fb_rect(0, SKYV_HZ, FB_W, 14, 58);
    fb_rect(0, SKYV_HZ + 14, FB_W, 20, 57);
    fb_rect(0, SKYV_HZ + 34, FB_W, SKYV_Y1 - SKYV_HZ - 34, 56);
    fb_checker(0, SKYV_HZ + 10, FB_W, 4, 57);
    fb_checker(0, SKYV_HZ + 30, FB_W, 4, 56);
    fb_hline(0, SKYV_HZ, FB_W, 59);
    /* the swell: rows of dashes, wider and swifter near us, lifting with the sea voice */
    for (r = 0; r < 16; r++) {
        int py = SKYV_HZ + 3 + r * r / 3 + r * 2, len = 3 + r / 2, gap = 9 + r, per = len + gap, x;
        int off = (int)((now >> 6) * (uint32_t)(1 + r / 3)) % per;
        uint8_t c = e0 > 110 ? 55 : 58;
        if (py >= SKYV_Y1)
            break;
        for (x = -off + (r * 7) % per; x < FB_W; x += per) {
            int yo = (skyv_tri(x * 2 + (int)(now >> 3)) * (e0 >> 5)) >> 9;
            fb_hline(x, py + yo, len, c);
        }
    }
    /* the moon's path on the water, breaking up into glints */
    for (y = SKYV_HZ + 2; y < SKYV_Y1; y += 2) {
        int dp = y - SKYV_HZ, w = 2 + dp / 5 + (e1 >> 6);
        int wob = ((skyv_tri((y << 3) + (int)(now >> 4)) - 128) * (3 + dp / 20)) >> 7;
        if (fl_hash32((uint32_t)y * 13u + (now >> 8)) % 5u != 0u) {
            fb_checker(172 + wob - w / 2 - 2, y, w + 4, 1, 55);
            fb_hline(172 + wob - w / 2, y, w, e1 > 150 ? 53 : 54);
        }
    }
    /* ripples from the sea voice's strikes */
    {
        uint32_t age = now - skyv.hit[0];
        int k;
        for (k = 0; k < 3; k++) {
            int a = (int)age - k * 350, rr, q;
            if (a <= 0 || a > 2600)
                continue;
            rr = a / 12;
            if (rr > 80)
                continue;
            for (q = 0; q < 28; q++) {
                int ang = q * 36, x = 150 + ((fl_fsin(ang + 256) * rr) >> 14), yy = 166 + (((fl_fsin(ang) * rr) >> 14) >> 2);
                if (yy > SKYV_HZ && yy < SKYV_Y1 && x >= 0 && x < FB_W && (rr < 50 || (q & 1)))
                    fb_pset(x, yy, k == 0 ? 54 : 55);
            }
        }
    }
}

/* the lights on the horizon and the lighthouse, with their reflections */
static void skyv_lights(uint32_t now)
{
    int e = skyv_e[3], i;
    uint8_t col = SKYV_MAT[sky_live[3].mat];
    for (i = 0; i < 12; i++) {
        int x = 126 + i * 9 + (int)(fl_hash32((uint32_t)i + 9u) % 4u), on = e > 60 && i * 100 / 12 < sky_live[3].dens;
        fb_pset(x, SKYV_HZ - 1, on ? col : 55);
        if (on)
            fb_vline(x, SKYV_HZ + 1, 2 + e / 60, col);
    }
    fb_rect(226, SKYV_HZ - 12, 4, 12, 52);
    fb_vline(229, SKYV_HZ - 12, 12, 55);
    fb_pset(228, SKYV_HZ - 13, e > 40 ? col : 55);
    if (e > 40) {
        int ty = SKYV_HZ - 36 + (skyv_tri((int)(now >> 2)) >> 3);
        fb_line(228, SKYV_HZ - 13, 150, ty, col);
        fb_line(228, SKYV_HZ - 13, 150, ty + 3, col);
    }
}

static void skyv_cliff(uint32_t now)
{
    int x;
    for (x = 0; x < SKYV_EDGE; x++) {
        uint32_t k = fl_hash32((uint32_t)(x >> 2) + 0x61u);
        int ty = SKYV_PY + (x < 30 ? (30 - x) / 3 : 0) + (int)(k % 3u);
        fb_rect(x, ty, 1, SKYV_Y1 - ty, 56);
        fb_pset(x, ty, 58);
        if ((k >> 8) % 5u == 0u)
            fb_pset(x, ty - 1, 57);                  /* tufts of grass */
    }
    fb_rect(SKYV_EDGE, SKYV_PY + 1, 2, SKYV_Y1 - SKYV_PY - 1, 58);          /* the rim of the face */
    fb_vline(SKYV_EDGE + 2, SKYV_PY + 6, 40, 57);
    fb_checker(36, SKYV_PY + 12, 68, 40, 52);                                /* the face in shadow */
    fb_checker(SKYV_EDGE - 14, SKYV_PY + 30, 14, 10, 55);                    /* a few moonlit ledges */
    (void)now;
}

static void skyv_arm(int sx, int sy, int tx, int ty, uint8_t c)
{
    fb_line(sx, sy, tx, ty, c);
    fb_line(sx + 1, sy, tx + 1, ty, c);
}

/* a ember-lit cigarette's smoke rising from (ex, ey) */
static void skyv_smoke(int ex, int ey, uint32_t now, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        int ph = (int)((now / 90u + (uint32_t)i * 13u) % 40u);
        int x = ex - ph / 3 + ((skyv_tri(ph * 9 + i * 50) - 128) >> 5), y = ey - ph;
        if (ph < 34) {
            fb_pset(x, y, ph < 20 ? 54 : 55);
            if (ph > 10)
                fb_pset(x + 1, y, 55);
        }
    }
}

static void skyv_figure(uint32_t now)
{
    int px = SKYV_PX, py = SKYV_PY, age = (int)(now - skyv.t0), i;
    int cx = px - 7, fl = skyv_tri((int)(now >> 4)) >> 7;       /* the scarf's flutter, 0..1 */
    if (skyv.st == SV_TIDY && age >= 250 && age < 3050) {      /* standing */
        int sw = age >= 650 && age < 2450;
        fb_rect(cx - 3, py - 10, 3, 10, 52);
        fb_rect(cx + 1, py - 10, 3, 10, 52);
        fb_rect(cx - 3, py - 22, 7, 12, 52);
        fb_rect(cx - 3, py - 28, 6, 6, 52);
        fb_vline(cx + 3, py - 21, 10, 55);
        fb_vline(cx + 2, py - 27, 4, 55);
        fb_pset(cx + 1, py - 25, 53);
        fb_hline(cx - 3, py - 22, 7, 60);
        for (i = 1; i < 5; i++)
            fb_pset(cx - 3 - i, py - 21 + fl + (i >> 1), 60);
        if (sw) {
            int t = (age - 650) % 600, hy = py - 19 + t * 13 / 600;
            skyv_arm(cx + 1, py - 20, cx + 4, hy, 57);
            for (i = 0; i < 5; i++) {
                int ph = (age / 6 + i * 17) % 30;
                fb_pset(cx + 5 + (i & 1) * 2, py - 14 + ph, ph < 20 ? 54 : 55);
            }
        } else
            skyv_arm(cx + 1, py - 20, cx + 2, py - 9, 57);
        return;
    }
    if (skyv.st == SV_TIDY && ((age >= 0 && age < 250) || (age >= 3050 && age < 3350))) {   /* half up */
        fb_rect(cx - 3, py - 7, 7, 7, 52);
        fb_rect(cx - 3, py - 19, 7, 12, 52);
        fb_rect(cx - 3, py - 25, 6, 6, 52);
        fb_vline(cx + 3, py - 18, 10, 55);
        fb_vline(cx + 2, py - 24, 4, 55);
        fb_pset(cx + 1, py - 22, 53);
        fb_hline(cx - 3, py - 19, 7, 60);
        return;
    }
    /* sitting */
    fb_rect(px - 2, py - 3, 10, 3, 52);                         /* the thigh */
    fb_hline(px - 1, py - 3, 8, 55);
    fb_rect(px + 6, py - 1, 3, 9, 52);                          /* the leg over the edge */
    fb_rect(px + 6, py + 7, 5, 2, 52);
    fb_pset(px + 10, py + 7, 55);
    fb_rect(px - 4, py - 14, 6, 12, 52);                        /* the back and chest */
    fb_vline(px + 1, py - 13, 10, 55);
    fb_rect(px - 3, py - 20, 6, 6, 52);                         /* the head */
    fb_vline(px + 2, py - 19, 4, 55);
    fb_hline(px - 2, py - 20, 4, 58);
    {
        int up = skyv.st == SV_POINT && SKYV_AIM[skyv.aim][1] < 80;
        fb_pset(px + 1, py - (up ? 18 : 17), 53);               /* the eye: he looks up for the high things */
    }
    fb_hline(px - 3, py - 14, 6, 60);                           /* the scarf */
    for (i = 1; i < 6; i++)
        fb_pset(px - 4 - i, py - 13 + fl + (i >> 1), 60);
    if (skyv.st == SV_POINT) {
        int dx = SKYV_AIM[skyv.aim][0] - (px + 1), dy = SKYV_AIM[skyv.aim][1] - (py - 11);
        int m = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
        int ux = dx * 12 / m, uy = dy * 12 / m, bob = skyv_tri(age * 2) >> 7;
        skyv_arm(px, py - 11, px + ux, py - 11 + uy - bob, 57);
        fb_rect(px + ux, py - 11 + uy - bob - 1, 2, 2, 53);
    } else if (skyv.st == SV_SMOKE) {
        int ex, ey;
        if (age < 600 || (age >= 600 && age < 1900)) {          /* the hand at the mouth */
            int ember = age < 300 ? 0 : (age < 600 ? 1 : 2);
            skyv_arm(px, py - 11, px + 3, py - 15, 57);
            fb_rect(px + 3, py - 17, 2, 2, 54);
            fb_hline(px + 4, py - 16, 3, 53);
            ex = px + 7;
            ey = py - 16;
            if (age < 300 && (age / 60) & 1)
                fb_rect(ex - 1, ey - 2, 2, 2, 62);              /* the lighter's flame */
            if (ember >= 1)
                fb_pset(ex, ey, 62);
            if (ember == 2 && age > 700) {
                fb_checker(ex - 2, ey - 2, 5, 5, 62);
                fb_pset(ex, ey, 62);
            }
            skyv_smoke(ex, ey - 1, now, age > 600 ? 4 : 0);
        } else {                                                /* the hand rests, the cigarette still burning */
            skyv_arm(px, py - 11, px + 5, py - 5, 57);
            ex = px + 8;
            ey = py - 6;
            fb_hline(px + 6, py - 6, 2, 53);
            fb_pset(ex, ey, ((now >> 8) & 7u) < 5u ? 62 : 55);
            skyv_smoke(ex, ey - 1, now, age < 2500 ? 7 : 3);
        }
    } else
        skyv_arm(px - 1, py - 11, px + 4, py - 4, 57);          /* the arm on the knee */
}

/* pops of colour: fireflies drifting over the whole scene, and embers in a voice's colour rising where it struck */
static void skyv_glow(int x, int y, uint8_t c, int p)         /* p: 0..255 */
{
    if (p < 40)
        return;
    fb_pset(x, y, c);
    if (p > 110) {
        fb_pset(x + 1, y, c);
        fb_pset(x, y + 1, c);
    }
    if (p > 200) {                                          /* at its brightest a small cross of light round it */
        fb_pset(x - 1, y, c);
        fb_pset(x, y - 1, c);
        fb_pset(x + 2, y + 1, c);
        fb_pset(x + 1, y + 2, c);
    }
}

static void skyv_fireflies(uint32_t now)
{
    static const uint8_t FC[8] = {62, 60, 62, 62, 60, 62, 61, 62};
    int i, v;
    for (i = 0; i < 16; i++) {
        uint32_t k = fl_hash32((uint32_t)i * 7919u + 0x2Bu);
        int bx = (int)(k % 236u) + 2, by = SKYV_Y0 + 70 + (int)((k >> 8) % 112u);
        int x = bx + ((skyv_tri((int)(now >> 5) + (int)(k >> 16)) - 128) >> 3);
        int y = by + ((skyv_tri((int)(now >> 6) + (int)(k >> 20) * 3) - 128) >> 4) - (int)((now >> 8) + (k >> 4)) % 14u;
        int p = skyv_tri((int)(now >> 2) + (int)(k >> 9));
        if (x > 86 && x < 114 && y > 108 && y < 172)
            continue;                                       /* (not over him) */
        skyv_glow(x, y, FC[i & 7], p);
    }
    for (v = 0; v < SKY_NV; v++) {
        uint32_t age = now - skyv.hit[v];
        int k;
        if (age > 1600u)
            continue;
        for (k = 0; k < 4; k++) {
            uint32_t h = fl_hash32((uint32_t)(v * 31 + k) * 2654435761u + skyv.cnt[v]);
            int x = SKYV_AIM[v][0] + (int)(h % 25u) - 12 + ((skyv_tri((int)(age >> 2) + k * 90) - 128) >> 4);
            int y = SKYV_AIM[v][1] - (int)(age >> 4) - k * 4;
            if (y > SKYV_Y0 && y < SKYV_Y1)
                skyv_glow(x, y, SKYV_COL[v] == 53 || SKYV_COL[v] == 54 ? 62 : SKYV_COL[v], 255 - (int)(age * 255u / 1600u));
        }
    }
}

static void skyv_hud(void)
{
    char t[24];
    int i;
    str_cpy(t, SKY_VNAME[sky_sel], sizeof t);
    fb_text_halo(&CF_FONT_UI, 5, SKYV_Y0 + 3, t, 53, 56, 1);
    for (i = 0; i < SKY_NV; i++) {
        int x = 186 + i * 8, y = SKYV_Y0 + 5;
        fb_rect(x, y, 3, 3, skyv_e[i] > 120 ? SKYV_COL[i] : 55);
        if (i == sky_sel)
            fb_rect(x - 2, y - 2, 7, 1, 53), fb_rect(x - 2, y + 4, 7, 1, 53);
    }
}

static void draw_sky_home(void)
{
    uint32_t now = cu.now;
    skyv_update(now);
    skyv_sky(now);
    skyv_aurora(now);
    skyv_stars(now);
    skyv_comet(now);
    skyv_moon();
    skyv_sea(now);
    skyv_lights(now);
    skyv_cliff(now);
    skyv_figure(now);
    skyv_fireflies(now);
    skyv_hud();
}
