/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* SKYLYNNE's five remaining worlds. Included by sk_worlds.c (so everything of sk_scene.c is in scope).
 *
 *   3 Columbia     floating islands with white neoclassical buildings in a pale sky, a sea of cloud below; an airship
 *   4 Babel        a tower of tiers and a spiral ramp that goes up through the clouds out of the picture; torches, birds
 *   5 Lothlorien   huge golden-leaved trunks with flets, lamps and rope bridges, a river; leaves fall, a boat passes
 *   6 Yharnam      Victorian rooftops, spires and a bell tower in fog, a blood-red moon; gas lamps; crows
 *   7 Minas Tirith a white city in seven walled tiers up a mountain with a tower and flags; blossom drifts
 *
 * The same roles as the other worlds: voice 1 the light (rays), voice 2 the sun or moon's glow, voice 3 what crosses the
 * sky (airship, birds, a boat, crows), voice 4 the windows, voice 5 the haze, voice 6 the swell. The figure always sits on
 * the left: on the edge of a floating island, a ziggurat step, a mossy root, a slate roof, a battlement. */

/* birds in a loose V crossing the sky in about 9 s (voice 3 sets them off) */
static void sc_birds(uint32_t now, int y0, uint8_t c)
{
    uint32_t age = now - sc.hit[2];
    int p, i;
    if (age > 9000u)
        return;
    p = (int)(age * 256u / 9000u);
    for (i = 0; i < 7; i++) {
        int x = -20 + p * 280 / 256 - (i < 4 ? i * 7 : (i - 3) * 7), y = y0 + (i < 4 ? i * 2 : (i - 3) * 2 + 1) + (sc_tri((int)(now >> 5) + i * 40) >> 6);
        int up = ((now >> 7) + (uint32_t)i) & 1u;
        fb_pset(x, y, c);
        fb_pset(x - 1, y + (up ? -1 : 1), c);
        fb_pset(x + 1, y + (up ? -1 : 1), c);
    }
}

/* ============================================================ 3 Columbia === */
static void sc_cloud(int cx, int cy, int w, uint8_t c)
{
    int i;
    for (i = -w; i <= w; i += 3) {
        int r = 3 + (w - (i < 0 ? -i : i)) / 3;
        fb_disc(cx + i, cy - (r >> 2), r > 7 ? 7 : r, c);
    }
    fb_hline(cx - w - 2, cy + 3, 2 * w + 5, c);
}

static void sc_island(int x, int y, int w, int dep, uint32_t k, int lit, uint32_t now)
{
    int i, h = 8 + dep * 5;
    for (i = 0; i < w; i++) {                                           /* the rock hangs below in a cone */
        int hh = h - (h * (i < w / 2 ? w / 2 - i : i - w / 2) * 2 / w) + (int)(sc_hash(k + (uint32_t)i) % 3u);
        if (hh > 0)
            fb_rect(x + i, y + 1, 1, hh, (i & 3) == 0 ? C_F1 : C_F0);
    }
    fb_hline(x, y, w, C_F2);                                            /* the lit lip */
    fb_hline(x, y - 1, w, C_F1);
    for (i = 3; i < w - 3; i += 6 + (int)((k >> (i & 7)) & 3u)) {       /* buildings: a block, columns, a pediment or a dome */
        uint32_t q = sc_hash(k * 7u + (uint32_t)i);
        int bh = 7 + (int)(q % 12u), bw = 5 + (int)((q >> 5) % 5u), j;
        fb_rect(x + i, y - 1 - bh, bw, bh, C_F2);
        fb_vline(x + i + bw - 1, y - 1 - bh, bh, C_F1);
        if ((q >> 9) % 3u == 0u)
            fb_disc(x + i + bw / 2, y - 1 - bh, bw / 2 + 1, C_F2);       /* a dome */
        else if ((q >> 9) % 3u == 1u)
            for (j = 0; j < bw / 2 + 1; j++)
                fb_hline(x + i + j, y - 2 - bh - j, bw - 2 * j < 1 ? 1 : bw - 2 * j, C_F2);
        else
            fb_vline(x + i + bw / 2, y - 6 - bh, 5, C_F2);               /* a spire */
        for (j = y - bh + 1; j < y - 2; j += 3)
            if ((int)(sc_hash(q + (uint32_t)j) % 100u) < lit)
                fb_pset(x + i + 1 + (int)((q >> 14) & 1u), j, C_N0);
    }
    (void)now;
}

static void sc_columbia(uint32_t now)
{
    int e = sc_e[3], lit = 28 + (e * 40 >> 8) + (sk_s.mac[0] >> 3), i;
    uint32_t t = now >> 8;
    for (i = 0; i < FB_W; i++)
        sc_hmap[i] = 0;
    sc_cloud(34 + (int)((t >> 3) % 300u) - 40, 22, 14, C_MIST);        /* drifting cloud puffs, high and slow */
    sc_cloud(190 - (int)((t >> 4) % 260u) + 40, 14, 12, C_MIST);
    sc_island(116, 38, 46, 1, 0x1Au, lit, now);                         /* far, small islands in the haze */
    sc_island(196, 26, 36, 1, 0x2Bu, lit, now);
    /* the sky-line: a thin cable between islands with a car that crawls along it */
    fb_line(160, 36, 198, 30, C_F1);
    {
        int u = (int)((now >> 6) % 120u);
        int cx = 160 + u * 38 / 120, cy = 36 - u * 6 / 120 + 2;
        fb_rect(cx - 1, cy, 3, 2, C_F0);
        fb_pset(cx, cy + 1, C_N0);
    }
    sc_island(126, 74, 84, 2, 0x31u, lit + 6, now);                     /* the great island with the temple */
    fb_rect(150, 38, 34, 28, C_F2);                                     /* a colonnaded hall on it, columns and a dome */
    fb_vline(150, 38, 28, C_F1);
    fb_disc(167, 38, 12, C_F2);
    for (i = 153; i < 183; i += 5) {
        fb_vline(i, 44, 22, C_F1);
        fb_pset(i + 1, 44, C_F2);
    }
    fb_rect(158, 46, 18, 20, C_N0);                                     /* its great door: gold light */
    fb_checker(158, 46, 18, 20, C_N2);
    fb_vline(167, 26, 12, C_F2);                                        /* lantern spire over the dome */
    fb_pset(167, 24, C_N2);
    for (i = 150; i < 184; i++)
        sc_hmap[i] = 60;
    sc_island(206, 96, 34, 0, 0x41u, lit, now);                         /* near, low islands: the foot of the city */
    sc_island(100, 98, 30, 0, 0x52u, lit, now);
    /* the airship: voice 3 */
    {
        uint32_t age = now - sc.hit[2];
        if (age < 16000u) {
            int x = -30 + (int)(age * 300u / 16000u), y = 54 + (sc_tri((int)(now >> 6)) >> 6), j;
            for (j = -3; j <= 3; j++)
                fb_hline(x - 12 + (j * j) / 3, y + j, 24 - 2 * ((j * j) / 3), C_F2);
            fb_hline(x - 10, y + 4, 20, C_F1);
            fb_rect(x - 3, y + 5, 7, 3, C_F0);
            fb_pset(x - 1, y + 6, C_N0);
            fb_pset(x + 2, y + 6, C_N0);
            fb_vline(x - 14, y - 2, 4, ((now >> 5) & 1u) ? C_F0 : C_F1);       /* the propeller blurs */
        }
    }
    {
        int breathe = 2 + (sc_tri((int)(now >> 6)) >> 6);                /* glow: a pale gold haze over the great island */
        int x;
        for (x = 120; x < 214; x++) {
            int g = 5 + breathe + (e >> 6);
            for (i = 1; i <= g; i++) {
                int y = 66 - (sc_hmap[x] ? 24 : 0) - i;
                if (y > 0 && ((x + y) & 1) == 0 && i < g / 2 + 2 && fb[y * FB_W + x] >= C_S0 && fb[y * FB_W + x] <= C_S5)
                    fb_pset(x, y, C_G0);
            }
        }
    }
}

static void sc_col_ledge(uint32_t now)
{
    int x, y;
    (void)now;
    for (y = SC_PY; y < SC_H; y++) {                                    /* the island he sits on: a cone of rock below its lip */
        int xr = SC_EDGE - (y - SC_PY) * SC_EDGE / (SC_H - SC_PY + 18);
        fb_rect(0, y, xr + 1, 1, ((y + 3) % 7) == 0 ? C_F1 : C_F0);
        if (y > SC_PY + 4 && (sc_hash((uint32_t)y * 5u) & 3u) == 0u)
            fb_pset(xr, y, C_F1);
    }
    fb_hline(0, SC_PY, SC_EDGE + 2, C_F2);
    for (x = 0; x < SC_EDGE; x += 11)
        fb_vline(x, SC_PY + 1, 5, C_F1);                                /* the paving's joints */
    for (x = 30; x < 94; x += 6) {                                      /* a balustrade behind him, and a lamp post */
        fb_rect(x, SC_PY - 8, 2, 8, C_F1);
        fb_pset(x, SC_PY - 8, C_F2);
    }
    fb_hline(28, SC_PY - 9, 68, C_F2);
    fb_vline(14, SC_PY - 30, 30, C_F0);
    fb_hline(12, SC_PY - 31, 5, C_F0);
    fb_pset(14, SC_PY - 33, C_N2);
    fb_checker(11, SC_PY - 36, 7, 7, C_N0);
}

static void sc_petals(uint32_t now, uint8_t c1, uint8_t c2, int n, int dx, int dy)
{
    int i;
    for (i = 0; i < n; i++) {
        uint32_t k = sc_hash((uint32_t)i * 7919u + 0x2Bu);
        int sx = (int)(k % 300u), sy = (int)((k >> 9) % 180u);
        int x = (sx + dx * (int)((now >> 6) + (k >> 4)) / 16 + ((sc_tri((int)(now >> 5) + (int)(k >> 16)) - 128) >> 4)) % 252 - 6;
        int y = (sy + dy * (int)((now >> 6) + (k >> 4)) / 16) % 190;
        if (x < 0)
            x += 252;
        if (y < 0)
            y += 190;
        if (x > 82 && x < 112 && y > 108 && y < 150)
            continue;
        fb_pset(x, y, (i & 1) ? c1 : c2);
        if ((i % 3) == 0)
            fb_pset(x + 1, y, c2);
    }
}

/* ============================================================== 4 Babel === */
static void sc_babel(uint32_t now)
{
    int e = sc_e[3], lit = 22 + (e * 40 >> 8) + (sk_s.mac[0] >> 3), k, x, y;
    for (x = 0; x < FB_W; x++)
        sc_hmap[x] = 0;
    /* the far shore: low brick towns and a smaller, older tower */
    for (x = 0; x < 120; x += 9) {
        int h = 5 + (int)(sc_hash((uint32_t)x * 3u + 7u) % 9u);
        fb_rect(x, SC_HZ - h, 8, h, C_F2);
        fb_pset(x + 2, SC_HZ - h + 2, C_N1);
    }
    /* the tower: tiers each narrower than the last, climbing off the top of the picture */
    for (k = 0; k < 9; k++) {
        int y1 = SC_HZ - k * 13, tw = 90 - k * 7, cx = 192, tx = cx - tw / 2 + (k * 2);
        int th = 13;
        fb_rect(tx, y1 - th, tw, th, C_F1);
        fb_vline(tx, y1 - th, th, C_F2);
        fb_vline(tx + tw - 1, y1 - th, th, C_F0);
        fb_hline(tx - 1, y1 - th, tw + 2, C_F2);                         /* the cornice */
        fb_hline(tx - 1, y1 - th + 1, tw + 2, C_F0);
        for (x = tx + 4; x < tx + tw - 4; x += 8) {                      /* arches in a row: dark, a torch in some */
            fb_rect(x, y1 - 8, 4, 8, C_F0);
            fb_pset(x + 1, y1 - 9, C_F0);
            fb_pset(x + 2, y1 - 9, C_F0);
            if ((int)(sc_hash((uint32_t)(x * 31 + k * 7)) % 100u) < lit)
                fb_rect(x + 1 + (int)((x >> 3) & 1), y1 - 5, 1, 3, ((x >> 3) & 3) ? C_N0 : C_N1);
        }
        for (x = tx; x < tx + tw; x++)
            if (SC_HZ - y1 + th > sc_hmap[x])
                sc_hmap[x] = (int8_t)(SC_HZ - y1 + th > 60 ? 60 : SC_HZ - y1 + th);
    }
    /* the spiral ramp: a slanted light band across each tier, alternating sides */
    for (k = 0; k < 9; k++) {
        int y1 = SC_HZ - k * 13, tw = 90 - k * 7, tx = 192 - tw / 2 + k * 2;
        if (k & 1)
            fb_line(tx + tw - 2, y1 - 1, tx + 2, y1 - 13, C_F3);
        else
            fb_line(tx + 2, y1 - 1, tx + tw - 2, y1 - 13, C_F3);
    }
    /* scaffolding at the top and cranes; torches flicker */
    for (y = 0; y < 30; y += 10) {
        fb_vline(214, y, 10, C_F0);
        fb_hline(206, y, 16, C_F0);
    }
    for (k = 0; k < 4; k++) {
        int fx = 150 + k * 22, fy = SC_HZ - 3;
        if (((now >> 7) + (uint32_t)k) % 3u)
            fb_pset(fx, fy, C_N0);
        fb_pset(fx, fy - 1 - (int)(((now >> 6) + (uint32_t)k) & 1u), C_N1);
    }
    /* the quay and the brick-makers' fires */
    fb_hline(104, SC_HZ - 1, 136, C_F0);
    {
        int breathe = 2 + (sc_tri((int)(now >> 6)) >> 6);                /* a gold glow along the tower's flank */
        for (x = 100; x < FB_W - 1; x++) {
            int g = 4 + breathe + (e >> 6);
            for (y = 1; y <= g; y++) {
                int yy = SC_HZ - sc_hmap[x] - y;
                if (yy > 0 && ((x + yy) & 1) == 0 && fb[yy * FB_W + x] >= C_S0 && fb[yy * FB_W + x] <= C_S5)
                    fb_pset(x, yy, C_G0);
            }
        }
    }
}

static void sc_bab_ledge(uint32_t now)
{
    int x, y;
    for (x = 0; x < SC_EDGE; x++) {
        int ty = SC_PY + (x < 40 ? (40 - x) / 5 : 0);
        fb_rect(x, ty, 1, SC_H - ty, C_F0);
        fb_pset(x, ty, C_F2);
    }
    for (y = SC_PY + 5; y < SC_H; y += 6) {                              /* brick courses */
        int off = ((y - SC_PY) / 6 & 1) * 5;
        for (x = off; x < SC_EDGE; x += 10)
            fb_vline(x, y, 6, C_F1);
        fb_hline(0, y, SC_EDGE, C_F1);
    }
    fb_rect(SC_EDGE, SC_PY + 1, 3, SC_H - SC_PY - 1, C_F2);
    for (y = 0; y < 5; y++)                                              /* a small ziggurat of steps behind and left of him */
        fb_rect(2 + y * 6, SC_PY - 6 - (4 - y) * 4 + 4 * y - 4, 40 - y * 12, 4 + (4 - y) * 0 + 4, C_F1);
    fb_rect(0, SC_PY - 6, 44, 6, C_F1);
    fb_rect(6, SC_PY - 12, 32, 6, C_F1);
    fb_rect(12, SC_PY - 18, 20, 6, C_F1);
    fb_hline(0, SC_PY - 6, 44, C_F2);
    fb_hline(6, SC_PY - 12, 32, C_F2);
    fb_hline(12, SC_PY - 18, 20, C_F2);
    fb_vline(72, SC_PY - 26, 26, C_F0);                                  /* a ladder and a rope */
    fb_vline(76, SC_PY - 26, 26, C_F0);
    for (y = SC_PY - 24; y < SC_PY; y += 5)
        fb_hline(72, y, 5, C_F0);
    sc_bush(186, SC_H, 28, 12, 0x51u);
    sc_bush(222, SC_H, 24, 16, 0x61u);
    for (x = 150; x < 236; x += 7)                                       /* reeds */
        fb_vline(x + ((sc_tri((int)(now >> 5) + x * 6) - 128) >> 6), SC_H - 14 - (int)(sc_hash((uint32_t)x) % 8u), 14, C_F0);
}

/* ========================================================== 5 Lothlorien === */
static void sc_mallorn(int cx, int w, int by, int topy, uint32_t k, int lit, uint32_t now, int near)
{
    int y, j, x;
    uint8_t body = near ? C_F1 : C_F2;
    for (y = topy; y < by; y++) {                                        /* a trunk, flaring a little at the foot */
        int ww = w + (y - topy < 8 ? 0 : 0) + (by - y < 10 ? (10 - (by - y)) / 2 : 0);
        fb_hline(cx - ww / 2, y, ww, body);
        fb_pset(cx - ww / 2, y, C_F2);
        fb_pset(cx + ww / 2 - 1, y, C_F0);
        if (near && ((y + (int)(k & 3u)) % 5) == 0)
            fb_pset(cx - ww / 4 + (int)(sc_hash(k + (uint32_t)y) % 3u), y, C_F0);       /* bark */
    }
    for (j = 0; j < 3; j++) {                                            /* flets: a platform, rails, a hut with a lit window */
        int fy = topy + 28 + j * 26 + (int)((k >> (j * 3)) & 7u), fw = w + 16 + (j & 1) * 6, fx = cx - fw / 2;
        if (fy > by - 8 || !near)
            continue;
        fb_rect(fx, fy, fw, 2, C_F0);
        fb_hline(fx, fy, fw, C_F2);
        for (x = fx; x < fx + fw; x += 4)
            fb_vline(x, fy - 4, 4, C_F0);
        fb_hline(fx, fy - 4, fw, C_F0);
        if (j == 1) {
            fb_rect(cx + 4, fy - 10, 9, 8, C_F1);
            for (x = 0; x < 5; x++)
                fb_hline(cx + 4 + x, fy - 11 - x, 9 - 2 * x < 1 ? 1 : 9 - 2 * x, C_F0);
            fb_rect(cx + 7, fy - 8, 3, 4, (int)(sc_hash(k + 77u) % 100u) < lit + 40 ? C_N0 : C_F0);
        }
        fb_pset(fx + 2, fy + 3, C_N0);                                   /* a hanging lamp */
        if (((now >> 8) + (uint32_t)j) % 7u)
            fb_pset(fx + 2, fy + 4, C_N1);
    }
}

static void sc_loth(uint32_t now)
{
    int e = sc_e[3], lit = 24 + (e * 40 >> 8) + (sk_s.mac[0] >> 3), i, x, y;
    for (x = 0; x < FB_W; x++)
        sc_hmap[x] = 0;
    /* the golden canopy: clusters of leaf over the whole top of the picture, dithered gold over the sky */
    for (i = 0; i < 40; i++) {
        uint32_t k = sc_hash((uint32_t)i * 977u + 31u);
        int cx = (int)(k % 250u), cy = 4 + (int)((k >> 9) % 40u), r = 8 + (int)((k >> 16) % 10u);
        for (y = -r; y <= r; y++)
            for (x = -r; x <= r; x++)
                if (x * x + y * y <= r * r && ((x + y + cx + cy) & 1) == 0)
                    fb_pset(cx + x, cy + y, (k >> 20) & 1u ? C_N1 : C_N0);
    }
    /* far trunks in the haze, then the near ones with their flets, and rope bridges between */
    for (i = 0; i < 4; i++)
        sc_mallorn(14 + i * 26 + (i & 1) * 6, 8 + (i & 1) * 2, SC_HZ, 12 + i * 6, 0x11u + (uint32_t)i, lit, now, 0);
    {
        static const uint8_t TX[4] = {122, 158, 196, 230}, TW[4] = {16, 20, 14, 12};
        for (i = 0; i < 4; i++) {
            uint32_t k = sc_hash((uint32_t)i * 313u + 9u);
            sc_mallorn(TX[i], TW[i], SC_HZ, -4 + (int)(k % 8u), k, lit, now, 1);
            for (x = TX[i] - TW[i] / 2; x < TX[i] + TW[i] / 2; x++)
                sc_hmap[x] = 30;
        }
        for (i = 0; i < 3; i++) {                                        /* a bridge: a sagging rope */
            int x0 = TX[i] + TW[i] / 2 + 8, x1 = TX[i + 1] - TW[i + 1] / 2 - 8, by = 54 + i * 26 + 6;
            for (x = x0; x <= x1; x++) {
                int u = (x - x0) * 64 / (x1 - x0 + 1), sag = (u * (64 - u)) >> 8;
                fb_pset(x, by + sag, C_F0);
                if (((x - x0) & 3) == 0)
                    fb_pset(x, by + sag - 2, C_F0);
            }
        }
    }
    /* the river bank, and a golden haze on the water's edge */
    fb_hline(0, SC_HZ - 1, FB_W, C_F0);
    {
        int breathe = 2 + (sc_tri((int)(now >> 6)) >> 6);
        for (x = 0; x < FB_W; x++)
            for (i = 1; i <= 5 + breathe; i++) {
                y = SC_HZ - 2 - i;
                if (((x + y) & 1) == 0 && i < 4 && fb[y * FB_W + x] >= C_S0 && fb[y * FB_W + x] <= C_S5)
                    fb_pset(x, y, C_G0);
            }
    }
}

static void sc_loth_ledge(uint32_t now)
{
    int x, y, k;
    for (x = 0; x < SC_EDGE; x++) {
        uint32_t q = sc_hash((uint32_t)(x >> 2) + 0x61u);
        int ty = SC_PY + (x < 30 ? (30 - x) / 3 : 0) + (int)(q % 3u);
        fb_rect(x, ty, 1, SC_H - ty, C_F0);
        fb_pset(x, ty, C_F2);
        if (x & 1)
            fb_pset(x, ty - 1, C_F1);                                    /* moss on the lip */
    }
    fb_rect(SC_EDGE, SC_PY + 1, 3, SC_H - SC_PY - 1, C_F2);
    for (k = 0; k < 4; k++) {                                            /* roots curling down the face */
        int rx = 50 + k * 14;
        for (y = SC_PY; y < SC_H; y++) {
            int sw = ((sc_tri(y * 5 + k * 80) - 128) >> 5);
            fb_pset(rx + sw, y, C_F1);
            fb_pset(rx + sw + 1, y, C_F1);
        }
    }
    fb_checker(40, SC_PY + 12, 66, 20, C_F1);
    for (k = 0; k < 5; k++) {                                            /* ferns on the left */
        int fx = 6 + k * 9;
        for (x = -6; x <= 6; x++)
            fb_pset(fx + x, SC_PY - 2 - 8 + (x * x) / 6 - (k & 1) * 2, C_F0);
        fb_vline(fx, SC_PY - 10, 10, C_F0);
    }
    fb_vline(70, SC_PY - 34, 24, C_F0);                                   /* a branch with a hanging lantern at his shoulder */
    fb_hline(70, SC_PY - 34, 18, C_F0);
    fb_vline(86, SC_PY - 34, 6, C_F0);
    fb_rect(84, SC_PY - 28, 5, 6, C_F0);
    fb_rect(85, SC_PY - 27, 3, 4, ((now >> 8) % 11u) ? C_N0 : C_N1);
    sc_bush(190, SC_H, 30, 12, 0x51u);
    sc_bush(226, SC_H, 20, 20, 0x61u);
}

/* a boat on the river: a swan-prowed hull and a lantern */
static void sc_boat(uint32_t now)
{
    uint32_t age = now - sc.hit[2];
    int x, y = SC_HZ + 10, i;
    if (age > 20000u)
        return;
    x = -24 + (int)(age * 290u / 20000u);
    for (i = 0; i < 14; i++)
        fb_vline(x + i, y + (i < 3 ? 2 - i : i > 10 ? i - 10 : 0), 3, C_F0);
    fb_pset(x + 14, y - 1, C_F0);
    fb_pset(x + 15, y - 3, C_F0);
    fb_pset(x + 16, y - 4, C_F0);
    fb_vline(x + 7, y - 6, 6, C_F0);
    fb_pset(x + 7, y - 7, ((now >> 7) & 7u) ? C_N0 : C_N1);
    fb_checker(x + 5, y - 9, 5, 5, C_G0);
    fb_hline(x - 2, y + 4, 20, C_W0);
}

/* ========================================================== 6 Yharnam === */
static void sc_roof(int x, int w, int h, uint32_t k, int lit)
{
    int i, j, base = SC_HZ - 3;
    fb_rect(x, base - h, w, h, C_F1);                                    /* the wall, a steep gable above, chimneys */
    fb_vline(x, base - h, h, C_F2);
    for (i = 0; i < w / 2 + 1; i++)
        fb_hline(x + i, base - h - 1 - i * 3 / 2, w - 2 * i < 1 ? 1 : w - 2 * i, i ? C_F1 : C_F2);
    if ((k >> 3) & 1u) {
        fb_rect(x + w - 4, base - h - 10, 2, 6, C_F0);                   /* a chimney */
        fb_hline(x + w - 5, base - h - 11, 4, C_F2);
    }
    for (j = base - h + 3; j < base - 3; j += 4)
        for (i = x + 2; i < x + w - 2; i += 4) {
            uint32_t q = sc_hash(k * 5u + (uint32_t)(i * 7 + j));
            if ((int)(q % 100u) < lit)
                fb_rect(i, j, 1, 2, ((q >> 9) & 3u) == 0u ? C_N1 : C_N0);
            else if ((q >> 12) % 4u == 0u)
                fb_pset(i, j, C_F0);
        }
}

static void sc_yharnam(uint32_t now)
{
    int e = sc_e[3], lit = 14 + (e * 40 >> 8) + (sk_s.mac[0] >> 4), x, i, b = 0;
    for (x = 0; x < FB_W; x++)
        sc_hmap[x] = 0;
    for (x = 6; x < 112; x += 24 + (int)(sc_hash((uint32_t)x + 3u) % 9u)) {    /* far spires in the fog on the left */
        int h = 18 + (int)(sc_hash((uint32_t)x * 7u + 1u) % 18u);
        fb_rect(x - 3, SC_HZ - h, 6, h, C_F2);
        sc_spire(x, SC_HZ - h, 8, 16, C_F2, C_F2);
    }
    x = 104;
    while (x < FB_W - 2) {                                               /* the rooftops, shoulder to shoulder */
        uint32_t k = sc_hash((uint32_t)b * 4177u + 0x91u);
        int w = 9 + (int)(k % 9u), h = 10 + (int)((k >> 6) % 16u);
        if (x > 150 && x < 196) {
            x = 196;
            continue;
        }
        if (x + w > FB_W - 1)
            w = FB_W - 1 - x;
        if (w >= 5)
            sc_roof(x, w, h, k, lit);
        for (i = 0; i < w; i++)
            sc_hmap[x + i] = (int8_t)(h + 8);
        x += w + (int)((k >> 16) % 2u);
        b++;
    }
    /* the cathedral between: a broad nave, a rose window, a spire, buttresses; darker and narrower than a nave in gold */
    fb_rect(152, SC_HZ - 36, 44, 33, C_F1);
    fb_vline(152, SC_HZ - 36, 33, C_F2);
    for (i = 0; i < 22; i++)
        fb_hline(152 + i, SC_HZ - 37 - i * 14 / 22, 44 - 2 * i < 1 ? 1 : 44 - 2 * i, i ? C_F1 : C_F2);
    for (x = 160; x < 190; x += 8)
        sc_lancet(x, SC_HZ - 30, 4, 18, (int)(sc_hash((uint32_t)x * 9u) % 100u) < lit + 30 ? C_N0 : C_F0);
    sc_spire(174, SC_HZ - 52, 10, 36, C_F1, C_F2);
    fb_vline(174, SC_HZ - 90, 6, C_F2);
    fb_hline(172, SC_HZ - 88, 5, C_F2);
    for (i = 0; i < 16; i++)
        fb_pset(148 - (i * 6 >> 4), SC_HZ - 4 - i, C_F2);
    for (x = 152; x < 196; x++)
        sc_hmap[x] = 50;
    for (x = 112; x < FB_W; x += 29) {                                   /* gas lamps along the quay */
        fb_vline(x, SC_HZ - 9, 8, C_F0);
        fb_pset(x, SC_HZ - 10, (((now >> 7) + (uint32_t)x) % 13u) ? C_N0 : C_N1);
        fb_checker(x - 2, SC_HZ - 13, 5, 5, C_G0);
    }
}

static void sc_moon_blood(uint32_t now)                                  /* a large dim moon low on the left with a halo */
{
    int cx = 56, cy = 44, R = 15, i, j, e = sc_e[1], gl = 6 + e * 6 / 255;
    (void)now;
    for (j = -R - gl; j <= R + gl; j++)
        for (i = -R - gl; i <= R + gl; i++) {
            int d2 = i * i + j * j;
            if (d2 <= R * R)
                fb_pset(cx + i, cy + j, d2 > (R - 2) * (R - 2) ? C_MOON2 : ((i * 3 + j * 5) % 11 == 0 ? C_MOON2 : C_MOON));
            else if (d2 <= (R + gl) * (R + gl) && ((i + j) & 1) == 0)
                fb_pset(cx + i, cy + j, C_S5);
        }
}

static void sc_fogbank(uint32_t now)
{
    int band, x, y, e = sc_e[4];
    for (band = 0; band < 3; band++) {
        int y0 = SC_HZ - 26 + band * 9, h = 5 + (e >> 6);
        for (y = 0; y < h; y++)
            for (x = (y + band) & 1; x < FB_W; x += 2)
                if (sc_tri(x * 2 + (int)(now >> (5 - band)) * 2 + band * 70) > 120 + y * 12)
                    fb_pset(x, y0 + y, C_MIST);
    }
}

static void sc_yh_ledge(uint32_t now)
{
    int x, y;
    for (x = 0; x < SC_EDGE; x++) {                                      /* a slate roof sloping toward the street */
        int ty = SC_PY + (x < 40 ? (40 - x) / 4 : 0);
        fb_rect(x, ty, 1, SC_H - ty, C_F0);
        fb_pset(x, ty, C_F2);
    }
    for (y = SC_PY + 3; y < SC_H; y += 5)                                /* slates: rows of scales */
        for (x = ((y - SC_PY) / 5 & 1) * 3; x < SC_EDGE; x += 6) {
            fb_pset(x, y, C_F1);
            fb_pset(x + 1, y + 1, C_F1);
            fb_pset(x + 2, y + 1, C_F1);
            fb_pset(x + 3, y, C_F1);
        }
    fb_rect(SC_EDGE, SC_PY + 1, 3, SC_H - SC_PY - 1, C_F2);
    fb_rect(10, SC_PY - 26, 14, 26, C_F1);                               /* a chimney stack, a gargoyle on its shoulder */
    fb_vline(10, SC_PY - 26, 26, C_F2);
    fb_rect(8, SC_PY - 28, 18, 3, C_F2);
    fb_rect(26, SC_PY - 12, 8, 6, C_F0);
    fb_pset(33, SC_PY - 13, C_F0);
    fb_pset(34, SC_PY - 12, C_F0);
    fb_pset(28, SC_PY - 13, C_F0);
    fb_pset(31, SC_PY - 13, C_F0);
    for (x = 40; x < 92; x += 5) {                                       /* iron railings along the ridge */
        fb_vline(x, SC_PY - 9, 9, C_F0);
        fb_pset(x, SC_PY - 10, C_F2);
    }
    fb_hline(40, SC_PY - 8, 50, C_F0);
    (void)now;
    sc_bush(190, SC_H, 30, 10, 0x51u);
    sc_bush(228, SC_H, 22, 16, 0x61u);
}

/* ======================================================= 7 Minas Tirith === */
static void sc_minas(uint32_t now)
{
    int e = sc_e[3], lit = 22 + (e * 40 >> 8) + (sk_s.mac[0] >> 3), t, x, y, i;
    for (x = 0; x < FB_W; x++)
        sc_hmap[x] = 0;
    /* the far range, pale, with snow */
    for (x = 0; x < 128; x++) {
        int h = 10 + (sc_tri(x * 3 + 40) >> 4) + (sc_tri(x + 120) >> 5);
        fb_rect(x, SC_HZ - h, 1, h, C_F2);
        if (h > 20)
            fb_pset(x, SC_HZ - h, C_F3);
    }
    /* the mountain: a wedge up to a peak at the right, shaded on its right flank */
    for (x = 112; x < FB_W; x++) {
        int h = x < 196 ? (x - 112) * 92 / 84 : 92 - (x - 196) * 40 / 44, yy;
        if (h < 6)
            h = 6;
        for (yy = 0; yy < h; yy++)
            fb_pset(x, SC_HZ - 1 - yy, x < 190 ? C_F1 : C_F0);
        fb_pset(x, SC_HZ - h, C_F3);
        if (h > 70)
            fb_pset(x, SC_HZ - h + 1, C_F3);
    }
    /* the seven tiers: each a wall with a crenellated top, houses behind it, a gate with a lamp */
    for (t = 0; t < 7; t++) {
        int y1 = SC_HZ - 2 - t * 11, x0 = 120 + t * 10, x1 = FB_W - 1 - (t > 4 ? (t - 4) * 4 : 0);
        fb_rect(x0, y1 - 9, x1 - x0, 9, C_F2);
        fb_hline(x0, y1 - 9, x1 - x0, C_F3);
        for (x = x0; x < x1; x += 3)
            fb_pset(x, y1 - 10, C_F2);
        fb_rect(x1 - 4, y1 - 9, 4, 9, C_F1);
        for (x = x0 + 6; x < x1 - 6; x += 11 + (t & 3)) {                /* houses: blocks with a pitched roof, a window lit */
            uint32_t q = sc_hash((uint32_t)(t * 53 + x));
            int hh = 5 + (int)(q % 6u), ww = 5 + (int)((q >> 6) % 4u);
            fb_rect(x, y1 - 9 - hh, ww, hh, C_F3);
            fb_hline(x - 1, y1 - 10 - hh, ww + 2, C_F1);
            if ((int)(q % 100u) < lit + 30)
                fb_pset(x + 2, y1 - 9 - hh / 2, C_N0);
        }
        fb_rect(x0 + 18 + t * 3, y1 - 6, 4, 6, C_F0);                    /* the gate and its lamp */
        fb_pset(x0 + 19 + t * 3, y1 - 7, C_F0);
        fb_pset(x0 + 19 + t * 3, y1 - 4, (((now >> 8) + (uint32_t)t) % 9u) ? C_N0 : C_N3);
        for (x = x0; x < x1; x++)
            if (SC_HZ - y1 + 10 > sc_hmap[x])
                sc_hmap[x] = (int8_t)(SC_HZ - y1 + 10);
    }
    /* the white tower on the rock's top, its spire, the flags */
    fb_rect(186, SC_HZ - 106, 8, 26, C_F3);
    fb_vline(186, SC_HZ - 106, 26, C_F2);
    fb_vline(193, SC_HZ - 106, 26, C_F1);
    sc_spire(190, SC_HZ - 106, 10, 10, C_F3, C_F3);
    fb_vline(190, SC_HZ - 122, 10, C_F3);
    for (i = 0; i < 10; i++)
        fb_pset(191 + i, SC_HZ - 121 + ((sc_tri((int)(now >> 4) + i * 40) - 128) >> 6) + i / 4, i & 1 ? C_F3 : C_PINK);
    fb_hline(186, SC_HZ - 100, 8, C_N0);
    {
        int breathe = 2 + (sc_tri((int)(now >> 6)) >> 6);
        for (x = 120; x < FB_W - 1; x++) {
            int h = sc_hmap[x] + 8, g = 4 + breathe + (e >> 6);
            for (y = 1; y <= g; y++) {
                int yy = SC_HZ - h - y;
                if (yy > 0 && ((x + yy) & 1) == 0 && y < g / 2 + 1 && fb[yy * FB_W + x] >= C_S0 && fb[yy * FB_W + x] <= C_S5)
                    fb_pset(x, yy, C_G0);
            }
        }
    }
}

static void sc_min_ledge(uint32_t now)
{
    int x, y;
    for (x = 0; x < SC_EDGE; x++) {
        fb_rect(x, SC_PY, 1, SC_H - SC_PY, C_F0);
        fb_pset(x, SC_PY, C_F2);
    }
    for (y = SC_PY + 4; y < SC_H; y += 7) {                              /* white-stone courses, in shadow */
        int off = ((y - SC_PY) / 7 & 1) * 7;
        for (x = off; x < SC_EDGE; x += 14)
            fb_vline(x, y, 7, C_F1);
        fb_hline(0, y, SC_EDGE, C_F1);
    }
    for (x = 0; x < SC_EDGE; x += 8) {                                   /* crenellations: merlons on the rim, he sits in an embrasure */
        if (x > 70 && x < 100)
            continue;
        fb_rect(x, SC_PY - 6, 5, 6, C_F1);
        fb_hline(x, SC_PY - 6, 5, C_F2);
    }
    fb_rect(SC_EDGE, SC_PY + 1, 3, SC_H - SC_PY - 1, C_F2);
    fb_vline(18, SC_PY - 40, 34, C_F0);                                  /* a banner pole with a waving white banner */
    for (y = 0; y < 14; y++)
        for (x = 0; x < 12; x++)
            fb_pset(19 + x + ((sc_tri((int)(now >> 4) + x * 40 + y * 10) - 128) >> 6), SC_PY - 40 + y, x < 2 ? C_F2 : C_F3);
    fb_rect(27, SC_PY - 38, 2, 4, C_F1);
    sc_bush(188, SC_H, 30, 10, 0x51u);
    sc_bush(224, SC_H, 22, 14, 0x61u);
}

/* the world's own scene, dispatched from sc_draw */
static void sc_world2(uint32_t now)
{
    int e0 = sc_e[0];
    switch (sc_world) {
    case 3:
        fb_disc(180, 30, 9, C_MOON);
        sc_rays(now, 180, 30, -40 - (e0 >> 4), 20, 3 + (e0 >> 6), 40 + (e0 >> 2));
        sc_columbia(now);
        break;
    case 4:
        fb_disc(236, 30, 12, C_MOON);
        sc_rays(now, 236, 30, -60, -10 + (e0 >> 4), 3 + (e0 >> 6), 50 + (e0 >> 2));
        sc_mist(now);
        sc_babel(now);
        sc_birds(now, 30, C_F0);
        break;
    case 5:
        sc_rays(now, 150, 0, -20, 30, 3 + (e0 >> 6), 70 + (e0 >> 2));
        sc_loth(now);
        break;
    case 6:
        sc_moon_blood(now);
        sc_yharnam(now);
        sc_fogbank(now);
        sc_birds(now, 28, C_F0);
        break;
    default:
        fb_disc(60, 36, 11, C_MOON);
        sc_rays(now, 60, 36, -10, 40, 3 + (e0 >> 6), 60 + (e0 >> 2));
        sc_mist(now);
        sc_minas(now);
        sc_birds(now, 40, C_F3);
        break;
    }
}
