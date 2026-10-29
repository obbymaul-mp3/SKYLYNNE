/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* The effect pages show what their knobs do: over the scene, just above the four knob boxes, a small live picture for each
 * knob (the room's tail, the echo's taps, the scale's notes, the envelope's shape, the arpeggio's path, the grains as they
 * fly...). Drawn after the scene and before the HUD; included by sk_ui.c. */

#define VZ_W 55
#define VZ_H 40
#define VZ_Y 146

static int vz_abs(int v) { return v < 0 ? -v : v; }

/* the frame of one picture; i is its knob (0..3) */
static void vz_frame(int i, int *x, int *y)
{
    int x0 = 4 + i * 59;
    fb_rect(x0, VZ_Y, VZ_W, VZ_H, C_BG);
    fb_checker(x0, VZ_Y, VZ_W, VZ_H, C_PANEL);
    fb_hline(x0, VZ_Y + VZ_H - 1, VZ_W, C_PANEL2);
    *x = x0 + 3;
    *y = VZ_Y + 3;
}
#define VZ_PW (VZ_W - 6)
#define VZ_PH (VZ_H - 6)

static int vz_sin(uint32_t ph) { return SK_SIN[(ph >> 22) & 1023u]; }          /* ph: 2^32 a turn; +-32767 */

static void vz_fx(int i, int x, int y, uint32_t now, uint8_t col)
{
    int k;
    if (i == 0) {                                                           /* ROOM: the tail */
        int rt = (int)sk_s.b->rv + ((int)sk_s.mac[3] - 64) / 6 + ((int)sk_s.fx[0] - 64) / 4, a = VZ_PH * 256, f;
        rt = rt < 0 ? 0 : rt > 31 ? 31 : rt;
        f = 196 + rt * 59 / 31;
        for (k = 0; k < VZ_PW; k++) {
            int h = a >> 8;
            fb_vline(x + k, y + VZ_PH - h, h, (k & 3) == 0 ? col : C_INK2);
            fb_pset(x + k, y + VZ_PH - h, col);
            a = a * f >> 8;
        }
    } else if (i == 1) {                                                    /* ECHO: the taps */
        int sp = 6 + (int)(sk_s.b->dl ? sk_s.b->dl : 1u) * 4, h = VZ_PH, amt = (int)sk_s.fx[1];
        int fb = ((9000 + (int)sk_s.mac[3] * 100) * (64 + amt) / 128) * 256 / 32768;
        fb_vline(x, y + VZ_PH - h, h, C_INK);
        h = h * amt / 90;
        for (k = 1; k < 8 && h > 1; k++) {
            int tx = x + k * sp;
            if (tx >= x + VZ_PW)
                break;
            h = h * fb >> 8;
            fb_vline(tx, y + VZ_PH - h, h, col);
            fb_vline(tx + 1, y + VZ_PH - h, h, col);
        }
    } else if (i == 2) {                                                    /* CLOUD: grains of light */
        int n = sk_s.fx[2] > 64u ? ((int)sk_s.fx[2] - 64) * 22 / 63 : 0, sz = 1 + (int)sk_s.b->gr / 50;
        for (k = 0; k < n; k++) {
            uint32_t h = sc_hash((uint32_t)k * 97u + (now >> 8) * 13u);
            int gx = x + (int)(h % (uint32_t)(VZ_PW - 4)), gy = y + (int)((h >> 8) % (uint32_t)(VZ_PH - 3));
            fb_rect(gx, gy, 2 + sz, 1 + (sz > 1), (h >> 20) & 1u ? col : C_INK);
        }
        if (!n)
            fb_hline(x, y + VZ_PH / 2, VZ_PW, C_INK2);
    } else {                                                                /* HI CUT: the top rolling off */
        int kn = VZ_PW - (int)sk_s.fx[3] * (VZ_PW - 8) / 130 - 4, top = y + 4;
        fb_hline(x, top, kn, col);
        for (k = kn; k < VZ_PW; k++) {
            int yy = top + (k - kn) * 7 / 4;
            if (yy > y + VZ_PH - 1)
                break;
            fb_pset(x + k, yy, col);
            fb_vline(x + k, yy + 1, y + VZ_PH - yy - 1, (k & 1) ? C_INK2 : C_PANEL2);
        }
    }
}

static void vz_scl(int i, int x, int y, uint32_t now, uint8_t col)
{
    int k;
    (void)now;
    if (i == 0) {                                                           /* ROOT: the twelve notes, the key lit */
        for (k = 0; k < 12; k++) {
            int black = (0x54Au >> k) & 1, h = black ? 16 : 24, kx = x + k * 4 + 1;
            fb_rect(kx, y + VZ_PH - h, 3, h, (uint32_t)k == sk_s.root % 12u ? col : (black ? C_INK2 : C_INK));
        }
    } else if (i == 1) {                                                    /* SCALE: which of the twelve it uses */
        uint8_t on[12] = {0};
        int len = sk_s.len ? sk_s.len : 7;
        for (k = 0; k < len; k++)
            on[(sk_s.iv[k]) % 12u] = 1;
        for (k = 0; k < 12; k++)
            fb_rect(x + k * 4 + 1, y + VZ_PH - (on[k] ? 28 : 6), 3, on[k] ? 28 : 6, on[k] ? (k == 0 ? C_INK : col) : C_PANEL2);
    } else if (i == 2) {                                                    /* OCTAVE: a ladder, the shift marked */
        for (k = -2; k <= 2; k++) {
            int ry = y + VZ_PH / 2 - k * 7;
            fb_hline(x + 8, ry, VZ_PW - 22, k == sk_s.shift ? col : C_PANEL2);
            if (k == sk_s.shift)
                fb_rect(x + VZ_PW / 2 - 4, ry - 2, 8, 5, col);
        }
    } else {                                                                /* WALK: how far the notes wander from the middle */
        int w = sk_s.walk, r = vz_abs(w) * 7, cx = x + VZ_PW / 2, cy = y + VZ_PH / 2;
        fb_hline(x, cy, VZ_PW, C_PANEL2);
        fb_vline(cx, cy - 6, 13, C_INK2);
        if (r) {
            fb_hline(cx - r, cy - 1, 2 * r + 1, col);
            fb_hline(cx - r, cy + 1, 2 * r + 1, col);
            fb_vline(cx - r, cy - 5, 11, col);
            fb_vline(cx + r, cy - 5, 11, col);
            fb_rect(cx + (vz_sin((uint32_t)now * 3000000u) * r >> 15) - 1, cy - 2, 3, 5, C_INK);
        } else
            fb_rect(cx - 1, cy - 2, 3, 5, C_INK);
    }
}

static void vz_env(int i, int x, int y, uint32_t now, uint8_t col)
{
    int k, v = sk_s.sel, h = VZ_PH - 2, by = y + VZ_PH - 1;
    (void)now;
    if (i == 0) {                                                           /* ATTACK */
        int w = 6 + (sk_s.atk[v] + 6) * 3, px = x, py = by;
        for (k = 0; k < VZ_PW; k++) {
            int yy = k < w ? by - h * k / w : by - h;
            fb_line(px, py, x + k, yy, col);
            px = x + k;
            py = yy;
        }
    } else if (i == 1) {                                                    /* RELEASE */
        int w = 6 + (sk_s.rel[v] + 6) * 3, st = 8, px = x, py = by - h;
        for (k = st; k < VZ_PW; k++) {
            int yy = k < st + w ? by - h + h * (k - st) / w : by;
            fb_line(px, py, x + k, yy, col);
            px = x + k;
            py = yy;
        }
        fb_hline(x, by - h, st, col);
    } else if (i == 2) {                                                    /* GLIDE: a step, rounded off */
        int w = 2 + (sk_s.gli[v] + 6) * 3, px = x, py = by - 6;
        for (k = 0; k < VZ_PW; k++) {
            int mid = VZ_PW / 2 - w / 2, yy;
            if (k < mid)
                yy = by - 6;
            else if (k > mid + w)
                yy = by - h + 4;
            else
                yy = by - 6 - (h - 10) * (k - mid) / (w ? w : 1);
            fb_line(px, py, x + k, yy, col);
            px = x + k;
            py = yy;
        }
    } else {                                                                /* CURVE: slow start .. quick start */
        int c = sk_s.crv[v] - 64, px = x, py = by;
        for (k = 0; k < VZ_PW; k++) {
            int t = k * 256 / VZ_PW, lin = t, bent = c > 0 ? (t * t) >> 8 : 256 - (((256 - t) * (256 - t)) >> 8);
            int m = vz_abs(c) * 4, yv = (lin * (256 - m) + bent * m) >> 8, yy = by - h * yv / 256;
            fb_line(px, py, x + k, yy, col);
            px = x + k;
            py = yy;
        }
    }
}

static void vz_lfo(int i, int x, int y, uint32_t now, uint8_t col)
{
    int k, cy = y + VZ_PH / 2;
    uint32_t scroll = (uint32_t)now * 4000u;
    fb_hline(x, cy, VZ_PW, C_PANEL2);
    if (i == 0 || i == 1) {                                                 /* RATE: faster or slower waves; DEPTH: taller or flatter */
        uint32_t cyc = i == 0 ? 1u + sk_s.lfo[0] / 32u : 2u;
        int amp = i == 0 ? 12 : sk_s.lfo[1] * 14 / 127 + 1, py = cy;
        for (k = 0; k < VZ_PW; k++) {
            uint32_t ph = (uint32_t)k * 87652000u * cyc + scroll * (i == 0 ? cyc : 1u);
            int yy = cy - (vz_sin(ph) * amp >> 15);
            fb_line(x + k - (k ? 1 : 0), py, x + k, yy, col);
            py = yy;
        }
    } else if (i == 2) {                                                    /* PAN: a dot swinging left and right */
        int amp = sk_s.lfo[2] * (VZ_PW / 2 - 4) / 127, bx = x + VZ_PW / 2 + (vz_sin((uint32_t)now * 5000u * (1u + sk_s.lfo[0] / 32u)) * amp >> 15);
        fb_hline(x + 3, cy + 6, VZ_PW - 6, C_INK2);
        fb_rect(x + 1, cy + 4, 2, 5, C_INK);
        fb_rect(x + VZ_PW - 3, cy + 4, 2, 5, C_INK);
        fb_disc(bx, cy - 2, 3, col);
    } else {                                                                /* VIB: a wobble on a steady note */
        int amp = sk_s.lfo[3] * 10 / 127, py = cy;
        for (k = 0; k < VZ_PW; k++) {
            int yy = cy - (vz_sin(((uint32_t)k * 70000000u) + scroll * 3u) * amp >> 15) + (vz_sin((uint32_t)k * 9000000u) * 2 >> 15);
            fb_line(x + k - (k ? 1 : 0), py, x + k, yy, col);
            py = yy;
        }
    }
}

/* where in its cycle the arpeggio's path is for step k (the same maths as the player) */
static int vz_arp_t(uint32_t pat, uint32_t k, uint32_t n)
{
    static const uint8_t EV[8] = {0, 2, 1, 3, 2, 4, 3, 5};
    switch (pat) {
    case 1: return (int)(k % n);
    case 2: return (int)(n - 1u - k % n);
    case 3: { uint32_t m = 2u * n - 2u, j = k % m; return (int)(j < n ? j : m - j); }
    case 4: return (int)(sc_hash(k * 7u + 3u) % n);
    case 5: return (int)(k % (n + 3u) < n ? k % (n + 3u) : n - 1u);
    default: return (int)(EV[k & 7u] % n);
    }
}

static void vz_arp(int i, int x, int y, uint32_t now, uint8_t col)
{
    int k;
    uint32_t oct = sk_s.arp[2] < 1u ? 1u : sk_s.arp[2] > 3u ? 3u : sk_s.arp[2], n = 3u * oct + 1u;
    if (i == 0) {                                                           /* PATTERN: the notes it walks through */
        int px = 0, py = 0, cur = (int)(sk_ar.k % 12u);
        for (k = 0; k < 12; k++) {
            int tx = x + 2 + k * 4, ty = y + VZ_PH - 3 - vz_arp_t(sk_s.arp[0] ? sk_s.arp[0] : 1u, (uint32_t)k, n) * (VZ_PH - 6) / (int)(n - 1u);
            if (!sk_s.arp[0])
                ty = y + VZ_PH / 2;
            if (k)
                fb_line(px, py, tx, ty, C_INK2);
            px = tx;
            py = ty;
            fb_rect(tx - 1, ty - 1, 3, 3, (k == cur && sk_s.arp[0]) ? C_INK : col);
        }
    } else if (i == 1) {                                                    /* RATE: the ticks in a bar's four beats */
        static const uint8_t PER[5] = {1, 2, 4, 6, 8};                      /* per beat: 1/4 1/8 1/16 1/16T 1/32 */
        int per = PER[sk_s.arp[1] % 5u], total = per * 4, cur = (int)((now * (uint32_t)sk_s.bpm / 15000u) % (uint32_t)total);
        for (k = 0; k < total; k++) {
            int tx = x + 1 + k * (VZ_PW - 3) / total;
            fb_vline(tx, y + 8, VZ_PH - 16, (k % per) == 0 ? C_INK : col);
            if (k == cur)
                fb_rect(tx - 1, y + 4, 3, 3, C_INK);
        }
    } else if (i == 2) {                                                    /* RANGE: octaves stacked */
        for (k = 0; k < 3; k++) {
            int ry = y + VZ_PH - 10 - k * 10;
            fb_rect(x + 6, ry, VZ_PW - 12, 7, (uint32_t)k < oct ? col : C_PANEL2);
        }
    } else {                                                                /* GATE: how much of each step the note fills */
        int step = VZ_PW / 4, len = (step - 2) * (8 + (int)sk_s.arp[3] * 120 / 127) / 128;
        len = len < 1 ? 1 : len > step - 1 ? step - 1 : len;
        for (k = 0; k < 4; k++) {
            fb_hline(x + k * step, y + VZ_PH - 4, step - 1, C_PANEL2);
            fb_rect(x + k * step, y + VZ_PH / 2 - 3, len, 7, col);
        }
    }
}

static void vz_grain(int i, int x, int y, uint32_t now, uint8_t col)
{
    int k;
    if (i == 0) {                                                           /* POS: the buffer, newest at the right, with the live grains */
        fb_hline(x, y + VZ_PH - 2, VZ_PW, sk_gm.frz ? C_CYAN : C_INK2);
        for (k = 0; k < SK_NG; k++) {
            const sk_grain_t *g = &sk_f.gr[k];
            if (g->on) {
                int back = (int)((sk_f.gwp - (g->pos >> 16)) & (SK_GRN - 1u)), gx = x + VZ_PW - 1 - back * VZ_PW / (int)SK_GRN;
                int st = (int)(g->step >> 1) - 32768, gy = y + 14 - st * 11 / 32768, w = (int)g->len * VZ_PW / (int)SK_GRN + 2;
                if (gx < x)
                    gx = x;
                fb_hline(gx, gy, w > VZ_PW - (gx - x) ? VZ_PW - (gx - x) : w, g->age * 2u < g->len ? C_INK : col);
            }
        }
        {
            int mx = x + VZ_PW - 1 - sk_s.grn[0] * (VZ_PW - 2) / 127;
            fb_rect(mx - 1, y + VZ_PH - 6, 3, 4, col);
        }
    } else if (i == 1) {                                                    /* SIZE: the grain's window */
        int w = 6 + sk_s.grn[1] * (VZ_PW - 6) / 127;
        for (k = 0; k < w; k++) {
            int h = SK_HANN[k * 255 / (w > 1 ? w - 1 : 1)] * (VZ_PH - 4) >> 15;
            fb_vline(x + (VZ_PW - w) / 2 + k, y + VZ_PH - 2 - h, h, (k & 1) ? col : C_INK2);
        }
    } else if (i == 2) {                                                    /* DENSITY: how many at once */
        int n = 2 + sk_s.grn[2] * 26 / 127;
        for (k = 0; k < n; k++) {
            uint32_t h = sc_hash((uint32_t)k * 131u + (now >> 7) * 17u);
            fb_rect(x + (int)(h % (uint32_t)(VZ_PW - 5)), y + (int)((h >> 9) % (uint32_t)(VZ_PH - 4)), 3, 2, (h >> 20) & 1u ? col : C_INK);
        }
    } else {                                                                /* PITCH: a ladder of semitones, the setting marked */
        int st = ((int)sk_s.grn[3] - 64) * 12 / 63, my;
        st = st < -12 ? -12 : st > 12 ? 12 : st;
        for (k = -12; k <= 12; k += 12)
            fb_hline(x + 6, y + VZ_PH / 2 - k * (VZ_PH - 8) / 24, VZ_PW - 12, C_PANEL2);
        fb_hline(x + 2, y + VZ_PH / 2, VZ_PW - 4, C_INK2);
        my = y + VZ_PH / 2 - st * (VZ_PH - 8) / 24;
        fb_rect(x + VZ_PW / 2 - 6, my - 2, 12, 5, col);
    }
}

static void vz_glo(int i, int x, int y, uint32_t now, uint8_t col)
{
    int k;
    if (i == 0) {                                                           /* LOOP: the bars, the present one lit */
        int bars = 4 * ((int)sk_s.glo[0] % 8 + 1), w = (VZ_PW - 1) / bars, cur = sk_gv.on ? (int)sk_gv.bar - 1 : -1, rows = bars > 16 ? 2 : 1, per = (bars + rows - 1) / rows;
        w = (VZ_PW - 1) / per;
        for (k = 0; k < bars; k++) {
            int r = k / per, c = k % per, lvl = k * 4 / bars;
            fb_rect(x + c * w, y + 6 + r * 16, w - 1 > 0 ? w - 1 : 1, 11 + lvl * 2 - (r ? 0 : 0), k == cur ? C_INK : (k < cur ? col : C_PANEL2));
        }
    } else if (i == 1) {                                                    /* POWER: four stages of the build; the loud ones lit */
        int lit = sk_gv.on ? (int)sk_gv.level + 1 : 0, bias = sk_s.glo[1] >= 96 ? 1 : sk_s.glo[1] < 32 ? -1 : 0;
        for (k = 0; k < 4; k++)
            fb_rect(x + 3 + k * 12, y + VZ_PH - 8 - k * 7, 9, 7 + k * 7, k < lit ? col : (k < lit + 1 && bias > 0 ? C_INK2 : C_PANEL2));
    } else if (i == 2) {                                                    /* BOLTS: the more, the more often it flashes */
        int n = 1 + sk_s.glo[2] / 45;
        for (k = 0; k < n; k++) {
            int bx = x + 8 + k * 14, by = y + 2, fl = ((now / 120u + (uint32_t)k * 5u) % 9u) < 3u;
            uint8_t c = fl ? C_INK : col;
            fb_line(bx + 4, by, bx, by + 12, c);
            fb_line(bx, by + 12, bx + 6, by + 14, c);
            fb_line(bx + 6, by + 14, bx + 2, by + VZ_PH - 8, c);
        }
    } else {                                                                /* AURA: its four colours */
        for (k = 0; k < 4; k++)
            fb_rect(x + 2 + k * 12, y + 4, 11, VZ_PH - 8, C_A0 + k);
    }
}

static void viz_draw(uint32_t now)
{
    static const uint8_t PGV[PG_N] = {0, 0, 0, 1, 2, 3, 4, 0, 5, 7, 6};
    int i, x, y;
    uint32_t p = PGV[ui.page];
    if (!p)
        return;
    for (i = 0; i < 4; i++) {
        vz_frame(i, &x, &y);
        switch (p) {
        case 1: vz_fx(i, x, y, now, UI_ACC[i]); break;
        case 2: vz_scl(i, x, y, now, UI_ACC[i]); break;
        case 3: vz_env(i, x, y, now, UI_ACC[i]); break;
        case 4: vz_lfo(i, x, y, now, UI_ACC[i]); break;
        case 5: vz_arp(i, x, y, now, UI_ACC[i]); break;
        case 6: vz_grain(i, x, y, now, UI_ACC[i]); break;
        default: vz_glo(i, x, y, now, UI_ACC[i]); break;
        }
    }
}
