/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* SKYLYNNE's interface: the scene fills the screen, four knob boxes sit under it, and everything else is a gesture.
 *
 *   K1..K4      DENSITY  MOTION  COLOUR  SPACE: how much plays, how far it wanders, how bright, how far away
 *   SELECT      which of the six voices you are shaping (the figure points at it)
 *   ALGORITHM   that voice's kind: resonator, pluck, FM bell, pad, air
 *   PRESETS     the biome: a whole soundscape with its scale, tempo and room
 *   EDIT        the voice page: K1..K4 become LEVEL  PAN  DECAY  MATERIAL
 *   SEQ         the physics page: K1..K4 become SPIN  GRAVITY  BOUNCE  BALLS, PRESETS the shape's sides
 *   HOME        back to the scene
 *   OCT- / OCT+ the tempo         PLAY  the voices wait, and the sound rings out
 *   keys 1-6    mute a voice      keys 7-27  play the selected voice on the biome's scale
 *
 * The platform provides (sk_hw.c, the host harness): sk_in_enc(role), sk_in_buttons(&held, &released),
 * sk_in_keys(&held), sk_led_btn(label, level), sk_led_key(n, level). */

/* the panel's labels and encoders */
enum { SB_FX, SB_SCL, SB_ENV, SB_LFO, SB_EDIT, SB_GLO, SB_HOME, SB_SAVE, SB_ARP, SB_SEQ, SB_PLAY, SB_REC, SB_OCTDN, SB_OCTUP, SB_N };
enum { SE_SELECT, SE_ALGO, SE_PRESET, SE_K1, SE_K2, SE_K3, SE_K4, SE_N };
enum { PG_HOME, PG_EDIT, PG_PHYS };

static int32_t sk_in_enc(uint32_t role);
static void sk_in_buttons(uint32_t *held, uint32_t *pressed);
static uint32_t sk_in_keys(uint32_t *pressed);
static void sk_led_btn(uint32_t label, uint32_t level);
static void sk_led_key(uint32_t n, uint32_t level);
static void sk_save_poll(void);

static struct {
    uint8_t page, biome;
    uint32_t pop_t, hud_t, k_t[4];       /* when the biome's name, the voice HUD, a knob was last touched */
    uint32_t held;
} ui;

static const char *const UI_MAC[4] = {"DENSITY", "MOTION", "COLOUR", "SPACE"};
static const char *const UI_EDITK[4] = {"LEVEL", "PAN", "DECAY", "MATERIAL"};
static const char *const UI_PHYSK[4] = {"SPIN", "GRAVITY", "BOUNCE", "BALLS"};
static const uint8_t UI_ACC[4] = {C_PINK, C_CYAN, C_YELLOW, C_GREEN};
static const uint8_t UI_MACAIM[4] = {2, 0, 1, 5};      /* the part of the scene each macro points at */

static void ui_touch(uint32_t v)
{
    sc_touch_v = (uint8_t)v;
    sc_touch_n++;
}

static void ui_biome(uint32_t i, uint32_t now)
{
    ui.biome = (uint8_t)(i % SK_NBIOME);
    sk_load(&SK_BIOMES[ui.biome]);
    sc_theme(ui.biome);
    ui.pop_t = now;
}

static int ui_clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void ui_input(uint32_t now)
{
    uint32_t held, pr, keys, kp, i;
    int32_t d;
    sk_in_buttons(&held, &pr);
    keys = 0;
    kp = sk_in_keys(&keys);
    ui.held = held;
    if (pr & (1u << SB_HOME))
        ui.page = PG_HOME;
    if (pr & (1u << SB_EDIT))
        ui.page = ui.page == PG_EDIT ? PG_HOME : PG_EDIT;
    if (pr & (1u << SB_SEQ))
        ui.page = ui.page == PG_PHYS ? PG_HOME : PG_PHYS;
    if (pr & (1u << SB_PLAY))
        sk_s.hold ^= 1u;
    if (pr & (1u << SB_OCTDN))
        sk_s.bpm = (uint8_t)ui_clamp(sk_s.bpm - 2, 30, 160), sk_steplen_set();
    if (pr & (1u << SB_OCTUP))
        sk_s.bpm = (uint8_t)ui_clamp(sk_s.bpm + 2, 30, 160), sk_steplen_set();
    if (pr & ((1u << SB_OCTDN) | (1u << SB_OCTUP)))
        ui.hud_t = now;
    /* the voice and its kind */
    d = sk_in_enc(SE_SELECT);
    if (d) {
        sk_s.sel = (uint8_t)((sk_s.sel + SK_NV * 8u + (uint32_t)d) % SK_NV);
        ui.hud_t = now;
        ui_touch(sk_s.sel);
    }
    d = sk_in_enc(SE_ALGO);
    if (d) {
        sk_s.type[sk_s.sel] = (uint8_t)((sk_s.type[sk_s.sel] + VT_N * 8u + (uint32_t)d) % VT_N);
        ui.hud_t = now;
        ui_touch(sk_s.sel);
    }
    d = sk_in_enc(SE_PRESET);
    if (d) {
        if (ui.page == PG_PHYS)
            ph.sides = (uint8_t)ui_clamp(ph.sides + d, 3, 8);
        else
            ui_biome(ui.biome + SK_NBIOME * 8u + (uint32_t)d, now);
    }
    /* the four knobs */
    for (i = 0; i < 4u; i++) {
        d = sk_in_enc(SE_K1 + i);
        if (!d)
            continue;
        ui.k_t[i] = now;
        if (ui.page == PG_HOME) {
            sk_s.mac[i] = (uint8_t)ui_clamp(sk_s.mac[i] + d * 2, 0, 127);
            ui_touch(UI_MACAIM[i]);
        } else if (ui.page == PG_EDIT) {
            uint32_t v = sk_s.sel;
            switch (i) {
            case 0: sk_s.level[v] = (uint8_t)ui_clamp(sk_s.level[v] + d * 2, 0, 100); break;
            case 1: sk_s.pan[v] = (int8_t)ui_clamp(sk_s.pan[v] + d * 4, -64, 63); break;
            case 2: sk_s.decay[v] = (int8_t)ui_clamp(sk_s.decay[v] + d, -6, 6); break;
            default: {
                uint32_t m = sk_s.type[v] == VT_BELL ? 4u : 6u;
                sk_s.mat[v] = (uint8_t)((sk_s.mat[v] % m + m * 8u + (uint32_t)d) % m);
                break;
            }
            }
            ui.hud_t = now;
            ui_touch(v);
        } else {
            switch (i) {
            case 0: ph.spin = (int8_t)ui_clamp(ph.spin + d * 2, -64, 63); break;
            case 1: ph.grav = (uint8_t)ui_clamp(ph.grav + d * 3, 0, 127); break;
            case 2: ph.bounce = (uint8_t)ui_clamp(ph.bounce + d * 3, 0, 127); break;
            default: ph.nb = (uint8_t)ui_clamp(ph.nb + d, 0, PH_MAXB); break;
            }
        }
    }
    /* the keys: the first six mute a voice, the rest play the selected one */
    for (i = 0; i < 27u; i++) {
        if (!((kp >> i) & 1u))
            continue;
        if (i < SK_NV) {
            sk_s.mute ^= (uint8_t)(1u << i);
            ui_touch(i);
        } else {
            sk_play(sk_s.sel, sk_note_of(sk_s.sel, (int)i - 6 - 4), 96u);
            ui_touch(sk_s.sel);
        }
    }
    for (i = 0; i < 27u; i++) {
        uint32_t lvl = 0;
        if (i < SK_NV) {
            lvl = (sk_s.mute >> i) & 1u ? 0u : (sk_ui.lit[i] > 150 ? 3u : 2u);
        } else if ((keys >> i) & 1u)
            lvl = 3u;
        sk_led_key(i, lvl);
    }
    for (i = 0; i < SB_N; i++) {
        uint32_t lvl = 0;
        if ((i == SB_EDIT && ui.page == PG_EDIT) || (i == SB_SEQ && ui.page == PG_PHYS) || (i == SB_HOME && ui.page == PG_HOME) ||
            (i == SB_PLAY && sk_s.hold))
            lvl = 3;
        else if (i == SB_EDIT || i == SB_SEQ || i == SB_HOME || i == SB_PLAY || i == SB_OCTDN || i == SB_OCTUP)
            lvl = 1;
        sk_led_btn(i, lvl);
    }
}

/* ---------------------------------------------------------------- drawing --- */
static char *ui_num(char *p, int v)
{
    char t[12];
    int n = 0;
    uint32_t u;
    if (v < 0) {
        *p++ = '-';
        v = -v;
    }
    u = (uint32_t)v;
    do {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
    } while (u);
    while (n)
        *p++ = t[--n];
    *p = 0;
    return p;
}

static void ui_box(int i, const char *label, const char *val, int frac /* 0..255 */, uint32_t now)
{
    int x = 4 + i * 59, y = 192, w = 55, h = 46;
    uint8_t acc = UI_ACC[i];
    int act = now - ui.k_t[i] < 900u;
    fb_rbox(x, y, w, h, 4, act ? C_PANEL2 : C_PANEL);
    fb_text(x + 6, y + 4, label, act ? C_INK : C_INK2);
    fb_text(x + 6, y + 18, val, C_INK);
    fb_rect(x + 6, y + 36, w - 12, 3, C_BG);
    fb_rect(x + 6, y + 36, (w - 12) * frac / 255, 3, acc);
    if (act)
        fb_pset(x + w - 8, y + 8, acc), fb_rect(x + w - 9, y + 7, 3, 3, acc);
}

static void ui_panel(uint32_t now)
{
    int i;
    char b[16];
    fb_rect(0, SC_H, FB_W, FB_H - SC_H, C_BG);
    for (i = 0; i < 4; i++) {
        const char *label;
        int frac, v;
        b[0] = 0;
        if (ui.page == PG_HOME) {
            label = UI_MAC[i];
            v = sk_s.mac[i];
            frac = v * 2;
            ui_num(b, v * 100 / 127);
        } else if (ui.page == PG_EDIT) {
            uint32_t s = sk_s.sel;
            label = UI_EDITK[i];
            switch (i) {
            case 0: v = sk_s.level[s]; frac = v * 255 / 100; ui_num(b, v); break;
            case 1: v = sk_s.pan[s]; frac = (v + 64) * 2; if (v) { char *e = ui_num(b, v < 0 ? -v : v); *e++ = v < 0 ? 'L' : 'R'; *e = 0; } else b[0] = 'C', b[1] = 0; break;
            case 2: v = sk_s.decay[s]; frac = (v + 6) * 255 / 12; ui_num(b, v); break;
            default:
                frac = 255;
                if (sk_s.type[s] == VT_RES || sk_s.type[s] == VT_BELL) {
                    const char *m = sk_s.type[s] == VT_BELL ? SK_BELL_NAME[sk_s.mat[s] & 3u] : SK_MAT_NAME[sk_s.mat[s] % 6u];
                    int k;
                    for (k = 0; m[k] && k < 8; k++)
                        b[k] = m[k];
                    b[k] = 0;
                } else
                    b[0] = '-', b[1] = 0;
                break;
            }
        } else {
            label = UI_PHYSK[i];
            switch (i) {
            case 0: v = ph.spin; frac = (v + 64) * 2; ui_num(b, v); break;
            case 1: v = ph.grav; frac = v * 2; ui_num(b, v * 100 / 127); break;
            case 2: v = ph.bounce; frac = v * 2; ui_num(b, v * 100 / 127); break;
            default: v = ph.nb; frac = v * 255 / PH_MAXB; ui_num(b, v); break;
            }
        }
        ui_box(i, label, b, ui_clamp(frac, 0, 255), now);
    }
}

static void ui_hud(uint32_t now)
{
    char b[24], *p;
    uint32_t s = sk_s.sel;
    const char *tn = SK_TYPE_NAME[sk_s.type[s]];
    /* the selected voice, top left: its colour, number and kind */
    p = b;
    *p++ = (char)('1' + s);
    *p++ = ' ';
    while (*tn)
        *p++ = *tn++;
    if ((sk_s.mute >> s) & 1u) {
        const char *m = " off";
        while (*m)
            *p++ = *m++;
    }
    *p = 0;
    fb_rect(6, 8, 4, 4, SC_COL[s]);
    fb_text_halo(14, 4, b, C_INK, C_BG);
    /* the tempo and the hold, top right */
    p = ui_num(b, sk_s.bpm);
    *p++ = ' ';
    *p++ = 'B';
    *p++ = 'P';
    *p++ = 'M';
    *p = 0;
    fb_text_halo(FB_W - 6 - fb_text_w(b), 4, b, sk_s.hold ? C_YELLOW : C_INK2, C_BG);
    if (sk_s.hold)
        fb_text_halo(FB_W - 6 - fb_text_w("HOLD"), 18, "HOLD", C_YELLOW, C_BG);
    /* the biome's name, when it has just changed */
    if (now - ui.pop_t < 2600u) {
        const char *n = SK_BIOMES[ui.biome].name;
        int w = fb_text_w(n) + 24, x = (FB_W - w) / 2;
        fb_rbox(x, 62, w, 28, 5, C_PANEL);
        fb_text_c(FB_W / 2, 64, n, C_INK);
        {
            char c[8], *q = ui_num(c, ui.biome + 1);
            *q++ = '/';
            q = ui_num(q, (int)SK_NBIOME);
            fb_text_c(FB_W / 2, 76, c, C_INK2);
        }
    }
}

static void ui_draw(uint32_t now)
{
    sc_update(now);
    if (ui.page == PG_PHYS) {
        ph_update(now);
        ph_draw(now);
    } else
        sc_draw(now);
    ui_hud(now);
    ui_panel(now);
    sk_present();
}

static void ui_init(uint32_t now)
{
    ph_init();
    ui_biome(0, now);
    sk_s.bpm = SK_BIOMES[0].bpm;
    sk_steplen_set();
}
