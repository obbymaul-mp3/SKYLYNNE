/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* SKYLYNNE's interface: the scene fills the screen, four knob boxes sit under it, and everything else is a gesture.
 *
 *   K1..K4      DENSITY  MOTION  COLOUR  SPACE: how much plays, how far it wanders, how bright, how far away
 *   SELECT      which of the six voices you are shaping (the figure points at it)
 *   ALGORITHM   that voice's kind: resonator, pluck, FM bell, pad, air
 *   PRESETS     the biome: a whole soundscape with its scale, tempo and room
 *   EDIT        the voice page: K1..K4 become LEVEL  PAN  SUSTAIN  TONE
 *   SEQ         the physics page: K1..K4 become SPIN  GRAVITY  BOUNCE  BALLS, PRESETS the shape's sides
 *   FX          the room: ROOM  ECHO  CLOUD  HI CUT (a soft top end, the cure for a harsh mix)
 *   SCL         the music: ROOT  SCALE  OCTAVE  WALK (how far the voices wander)
 *   ENV         the selected voice's swell: ATTACK  RELEASE  GLIDE  CURVE
 *   LFO         the slow drifts of every voice: RATE  DEPTH  PAN  VIB
 *   SAVE        eight slots in the flash: K1 slot, K2 load / save / clear, SAVE again does it
 *   HOME        back to the scene (each page's button also toggles it)
 *   OCT- / OCT+ the tempo         PLAY  the voices wait, and the sound rings out
 *   keys 1-6    mute a voice      keys 7-27  play the selected voice on the biome's scale
 *
 * The platform provides (sk_hw.c, the host harness): sk_in_enc(role), sk_in_buttons(&held, &released),
 * sk_in_keys(&held), sk_led_btn(label, level), sk_led_key(n, level). */

/* the panel's labels and encoders */
enum { SB_FX, SB_SCL, SB_ENV, SB_LFO, SB_EDIT, SB_GLO, SB_HOME, SB_SAVE, SB_ARP, SB_SEQ, SB_PLAY, SB_REC, SB_OCTDN, SB_OCTUP, SB_N };
enum { SE_SELECT, SE_ALGO, SE_PRESET, SE_K1, SE_K2, SE_K3, SE_K4, SE_N };
enum { PG_HOME, PG_EDIT, PG_PHYS, PG_FX, PG_SCL, PG_ENV, PG_LFO, PG_SAVE, PG_N };

static int32_t sk_in_enc(uint32_t role);
static void sk_in_buttons(uint32_t *held, uint32_t *pressed);
static uint32_t sk_in_keys(uint32_t *pressed);
static void sk_led_btn(uint32_t label, uint32_t level);
static void sk_led_key(uint32_t n, uint32_t level);
static int sk_store_read(void *dst, uint32_t n);            /* the saved slots: 0 ok */
static int sk_store_write(const void *src, uint32_t n);

static struct {
    uint8_t page, biome;
    uint32_t pop_t, hud_t, k_t[4];       /* when the biome's name, the voice HUD, a knob was last touched */
    uint32_t held;
    uint8_t slot, act;                   /* the SAVE page: the slot (0..7) and what the button does (load, save, clear) */
    uint32_t toast_t;
    char toast[16];
} ui;

/* the saved slots: a header, eight snapshots of a soundscape as the player has shaped it, and a checksum */
#define SV_N 8
#define SV_SLOT 88
static struct {
    uint32_t magic;
    uint8_t slot[SV_N][SV_SLOT];
    uint32_t sum;
} sv;
#define SV_MAGIC 0x31594B53u                 /* "SKY1" */

static const char *const UI_MAC[4] = {"DENSITY", "MOTION", "COLOUR", "SPACE"};
static const char *const UI_EDITK[4] = {"LEVEL", "PAN", "SUSTAIN", "TONE"};
static const char *const UI_PHYSK[4] = {"SPIN", "GRAVITY", "BOUNCE", "BALLS"};
static const char *const UI_PGK[PG_N][4] = {
    {"DENSITY", "MOTION", "COLOUR", "SPACE"}, {"LEVEL", "PAN", "SUSTAIN", "TONE"}, {"SPIN", "GRAVITY", "BOUNCE", "BALLS"},
    {"ROOM", "ECHO", "CLOUD", "HI CUT"}, {"ROOT", "SCALE", "OCTAVE", "WALK"}, {"ATTACK", "RELEASE", "GLIDE", "CURVE"},
    {"RATE", "DEPTH", "PAN", "VIB"}, {"SLOT", "DO", "FROM", "STATE"},
};
static const char *const UI_PGNAME[PG_N] = {"", "VOICE", "PHYSICS", "FX", "SCALE", "ENVELOPE", "LFO", "SAVE"};
static const char *const UI_ACTS[3] = {"LOAD", "SAVE", "CLEAR"};
static const uint8_t UI_ACC[4] = {C_PINK, C_CYAN, C_YELLOW, C_GREEN};
static const uint8_t UI_MACAIM[4] = {2, 0, 1, 5};      /* the part of the scene each macro points at */

static void ui_touch(uint32_t v)
{
    sc_touch_v = (uint8_t)v;
    sc_touch_n++;
}

static uint32_t sv_sum(void)
{
    const uint8_t *p = (const uint8_t *)sv.slot;
    uint32_t i, h = 0x811C9DC5u;
    for (i = 0; i < sizeof sv.slot; i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

static void sv_pack(uint8_t *d, uint32_t biome)
{
    uint32_t i, k = 0;
    d[k++] = 1;
    d[k++] = (uint8_t)biome;
    for (i = 0; i < 4u; i++)
        d[k++] = sk_s.mac[i];
    d[k++] = sk_s.bpm;
    d[k++] = sk_s.root;
    d[k++] = sk_s.scl;
    d[k++] = (uint8_t)sk_s.shift;
    d[k++] = (uint8_t)sk_s.walk;
    for (i = 0; i < 4u; i++)
        d[k++] = sk_s.fx[i];
    for (i = 0; i < 4u; i++)
        d[k++] = sk_s.lfo[i];
    for (i = 0; i < SK_NV; i++) {
        d[k++] = sk_s.type[i];
        d[k++] = sk_s.level[i];
        d[k++] = (uint8_t)sk_s.pan[i];
        d[k++] = (uint8_t)sk_s.sus[i];
        d[k++] = (uint8_t)sk_s.tone[i];
        d[k++] = (uint8_t)sk_s.atk[i];
        d[k++] = (uint8_t)sk_s.rel[i];
        d[k++] = (uint8_t)sk_s.gli[i];
        d[k++] = sk_s.crv[i];
    }
    d[k++] = sk_s.len;
    for (i = 0; i < 8u; i++)
        d[k++] = sk_s.iv[i];
}

static void ui_biome(uint32_t i, uint32_t now);
static void sv_unpack(const uint8_t *d, uint32_t now)
{
    uint32_t i, k = 2, base = d[1] % SK_NBIOME;
    ui_biome(base, now);
    ui.pop_t = now - 10000u;
    for (i = 0; i < 4u; i++)
        sk_s.mac[i] = d[k++] & 127u;
    sk_s.bpm = d[k] < 30u ? 30u : d[k] > 160u ? 160u : d[k];
    k++;
    sk_s.root = d[k++] % 12u;
    {
        uint32_t scl = d[k++];
        uint32_t len = d[2 + 4 + 1 + 1 + 1 + 1 + 1 + 4 + 4 + SK_NV * 9];
        sk_s.shift = (int8_t)d[k++];
        sk_s.walk = (int8_t)d[k++];
        if (scl < SK_NSCALE)
            sk_scale_set(scl);
        else if (len >= 1u && len <= 8u) {
            sk_s.scl = SK_NSCALE;
            sk_s.len = (uint8_t)len;
            for (i = 0; i < 8u; i++)
                sk_s.iv[i] = d[2 + 4 + 1 + 1 + 1 + 1 + 1 + 4 + 4 + SK_NV * 9 + 1 + i];
        }
    }
    sk_s.shift = (int8_t)(sk_s.shift < -2 ? -2 : sk_s.shift > 2 ? 2 : sk_s.shift);
    sk_s.walk = (int8_t)(sk_s.walk < -3 ? -3 : sk_s.walk > 3 ? 3 : sk_s.walk);
    for (i = 0; i < 4u; i++)
        sk_s.fx[i] = d[k++] & 127u;
    for (i = 0; i < 4u; i++)
        sk_s.lfo[i] = d[k++] & 127u;
    for (i = 0; i < SK_NV; i++) {
        sk_s.type[i] = (uint8_t)(d[k++] % VT_N);
        sk_s.level[i] = d[k] > 100u ? 100u : d[k];
        k++;
        sk_s.pan[i] = (int8_t)d[k++];
        sk_s.sus[i] = (int8_t)d[k++];
        sk_s.tone[i] = (int8_t)d[k++];
        sk_s.atk[i] = (int8_t)d[k++];
        sk_s.rel[i] = (int8_t)d[k++];
        sk_s.gli[i] = (int8_t)d[k++];
        sk_s.crv[i] = d[k++] & 127u;
        if (sk_s.sus[i] < -6 || sk_s.sus[i] > 6) sk_s.sus[i] = 0;
        if (sk_s.tone[i] < -6 || sk_s.tone[i] > 6) sk_s.tone[i] = 0;
        if (sk_s.atk[i] < -6 || sk_s.atk[i] > 6) sk_s.atk[i] = 0;
        if (sk_s.rel[i] < -6 || sk_s.rel[i] > 6) sk_s.rel[i] = 0;
        if (sk_s.gli[i] < -6 || sk_s.gli[i] > 6) sk_s.gli[i] = 0;
        if (sk_s.pan[i] < -64 || sk_s.pan[i] > 63) sk_s.pan[i] = 0;
    }
    sk_steplen_set();
}

static void ui_toast(const char *a, uint32_t n, uint32_t now)
{
    char *p = ui.toast;
    while (*a && p < ui.toast + 11)
        *p++ = *a++;
    *p++ = ' ';
    *p++ = (char)('0' + n);
    *p = 0;
    ui.toast_t = now;
}

/* what the SAVE button does on the SAVE page */
static void ui_do_save(uint32_t now)
{
    uint8_t *d = sv.slot[ui.slot];
    if (ui.act == 0u) {                                            /* load */
        if (!d[0])
            ui_toast("EMPTY", ui.slot + 1u, now);
        else {
            sv_unpack(d, now);
            ui_toast("LOADED", ui.slot + 1u, now);
        }
        return;
    }
    if (ui.act == 1u)
        sv_pack(d, ui.biome);
    else {
        uint32_t i;
        for (i = 0; i < SV_SLOT; i++)
            d[i] = 0;
    }
    sv.magic = SV_MAGIC;
    sv.sum = sv_sum();
    ui_toast(sk_store_write(&sv, sizeof sv) == 0 ? (ui.act == 1u ? "SAVED" : "CLEARED") : "FAILED", ui.slot + 1u, now);
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
    {
        static const uint8_t PB[5] = {SB_FX, SB_SCL, SB_ENV, SB_LFO, SB_SAVE};
        static const uint8_t PGS[5] = {PG_FX, PG_SCL, PG_ENV, PG_LFO, PG_SAVE};
        for (i = 0; i < 5u; i++)
            if (pr & (1u << PB[i])) {
                if (ui.page == PG_SAVE && PGS[i] == PG_SAVE)
                    ui_do_save(now);                                /* SAVE again does what K2 says */
                else
                    ui.page = ui.page == PGS[i] ? PG_HOME : PGS[i];
            }
    }
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
            case 2: sk_s.sus[v] = (int8_t)ui_clamp(sk_s.sus[v] + d, -6, 6); break;
            default: sk_s.tone[v] = (int8_t)ui_clamp(sk_s.tone[v] + d, -6, 6); break;
            }
            ui.hud_t = now;
            ui_touch(v);
        } else if (ui.page >= PG_FX) {
            uint32_t v = sk_s.sel;
            switch (ui.page * 4u + i) {
            case PG_FX * 4u + 0: case PG_FX * 4u + 1: case PG_FX * 4u + 2: case PG_FX * 4u + 3:
                sk_s.fx[i] = (uint8_t)ui_clamp(sk_s.fx[i] + d * 2, 0, 127);
                break;
            case PG_SCL * 4u + 0: sk_s.root = (uint8_t)((sk_s.root + 12u * 8u + (uint32_t)d) % 12u); break;
            case PG_SCL * 4u + 1: sk_scale_set((sk_s.scl % (SK_NSCALE + 1u) + (SK_NSCALE + 1u) * 8u + (uint32_t)d) % SK_NSCALE); break;
            case PG_SCL * 4u + 2: sk_s.shift = (int8_t)ui_clamp(sk_s.shift + d, -2, 2); break;
            case PG_SCL * 4u + 3: sk_s.walk = (int8_t)ui_clamp(sk_s.walk + d, -3, 3); break;
            case PG_ENV * 4u + 0: sk_s.atk[v] = (int8_t)ui_clamp(sk_s.atk[v] + d, -6, 6); break;
            case PG_ENV * 4u + 1: sk_s.rel[v] = (int8_t)ui_clamp(sk_s.rel[v] + d, -6, 6); break;
            case PG_ENV * 4u + 2: sk_s.gli[v] = (int8_t)ui_clamp(sk_s.gli[v] + d, -6, 6); break;
            case PG_ENV * 4u + 3: sk_s.crv[v] = (uint8_t)ui_clamp(sk_s.crv[v] + d * 4, 0, 127); break;
            case PG_LFO * 4u + 0: case PG_LFO * 4u + 1: case PG_LFO * 4u + 2: case PG_LFO * 4u + 3:
                sk_s.lfo[i] = (uint8_t)ui_clamp(sk_s.lfo[i] + d * 2, 0, 127);
                break;
            case PG_SAVE * 4u + 0: ui.slot = (uint8_t)ui_clamp(ui.slot + d, 0, SV_N - 1); break;
            case PG_SAVE * 4u + 1: ui.act = (uint8_t)ui_clamp(ui.act + d, 0, 2); break;
            default: break;
            }
            if (ui.page == PG_ENV)
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
            (i == SB_PLAY && sk_s.hold) || (i == SB_FX && ui.page == PG_FX) || (i == SB_SCL && ui.page == PG_SCL) ||
            (i == SB_ENV && ui.page == PG_ENV) || (i == SB_LFO && ui.page == PG_LFO) || (i == SB_SAVE && ui.page == PG_SAVE))
            lvl = 3;
        else if (i == SB_EDIT || i == SB_SEQ || i == SB_HOME || i == SB_PLAY || i == SB_OCTDN || i == SB_OCTUP || i == SB_FX ||
                 i == SB_SCL || i == SB_ENV || i == SB_LFO || i == SB_SAVE)
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
            case 2:
                v = sk_s.sus[s];
                frac = (v + 6) * 255 / 12;
                if (v >= 6)
                    b[0] = 'i', b[1] = 'n', b[2] = 'f', b[3] = 0;
                else
                    ui_num(b, v);
                break;
            default: v = sk_s.tone[s]; frac = (v + 6) * 255 / 12; ui_num(b, v); break;
            }
        } else if (ui.page >= PG_FX) {
            uint32_t s = sk_s.sel;
            label = UI_PGK[ui.page][i];
            frac = 0;
            switch (ui.page * 4u + (uint32_t)i) {
            case PG_FX * 4u + 0: case PG_FX * 4u + 1: case PG_FX * 4u + 2: case PG_FX * 4u + 3:
                v = sk_s.fx[i]; frac = v * 2; ui_num(b, v * 100 / 127); break;
            case PG_SCL * 4u + 0: { const char *m = SK_NOTE_NAME[sk_s.root % 12u]; int k; for (k = 0; m[k]; k++) b[k] = m[k]; b[k] = 0; frac = sk_s.root * 255 / 11; break; }
            case PG_SCL * 4u + 1: {
                const char *m = sk_s.scl < SK_NSCALE ? SK_SCALES[sk_s.scl].name : "Own";
                int k;
                for (k = 0; m[k] && k < 9; k++) b[k] = m[k];
                b[k] = 0;
                frac = sk_s.scl < SK_NSCALE ? sk_s.scl * 255 / (SK_NSCALE - 1u) : 255;
                break;
            }
            case PG_SCL * 4u + 2: v = sk_s.shift; frac = (v + 2) * 255 / 4; if (v > 0) b[0] = '+', ui_num(b + 1, v); else ui_num(b, v); break;
            case PG_SCL * 4u + 3: v = sk_s.walk; frac = (v + 3) * 255 / 6; if (v > 0) b[0] = '+', ui_num(b + 1, v); else ui_num(b, v); break;
            case PG_ENV * 4u + 0: v = sk_s.atk[s]; frac = (v + 6) * 255 / 12; if (v > 0) b[0] = '+', ui_num(b + 1, v); else ui_num(b, v); break;
            case PG_ENV * 4u + 1: v = sk_s.rel[s]; frac = (v + 6) * 255 / 12; if (v > 0) b[0] = '+', ui_num(b + 1, v); else ui_num(b, v); break;
            case PG_ENV * 4u + 2: v = sk_s.gli[s]; frac = (v + 6) * 255 / 12; if (v > 0) b[0] = '+', ui_num(b + 1, v); else ui_num(b, v); break;
            case PG_ENV * 4u + 3: v = sk_s.crv[s]; frac = v * 2; ui_num(b, v * 100 / 127); break;
            case PG_LFO * 4u + 0: case PG_LFO * 4u + 1: case PG_LFO * 4u + 2: case PG_LFO * 4u + 3:
                v = sk_s.lfo[i]; frac = v * 2; ui_num(b, v * 100 / 127); break;
            case PG_SAVE * 4u + 0: v = ui.slot; frac = v * 255 / (SV_N - 1); ui_num(b, v + 1); break;
            case PG_SAVE * 4u + 1: { const char *m = UI_ACTS[ui.act % 3u]; int k; for (k = 0; m[k]; k++) b[k] = m[k]; b[k] = 0; frac = ui.act * 127; break; }
            case PG_SAVE * 4u + 2:
                if (sv.slot[ui.slot][0]) { b[0] = 'B'; ui_num(b + 1, sv.slot[ui.slot][1] % SK_NBIOME + 1u); }
                else b[0] = '-', b[1] = 0;
                break;
            default: { const char *m = sv.slot[ui.slot][0] ? "full" : "empty"; int k; for (k = 0; m[k]; k++) b[k] = m[k]; b[k] = 0; frac = sv.slot[ui.slot][0] ? 255 : 0; break; }
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
    if (ui.page != PG_HOME)
        fb_text_halo((FB_W - fb_text_w(UI_PGNAME[ui.page])) / 2, 4, UI_PGNAME[ui.page], C_INK2, C_BG);
    if (now - ui.toast_t < 1800u && ui.toast[0]) {
        int w = fb_text_w(ui.toast) + 24, x = (FB_W - w) / 2;
        fb_rbox(x, 62, w, 22, 5, C_PANEL);
        fb_text_c(FB_W / 2, 66, ui.toast, C_INK);
    }
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

/* the saved slots, read once at boot (before the audio runs, on the device) */
static void ui_store_load(void)
{
    if (sk_store_read(&sv, sizeof sv) != 0 || sv.magic != SV_MAGIC || sv.sum != sv_sum()) {
        uint32_t i;
        for (i = 0; i < sizeof sv; i++)
            ((uint8_t *)&sv)[i] = 0;
    }
}

static void ui_init(uint32_t now)
{
    ph_init();
    ui_biome(0, now);
    sk_s.bpm = SK_BIOMES[0].bpm;
    sk_steplen_set();
}
