/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* SKYLYNNE's other worlds: what the figure looks at when the soundscape is not the city. Included by sk_scene.c.
 *
 *   world 1, the cathedral: a gilded gothic nave with spires and a rose window against a low sun, rays fanning out of
 *           it, a sea of cloud below; he sits on a terrace of old stone. Gold motes drift upward.
 *   world 2, the deep: a drowned temple of columns, domes and a gate, ruined and faintly lit by an old machine's aqua
 *           light, shafts of filtered sun from above, marine snow falling, bubbles rising; he sits on a fallen drum
 *           among kelp. Now and then a school of fish.
 *
 * The voices keep their roles (e[0] the rays, e[1] the sun or the snow, e[3] the windows, e[4] the haze, e[5] the swell).
 * Everything here uses the palette's own slots: the neon slots are gold in the cathedral and aqua in the deep, so the far
 * architecture is the one place with colour, as in the city. */

/* a pointed (lancet) window: x, top, width, height */
static void sc_lancet(int x, int y, int w, int h, uint8_t c)
{
    int i;
    fb_rect(x, y + w / 2, w, h - w / 2, c);
    for (i = 0; i < w / 2; i++)
        fb_hline(x + i + 1, y + w / 2 - 1 - i, w - 2 - 2 * i < 1 ? 1 : w - 2 - 2 * i, c);
}

/* a spire: a triangle of base w and height h on a base line, its left edge lit */
static void sc_spire(int cx, int by, int w, int h, uint8_t c, uint8_t edge)
{
    int r;
    for (r = 0; r < h; r++) {
        int ww = w * (h - r) / h;
        if (ww < 1)
            ww = 1;
        fb_hline(cx - ww / 2, by - r, ww, c);
        fb_pset(cx - ww / 2, by - r, edge);
    }
}

/* rays out of (sx, sy): n dotted lines, fanned from a0 to a1 (the x drift per 64 rows), length len */
static void sc_rays(uint32_t now, int sx, int sy, int a0, int a1, int n, int len)
{
    int i, t;
    for (i = 0; i < n; i++) {
        int a = n > 1 ? a0 + (a1 - a0) * i / (n - 1) : a0;
        int sway = (sc_tri((int)(now >> 5) + i * 97) - 128) >> 4;
        for (t = 4; t < len; t++) {
            int x = sx + (a + sway) * t / 64, y = sy + t;
            if (y >= SC_HZ - 2 || y < 0)
                break;
            if (((t + i) & 3) == 0 && ((t >> 2) + i) % 3 != 0)
                fb_pset(x, y, C_G0);
            if (t > 24 && ((t + i) & 7) == 1)
                fb_pset(x + 1, y, C_G1);
        }
    }
}

/* --------------------------------------------------------- the cathedral --- */
static void sc_sun(void)
{
    int e = sc_e[1], cx = 172, cy = 36, R = 11, gl = 7 + e * 8 / 255, i, j, m = R + gl + 1;
    for (j = -m; j <= m; j++)
        for (i = -m; i <= m; i++) {
            int d2 = i * i + j * j;
            if (d2 <= R * R)
                fb_pset(cx + i, cy + j, d2 > (R - 2) * (R - 2) ? C_MOON2 : C_MOON);
            else if (d2 <= (R + gl) * (R + gl) && ((i + j) & 1) == 0 && d2 < (R + gl / 2) * (R + gl / 2))
                fb_pset(cx + i, cy + j, C_S5);
        }
}

static void sc_nave(uint32_t now)
{
    int x, i, e = sc_e[3], lit = 38 + (e * 40 >> 8) + (sk_s.mac[0] >> 3) - (sc_var == 2 ? 22 : 0) + (sc_var == 1 ? 14 : 0);
    static const int8_t TW[3][3] = {{120, 15, 60}, {204, 15, 50}, {232, 9, 38}};        /* x, width, height of a tower */
    for (i = 0; i < FB_W; i++)
        sc_hmap[i] = 0;
    /* far spires in the haze, both shores */
    for (x = 8; x < FB_W; x += 29 + (int)(sc_hash((uint32_t)x + 5u) % 11u)) {
        int h = 14 + (int)(sc_hash((uint32_t)x * 7u + 3u) % 16u), w = 5 + (int)(sc_hash((uint32_t)x) % 4u);
        if (x > 110 && x < 236)
            continue;
        fb_rect(x - w / 2, SC_HZ - h, w, h, C_F2);
        sc_spire(x, SC_HZ - h, w + 2, 12, C_F2, C_F2);
        fb_pset(x, SC_HZ - h - 13, C_F2);
    }
    /* the arcade at the foot of it all, and a low wall */
    fb_rect(112, SC_HZ - 14, 128, 14, C_F1);
    fb_hline(112, SC_HZ - 14, 128, C_F2);
    for (x = 114; x < 238; x += 7) {
        fb_rect(x, SC_HZ - 11, 4, 9, C_F0);                              /* round-headed arches: dark inside, a lamp in some */
        fb_pset(x + 1, SC_HZ - 12, C_F0);
        fb_pset(x + 2, SC_HZ - 12, C_F0);
        if ((int)(sc_hash((uint32_t)x * 13u) % 100u) < lit)
            fb_vline(x + 1 + (int)((x >> 3) & 1), SC_HZ - 9, 4, ((x >> 3) & 3) ? C_N0 : C_N1);
    }
    /* the nave: a broad facade with a gable, pinnacles along its roof, the great window and the rose */
    fb_rect(140, SC_HZ - 40, 64, 40, C_F1);
    fb_vline(140, SC_HZ - 40, 40, C_F2);
    fb_vline(203, SC_HZ - 40, 40, C_F0);
    for (i = 0; i < 32; i++)                                             /* the gable's slopes */
        fb_hline(140 + i, SC_HZ - 40 - (i * 14 / 32), 64 - 2 * i, i ? C_F1 : C_F2);
    for (x = 146; x < 200; x += 8) {
        int top = SC_HZ - 40 - ((x < 172 ? x - 140 : 204 - x) * 14 / 32);
        fb_vline(x, top - 5, 5, C_F1);                                   /* a pinnacle on the roof */
        fb_pset(x, top - 6, C_F2);
    }
    sc_spire(172, SC_HZ - 54, 6, 12, C_F1, C_F2);                        /* the finial over the gable */
    fb_vline(172, SC_HZ - 69, 4, C_F2);
    fb_hline(171, SC_HZ - 67, 3, C_F2);
    /* the rose window: a lit ring with eight spokes that turn their brightness slowly */
    {
        int cx = 172, cy = SC_HZ - 41, j, br = (int)(now >> 9);
        for (j = -6; j <= 6; j++)
            for (i = -6; i <= 6; i++) {
                int d2 = i * i + j * j;
                if (d2 > 36)
                    continue;
                if (d2 > 25)
                    fb_pset(cx + i, cy + j, C_F0);
                else if (d2 > 3) {
                    uint8_t c = ((i == 0 || j == 0 || i == j || i == -j) ? C_F0 : (((i + j + br) & 7) ? C_N0 : C_N2));
                    fb_pset(cx + i, cy + j, c);
                } else
                    fb_pset(cx + i, cy + j, C_N2);
            }
    }
    sc_lancet(164, SC_HZ - 31, 16, 24, C_N0);                            /* the great window, mullions and a transom */
    for (x = 168; x < 180; x += 4)
        fb_vline(x, SC_HZ - 27, 20, C_F0);
    fb_hline(164, SC_HZ - 20, 16, C_F0);
    fb_hline(165, SC_HZ - 26, 14, C_N1);
    for (x = 146; x < 200; x += 9)                                       /* side lancets, a few lit */
        if (x < 160 || x > 182) {
            uint32_t q = sc_hash((uint32_t)x * 29u + 7u);
            sc_lancet(x, SC_HZ - 28, 4, 14, (int)(q % 100u) < lit + 20 ? C_N1 : C_F0);
        }
    /* the towers and their spires, flying buttresses between them and the nave */
    for (i = 0; i < 3; i++) {
        int tx = TW[i][0], tw = TW[i][1], th = TW[i][2], ty = SC_HZ - th, w2;
        fb_rect(tx, ty, tw, th, C_F1);
        fb_vline(tx, ty, th, C_F2);
        fb_vline(tx + tw - 1, ty, th, C_F0);
        fb_hline(tx - 1, ty, tw + 2, C_F2);
        fb_hline(tx - 1, ty + 1, tw + 2, C_F0);
        sc_spire(tx + tw / 2, ty - 1, tw + 2, 22 + th / 6, C_F1, C_F2);
        fb_vline(tx + tw / 2, ty - 24 - th / 6, 5, C_F2);
        for (w2 = ty + 6; w2 < SC_HZ - 16; w2 += 12) {
            uint32_t q = sc_hash((uint32_t)(tx * 3 + w2 * 5));
            sc_lancet(tx + tw / 2 - 2, w2, 4, 9, (int)(q % 100u) < lit + 25 ? ((q >> 10) & 3u ? C_N0 : C_N2) : C_F0);
        }
        for (w2 = tx; w2 < tx + tw; w2++)
            if (SC_HZ - (ty - 1) > sc_hmap[w2])
                sc_hmap[w2] = (int8_t)(th * 3 / 4);
    }
    for (i = 0; i < 18; i++) {                                           /* flying buttresses: arcs from tower to nave */
        int k = i * 256 / 17;
        fb_pset(135 - (k * 14 >> 8) + 12, SC_HZ - 46 + ((k * (256 - k)) >> 12) - (k >> 5), C_F2);
        fb_pset(209 + (k * 14 >> 8) - 7, SC_HZ - 40 + ((k * (256 - k)) >> 12) - (k >> 5), C_F2);
    }
    for (x = 140; x < 204; x++)
        if (SC_HZ - 40 > sc_hmap[x])
            sc_hmap[x] = (int8_t)(40);
    /* the glow: the same faint dithered haze as the city, golden */
    {
        int breathe = 2 + (sc_tri((int)(now >> 6)) >> 6);
        for (x = 112; x < FB_W - 1; x++) {
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

/* the terrace he sits on: old stone in courses, a balustrade behind him, a broken column; and, bottom right, a fallen arch */
static void sc_terrace(void)
{
    int x, y;
    for (x = 0; x < SC_EDGE; x++) {
        int ty = SC_PY + (x < 30 ? (30 - x) / 3 : 0);
        fb_rect(x, ty, 1, SC_H - ty, C_F0);
        fb_pset(x, ty, C_F2);
    }
    for (y = SC_PY + 6; y < SC_H; y += 8) {                              /* courses of blocks, the joints staggered */
        int off = ((y - SC_PY) / 8 & 1) * 6;
        for (x = off; x < SC_EDGE; x += 12)
            fb_vline(x, y, 8, C_F1);
        fb_hline(0, y, SC_EDGE, C_F1);
    }
    fb_rect(SC_EDGE, SC_PY + 1, 3, SC_H - SC_PY - 1, C_F2);
    fb_checker(60, SC_PY + 14, 44, 30, C_F1);
    for (x = 36; x < 92; x += 6) {                                        /* the balustrade behind him: a rail and balusters */
        int ty = SC_PY + (x < 30 ? (30 - x) / 3 : 0);
        fb_rect(x, ty - 8, 2, 8, C_F1);
        fb_pset(x, ty - 8, C_F2);
        fb_pset(x + 1, ty - 5, C_F0);
    }
    fb_hline(34, SC_PY - 10, 60, C_F1);
    fb_hline(34, SC_PY - 11, 60, C_F2);
    fb_rect(10, SC_PY - 26, 14, 22, C_F1);
    fb_vline(10, SC_PY - 26, 22, C_F2);
    fb_vline(14, SC_PY - 26, 22, C_F0);
    fb_vline(19, SC_PY - 26, 22, C_F0);
    fb_pset(12, SC_PY - 27, C_F1);
    fb_pset(16, SC_PY - 28, C_F1);
    fb_pset(21, SC_PY - 27, C_F1);
    fb_hline(8, SC_PY - 4, 18, C_F2);                                     /* (a broken column: shaft, flutes, a jagged top) */
    /* the far side: a fallen arch rising out of the cloud sea */
    fb_rect(222, 128, 8, SC_H - 128, C_F0);
    fb_vline(222, 128, SC_H - 128, C_F2);
    fb_hline(220, 128, 12, C_F2);
    for (y = 0; y < 24; y++) {
        int xx = 222 - y * 3 / 2 - (y * y >> 6);
        fb_pset(xx, 128 + y - 24, C_F0);
        fb_pset(xx + 1, 128 + y - 24, C_F0);
        fb_pset(xx + 2, 128 + y - 24, C_F1);
    }
    sc_bush(186, SC_H, 30, 12, 0x51u);
    sc_bush(206, SC_H, 16, 18, 0x61u);
}

/* gold motes rising from the terrace; ash in the light */
static void sc_motes(uint32_t now)
{
    int i;
    for (i = 0; i < (sc_var == 1 ? 44 : sc_var == 2 ? 12 : 26); i++) {
        uint32_t k = sc_hash((uint32_t)i * 7919u + 0x2Bu);
        int bx = (int)(k % 236u) + 2, life = 70 + (int)((k >> 6) % 50u);
        int up = (int)(((now >> 6) + (k >> 3)) % (uint32_t)life);
        int x = bx + ((sc_tri((int)(now >> 5) + (int)(k >> 16)) - 128) >> 3);
        int y = 176 - up * 140 / life;
        int p = sc_tri((int)(now >> 2) + (int)(k >> 9));
        if (x > 82 && x < 112 && y > 110 && y < 150)
            continue;
        sc_glow(x, y, (i & 3) ? C_YELLOW : C_EMBER, p);
    }
}

/* ------------------------------------------------------------ the deep --- */
static void sc_surface(uint32_t now)                                     /* the sea's roof: bright ripples and the sun's pale disc */
{
    int x, e = sc_e[1];
    for (x = 0; x < FB_W; x += 2) {
        int h = 2 + (sc_tri(x * 3 + (int)(now >> 5)) >> 6) + (e >> 6);
        int y;
        for (y = 0; y < h; y++)
            fb_pset(x + (y & 1), y, y < 2 ? C_MOON2 : C_S1);
    }
    fb_disc(192, 3, 7, C_MOON);
    fb_disc(192, 3, 5, C_MOON);
}

static void sc_snow(uint32_t now)                                        /* marine snow, drifting down; plankton light up with voice 2 */
{
    int i, e = sc_e[1];
    for (i = 0; i < 44; i++) {
        uint32_t k = sc_hash((uint32_t)i * 2654435761u + 0x51u);
        int x = (int)(k % 238u) + 1 + ((sc_tri((int)(now >> 6) + (int)(k >> 14)) - 128) >> 5);
        int y = (int)((k >> 9) % 100u) + (int)((now >> 8) + (k >> 3)) % 100u;
        uint8_t c = ((k >> 20) & 3u) ? C_STAR2 : C_STAR;
        if (y > SC_HZ - 3)
            y -= 100;
        if (y < 6)
            continue;
        if ((k >> 22) % 4u == 0u && e > 100)
            c = C_STAR;
        fb_pset(x, y, c);
    }
}

static void sc_ruins(uint32_t now)
{
    int x, i, e = sc_e[3], lit = 34 + (e * 40 >> 8) + (sk_s.mac[0] >> 3), pulse = 60 + (sc_tri((int)(now >> 4)) >> 2) + (sc_var == 2 ? 40 : 0);
    for (i = 0; i < FB_W; i++)
        sc_hmap[i] = 0;
    /* far shapes in the blue: columns, a dome, an arch, both sides */
    for (x = 6; x < FB_W; x += 22 + (int)(sc_hash((uint32_t)x * 3u + 1u) % 9u)) {
        uint32_t k = sc_hash((uint32_t)x + 99u);
        int h = 16 + (int)(k % 22u), t = (int)((k >> 8) % 3u);
        if (x > 112 && x < 232)
            continue;
        if (t == 0) {
            fb_rect(x, SC_HZ - h, 4, h, C_F2);
            fb_hline(x - 1, SC_HZ - h, 6, C_F2);
        } else if (t == 1) {
            fb_rect(x - 5, SC_HZ - 10, 11, 10, C_F2);
            fb_disc(x, SC_HZ - 10, 5, C_F2);
        } else {
            fb_rect(x - 6, SC_HZ - h / 2, 3, h / 2, C_F2);
            fb_rect(x + 3, SC_HZ - h / 2, 3, h / 2, C_F2);
            fb_hline(x - 6, SC_HZ - h / 2 - 1, 12, C_F2);
        }
    }
    /* the temple: a stepped base, a row of columns (some broken), a half-lost pediment and a lit cella */
    fb_rect(118, SC_HZ - 4, 118, 4, C_F1);
    fb_rect(122, SC_HZ - 8, 110, 4, C_F1);
    fb_hline(118, SC_HZ - 4, 118, C_F2);
    fb_hline(122, SC_HZ - 8, 110, C_F2);
    fb_rect(150, SC_HZ - 40, 54, 32, C_F0);                              /* the cella behind the columns */
    fb_rect(160, SC_HZ - 34, 34, 26, C_N0);                              /* its glow: an old machine awake */
    fb_checker(160, SC_HZ - 34, 34, 26, C_N1);
    for (x = 162; x < 192; x += 6) {                                     /* runes down the wall */
        int y;
        for (y = SC_HZ - 32; y < SC_HZ - 10; y += 3)
            if (sc_hash((uint32_t)(x * 17 + y)) % 3u)
                fb_pset(x, y, C_N2);
    }
    for (i = 0; i < 6; i++) {
        uint32_t k = sc_hash((uint32_t)i * 313u + 17u);
        int cx = 130 + i * 18, ch = ((k % 7u) == 0u) ? 12 + (int)(k % 9u) : 34, top = SC_HZ - 8 - ch;
        if (i == 2 || i == 3)
            ch = 36;
        top = SC_HZ - 8 - ch;
        fb_rect(cx, top, 6, ch, C_F1);
        fb_vline(cx, top, ch, C_F2);
        fb_vline(cx + 2, top, ch, C_F0);
        fb_vline(cx + 5, top, ch, C_F0);
        if (ch > 30) {
            fb_hline(cx - 1, top, 8, C_F2);                              /* the capital */
            fb_hline(cx - 1, top + 1, 8, C_F0);
        } else {
            fb_pset(cx + 1, top - 1, C_F1);                              /* a broken top */
            fb_pset(cx + 4, top - 2, C_F1);
        }
        for (x = cx; x < cx + 6; x++)
            if (SC_HZ - top > sc_hmap[x])
                sc_hmap[x] = (int8_t)(SC_HZ - top);
    }
    fb_rect(126, SC_HZ - 46, 74, 4, C_F1);                                /* the architrave */
    fb_hline(126, SC_HZ - 46, 74, C_F2);
    for (i = 0; i < 24; i++) {
        fb_hline(128 + i, SC_HZ - 47 - (i * 14 / 24), 70 - 2 * i < 1 ? 1 : 70 - 2 * i, C_F1);
        fb_pset(128 + i, SC_HZ - 47 - (i * 14 / 24), C_F2);
    }
    /* a domed tower to the right, ringed in light, and the old machine's orb above it */
    fb_rect(212, SC_HZ - 36, 20, 36, C_F1);
    fb_vline(212, SC_HZ - 36, 36, C_F2);
    fb_vline(231, SC_HZ - 36, 36, C_F0);
    fb_disc(222, SC_HZ - 36, 10, C_F1);
    fb_rect(212, SC_HZ - 36, 20, 12, C_F1);
    for (i = 0; i < 8; i++) {                                            /* the ring of lights */
        int a = i * 2 + (int)(now >> 9);
        fb_pset(214 + ((a * 5) & 15), SC_HZ - 28, (int)(sc_hash((uint32_t)i * 41u + 3u) % 100u) < lit + 30 ? C_N0 : C_F0);
    }
    for (i = 0; i < 4; i++) {
        uint32_t q = sc_hash((uint32_t)i * 71u + 5u);
        sc_lancet(216 + i * 4 - 1, SC_HZ - 22, 3, 12, (int)(q % 100u) < lit + 25 ? C_N3 : C_F0);
    }
    {
        int oy = SC_HZ - 58 - (sc_tri((int)(now >> 5)) >> 6), p = pulse + (sc_e[5] >> 2);
        fb_disc(222, oy, 3, C_N2);
        if (p > 70)
            fb_checker(218, oy - 4, 9, 9, C_N0);
        if (p > 100)
            fb_checker(216, oy - 6, 13, 13, C_G0);
        for (x = 218; x < 227; x++)
            if (SC_HZ - oy + 6 > sc_hmap[x])
                sc_hmap[x] = (int8_t)(SC_HZ - oy + 4);
    }
    for (x = 126; x < 200; x++)
        if (sc_hmap[x] < 48)
            sc_hmap[x] = 48;
    /* the glow: aqua, a little thicker than the city's, because water carries light further */
    {
        int breathe = 3 + (sc_tri((int)(now >> 6)) >> 6);
        for (x = 118; x < FB_W - 1; x++) {
            int h = sc_hmap[x], g = 7 + (h >> 2) + breathe + (e >> 6);
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

/* the ledge: a fallen drum of stone with a worn top, kelp swaying on both sides, coral at the foot */
static void sc_ledge(uint32_t now)
{
    int x, y, k;
    for (x = 0; x < SC_EDGE; x++) {
        uint32_t q = sc_hash((uint32_t)(x >> 2) + 0x61u);
        int ty = SC_PY + (x < 30 ? (30 - x) / 3 : 0) + (int)(q % 2u);
        fb_rect(x, ty, 1, SC_H - ty, C_F0);
        fb_pset(x, ty, C_F2);
    }
    fb_rect(SC_EDGE, SC_PY + 1, 3, SC_H - SC_PY - 1, C_F2);
    for (y = SC_PY + 4; y < SC_H; y += 9)                                /* the drum's fluting and its seams */
        fb_hline(40, y, SC_EDGE - 40, C_F1);
    for (x = 44; x < SC_EDGE; x += 9)
        fb_vline(x, SC_PY + 3, SC_H - SC_PY - 3, C_F1);
    fb_checker(40, SC_PY + 24, 66, 18, C_F1);
    for (k = 0; k < 7; k++) {                                            /* kelp on the left: strands swaying with the tide */
        int bx = 4 + k * 8, hh = 26 + (k * 37) % 22, j;
        for (j = 0; j < hh; j++) {
            int sw = ((sc_tri((int)(now >> 5) + j * 9 + k * 70) - 128) * (j + 4)) >> 9;
            fb_pset(bx + sw, SC_PY + 6 - j, C_F0);
            fb_pset(bx + sw + 1, SC_PY + 6 - j, (j & 3) == 0 ? C_F1 : C_F0);
        }
    }
    for (k = 0; k < 5; k++) {                                            /* and a forest at the right, bottom */
        int bx = 160 + k * 17, hh = 30 + (k * 53) % 30, j;
        for (j = 0; j < hh; j++) {
            int sw = ((sc_tri((int)(now >> 5) + j * 7 + k * 90) - 128) * (j + 6)) >> 9;
            fb_pset(bx + sw, SC_H - j, C_F0);
            fb_pset(bx + sw + 1, SC_H - j, C_F0);
            if ((j & 7) == 3)
                fb_pset(bx + sw + 2, SC_H - j, C_F1);
        }
    }
    sc_bush(214, SC_H, 22, 10, 0x71u);                                    /* coral heads */
    sc_bush(46, SC_PY + 4, 10, 6, 0x41u);
}

/* bubbles climb from the floor, wobbling; one rises where voice 3 sounds */
static void sc_bubbles(uint32_t now)
{
    int i;
    for (i = 0; i < (sc_var == 1 ? 34 : sc_var == 2 ? 8 : sc_var == 3 ? 14 : 20); i++) {
        uint32_t k = sc_hash((uint32_t)i * 7919u + 0x2Bu);
        int bx = (int)(k % 236u) + 2, life = 50 + (int)((k >> 6) % 40u);
        int up = (int)(((now >> 6) + (k >> 3)) % (uint32_t)life);
        int x = bx + ((sc_tri((int)(now >> 4) + (int)(k >> 16)) - 128) >> 4);
        int y = 184 - up * 170 / life;
        if (x > 82 && x < 112 && y > 108 && y < 150)
            continue;
        if (y < 8)
            continue;
        fb_pset(x, y, C_STAR2);
        if (i & 1) {
            fb_pset(x - 1, y, C_STAR2);
            fb_pset(x + 1, y, C_STAR2);
            fb_pset(x, y - 1, C_STAR2);
            fb_pset(x, y + 1, C_STAR2);
            fb_pset(x, y, C_G1);
        }
    }
}

/* voice 3: a school of small fish crosses the water */
static void sc_fish(uint32_t now)
{
    uint32_t age = now - sc.hit[2];
    int p, i;
    if (age > 7000u)
        return;
    p = (int)(age * 256u / 7000u);
    for (i = 0; i < 9; i++) {
        uint32_t k = sc_hash((uint32_t)i * 977u + 11u);
        int x = -14 + p * 270 / 256 - (int)(k % 22u), y = 60 + (int)((k >> 8) % 24u) + (sc_tri((int)(now >> 5) + i * 60) >> 6);
        fb_pset(x, y, C_STAR);
        fb_pset(x - 1, y, C_STAR2);
        fb_pset(x - 2, y + ((((now >> 7) + (uint32_t)i) & 1u) ? 1 : -1), C_STAR2);
    }
}
