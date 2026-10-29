/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* Glow, on the screen: the figure stands, rises and floats in a ball of energy while lightning walks across the sky.
 * Included by sk_scene.c, before the figure. What the sound side says is in sk_gv (sk_dsp.c): on, the stage (0..3) and a
 * counter that moves on every bar line the sky should answer. */

/* the aura's four colours, outer to core, per AURA setting; then the bolt and its halo. They are not part of a theme. */
enum { C_A0 = 40, C_A1, C_A2, C_A3, C_BOLT, C_BOLT2 };
static const uint32_t SK_AURA[6][4] = {
    {0x8A5A10, 0xE0A020, 0xFFD848, 0xFFF8C0},             /* gold */
    {0x607080, 0xA0B8D0, 0xE0F0FF, 0xFFFFFF},             /* white */
    {0x1840A0, 0x3080F0, 0x70C0FF, 0xE0F8FF},             /* blue */
    {0x206030, 0x40C060, 0x90FFA0, 0xF0FFF0},             /* green */
    {0x901818, 0xE03030, 0xFF8060, 0xFFE8D0},             /* red */
    {0x502890, 0x9040E0, 0xD090FF, 0xF8E8FF},             /* violet */
};

static void fb_fix(uint32_t i, uint32_t rgb)
{
    uint32_t k;
    for (k = 0; k < 3u; k++)
        fb_cur[i][k] = fb_tgt[i][k] = (uint8_t)((rgb >> (16 - 8 * k)) & 255u);
    fb_lut_set(i);
}

static struct {
    int rise;                            /* 0 (sitting) .. 256 (floating) */
    uint32_t t_last, t_on;
    uint8_t cnt, aura, was_on, was_bolt;
    struct { int16_t x; uint32_t t0, seed; } bolt[3];
    uint8_t nb;
} sg = {0, 0, 0, 0, 255, 0, 0, {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, 0};


static void sc_retint(uint32_t storm)
{
    uint32_t i;
    for (i = 0; i < 5u; i++)
        fb_tint[i] = storm ? SK_STORM[i] : sc_tint0[i];
    fb_theme(sc_pal_id);
}

static void sc_glow_update(uint32_t now)
{
    uint32_t dt = now - sg.t_last;
    uint32_t on = sk_gv.on;
    if (dt > 200u)
        dt = 200u;
    sg.t_last = now;
    if (sg.aura != sk_s.glo[3] % 6u) {
        uint32_t k;
        sg.aura = (uint8_t)(sk_s.glo[3] % 6u);
        for (k = 0; k < 4u; k++)
            fb_fix(C_A0 + k, SK_AURA[sg.aura][k]);
        fb_fix(C_BOLT, 0xD8E8FF);
        fb_fix(C_BOLT2, 0x6080D0);
    }
    if (on != sg.was_on) {
        sg.was_on = (uint8_t)on;
        sg.t_on = now;
        sc_retint(on);
    }
    if (on)
        sg.rise += (int)(dt * 256u / 1800u);
    else
        sg.rise -= (int)(dt * 256u / 900u);
    sg.rise = sg.rise < 0 ? 0 : sg.rise > 256 ? 256 : sg.rise;
    if (!on && sg.rise == 0)
        return;
    /* the sky answers: a bolt at each bar line the music says, and now and then by itself */
    {
        uint32_t want = 0;
        if (on && sg.rise > 140) {
            if (sg.cnt != sk_gv.cnt) {
                sg.cnt = sk_gv.cnt;
                want = 1;
            }
            if (sc_hash(now / 16u) % 2000u < (uint32_t)sk_s.glo[2] * 3u / 2u + 1u)
                want = 1;
        }
        if (want && sk_s.glo[2] > 4u) {
            uint32_t h = sc_hash(now * 7u + sg.nb);
            uint32_t j = sg.nb++ % 3u;
            sg.bolt[j].x = (int16_t)(16 + h % 208u);
            sg.bolt[j].t0 = now;
            sg.bolt[j].seed = h >> 8;
        }
    }
}

/* a jagged bolt from the top of the sky to the water, with a branch; it flickers out over 280 ms */
static void sc_bolt(int x0, uint32_t seed, uint32_t age)
{
    int x = x0, y = 0, bx = 0, by = 0, step = 0, br = 0, fl;
    uint32_t h = seed;
    if (age > 280u)
        return;
    fl = (age < 70u) || (age > 120u && age < 170u) || (age > 210u && age < 240u);
    while (y < SC_HZ + 6) {
        int nx, ny = y + 5 + (int)(h & 3u);
        h = sc_hash(h + (uint32_t)step);
        nx = x + (int)(h % 15u) - 7;
        if (fl) {
            fb_line(x, y, nx, ny, C_BOLT);
            fb_line(x + 1, y, nx + 1, ny, C_BOLT2);
            fb_line(x - 1, y, nx - 1, ny, C_BOLT2);
        } else
            fb_line(x, y, nx, ny, C_BOLT2);
        if (!br && y > 24 && (h & 7u) == 0u) {
            br = 1;
            bx = nx;
            by = ny;
        }
        x = nx;
        y = ny;
        step++;
    }
    if (br && fl) {                                                   /* the branch */
        int k;
        for (k = 0; k < 5; k++) {
            int nx = bx + 5 + (int)(sc_hash(h + (uint32_t)k) % 6u) - 1, ny = by + 7;
            fb_line(bx, by, nx, ny, C_BOLT);
            bx = nx;
            by = ny;
        }
    }
}

/* the sky's flash as a bolt strikes: a dithered brightening of the sky and a ring of light at the horizon */
static void sc_glow_sky(uint32_t now)
{
    int j, x, y;
    if (!sg.was_on && sg.rise == 0)
        return;
    for (j = 0; j < 3; j++) {
        uint32_t age = now - sg.bolt[j].t0;
        if (sg.bolt[j].t0 == 0u || age > 280u)
            continue;
        if (age < 90u)
            for (y = 0; y < SC_HZ; y += 2)
                for (x = (y >> 1) & 1; x < FB_W; x += 2)
                    if (fb[y * FB_W + x] >= C_S0 && fb[y * FB_W + x] <= C_S5)
                        fb_pset(x, y, C_BOLT2);
        sc_bolt(sg.bolt[j].x, sg.bolt[j].seed, age);
    }
}

/* the aura: dithered rings with flames licking upward, sparks climbing, and one shockwave when it begins */
static void sc_disc(int cx, int cy, int r, uint8_t c, int dither, uint32_t salt, uint32_t now)
{
    int y;
    for (y = -r; y <= r; y++) {
        int yy = cy + y, w, jit, x;
        if (yy < 0 || yy >= SC_H)
            continue;
        jit = (int)(sc_hash(salt + (uint32_t)y * 31u + (now >> 6)) % 5u) - 2;
        w = sc_isqrt(r * r - y * y) + jit;
        if (w <= 0)
            continue;
        if (!dither)
            fb_hline(cx - w, yy, 2 * w + 1, c);
        else
            for (x = cx - w + ((yy + 1) & 1); x <= cx + w; x += 2)
                fb_pset(x, yy, c);
    }
}

static void sc_aura(int cx, int cy, uint32_t now)
{
    int R = 25 + (sg.rise * 14 >> 8) + (sc_tri((int)(now >> 3)) >> 6), i, k;
    int pw = 60 + sk_s.glo[1] / 2;
    R = R * pw / 90;
    sc_disc(cx, cy, R + 4, C_A0, 1, 11u, now);
    sc_disc(cx, cy, R, C_A0, 0, 23u, now);
    sc_disc(cx, cy, R * 3 / 4, C_A1, 0, 37u, now);
    sc_disc(cx, cy, R / 2, C_A2, 0, 41u, now);
    /* flames: tongues off the top and the sides that lick and flicker */
    for (i = -R; i <= R; i += 3) {
        uint32_t h = sc_hash((uint32_t)(i + 100) * 17u + (now >> 5));
        int len = 4 + (int)(h % 9u) + (R >> 2), edge = sc_isqrt(R * R - i * i), top = cy - edge;
        for (k = 0; k < len; k++) {
            int yy = top - k, xx = cx + i + (int)((sc_tri((int)(now >> 2) + i * 9 + k * 30) - 128) >> 6);
            if (yy < 0)
                break;
            fb_pset(xx, yy, k < len / 2 ? C_A1 : C_A0);
            if (k < len / 3)
                fb_pset(xx + 1, yy, C_A2);
        }
    }
    /* sparks rise from the ball */
    for (i = 0; i < 14; i++) {
        int ph = (int)((now / 24u + (uint32_t)i * 29u) % 90u);
        int xx = cx + (int)(sc_hash((uint32_t)i * 53u + (now / 2200u)) % (uint32_t)(2 * R)) - R + ((sc_tri(ph * 7 + i * 40) - 128) >> 5);
        int yy = cy + R / 2 - ph * 2;
        if (yy >= 0 && yy < SC_H)
            fb_pset(xx, yy, ph < 50 ? C_A3 : C_A1);
    }
}

static void sc_shock(int cx, int cy, uint32_t age)
{
    int r, y, x;
    if (age > 900u)
        return;
    r = (int)(age * 170u / 900u);
    for (y = -r; y <= r; y += 1) {
        int w = sc_isqrt(r * r - y * y), yy = cy + y;
        if (yy < 0 || yy >= SC_H)
            continue;
        fb_pset(cx - w, yy, C_A3);
        fb_pset(cx + w, yy, C_A3);
        if (age < 600u) {
            fb_pset(cx - w + 1, yy, C_A2);
            fb_pset(cx + w - 1, yy, C_A2);
        }
    }
    for (x = -r; x <= r; x++) {
        int h = sc_isqrt(r * r - x * x), xx = cx + x;
        if (xx < 0 || xx >= FB_W)
            continue;
        if (cy - h >= 0 && cy - h < SC_H)
            fb_pset(xx, cy - h, C_A3);
        if (cy + h >= 0 && cy + h < SC_H)
            fb_pset(xx, cy + h, C_A3);
    }
}

/* the figure, risen: hair swept up, arms out, scarf streaming; he floats on a slow sway */
static void sc_glow_figure(uint32_t now)
{
    int cx = SC_PX - 4, lift = sg.rise * 42 >> 8, bob = (sc_tri((int)(now >> 3)) - 128) >> 6;
    int py = SC_PY - lift + (lift > 30 ? bob : 0), i, fl = sc_tri((int)(now >> 3)) >> 7;
    int cy = py - 16;
    uint32_t age = now - sg.t_on;
    if (sg.was_on)
        sc_shock(cx, SC_PY - 16, age);
    sc_aura(cx, cy, now);
    if (sg.rise < 70) {                                             /* getting up */
        fb_rect(cx - 3, py - 7, 7, 7, C_F0);
        fb_rect(cx - 3, py - 19, 7, 12, C_F0);
        fb_rect(cx - 3, py - 25, 6, 6, C_F0);
    } else {
        int leg = lift > 30 ? 8 : 10;
        fb_rect(cx - 3, py - leg, 3, leg, C_F0);                    /* the legs hang a little apart */
        fb_rect(cx + 1, py - leg, 3, leg, C_F0);
        fb_rect(cx - 3, py - 22, 7, 12, C_F0);
        fb_rect(cx - 3, py - 28, 6, 6, C_F0);
        fb_pset(cx + 1, py - 25, C_A3);                             /* the eyes burn */
        fb_pset(cx - 1, py - 25, C_A3);
        /* the arms out and a little down, fists clenched */
        sc_arm(cx - 2, py - 20, cx - 12, py - 15 + fl, C_F1);
        sc_arm(cx + 3, py - 20, cx + 13, py - 15 + fl, C_F1);
        fb_rect(cx - 14, py - 16 + fl, 3, 3, C_F0);
        fb_rect(cx + 12, py - 16 + fl, 3, 3, C_F0);
    }
    fb_hline(cx - 3, py - 22, 7, C_SCARF);
    for (i = 1; i < 8; i++)
        fb_pset(cx - 3 - i, py - 22 - (i >> 1) + fl, C_SCARF);        /* the scarf streams up and back in the updraft */
    /* the hair: spikes swept upward, flickering gold */
    for (i = 0; i < 5; i++) {
        uint32_t h = sc_hash((uint32_t)i * 7u + (now >> 6));
        int hh = 4 + (int)(h % 4u);
        fb_vline(cx - 3 + i, py - 29 - hh, hh + 1, h & 1u ? C_A2 : C_A3);
    }
}
