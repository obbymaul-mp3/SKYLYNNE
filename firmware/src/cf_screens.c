/* SPDX-License-Identifier: GPL-3.0-only
 * CoralFlower: the screens' pictures and the popups, then the frame (cf_ui_draw). Included after cf_ui.c. */

#define fcos_ui(a) fl_fcos(a)                       /* (flora.c's integer sine) */
#define fsin_ui(a) fl_fsin(a)
static char cu_name[16], cu_name_sub[12];        /* the chord shown (it stays after the hands let go) */
static uint8_t cu_name_single;                   /* .. it is single notes (no chord button) */
static uint8_t cu_name_live;

static void name_set(uint32_t root, uint32_t mask, int bass_pc, int single, uint32_t n, const uint8_t *note)
{
    cu_name_live = 1;
    cu_name_single = (uint8_t)single;
    if (single) {
        char a[4];
        int k = 0;
        uint32_t i;
        cu_name[0] = 0;
        for (i = 0; i < n && i < 3u; i++) {
            cf_pc_name(note[i] % 12, &cu_spell, a);
            str_cpy(cu_name + k, a, sizeof cu_name - (uint32_t)k);
            k = (int)str_len(cu_name);
            if (i + 1u < n && i < 2u && k < 14)
                cu_name[k++] = ' ', cu_name[k] = 0;
        }
        cu_name_sub[0] = 0;
        return;
    }
    cf_chord_name((int)root, (uint16_t)mask, bass_pc != (int)root ? bass_pc : -1, &cu_spell, cu_name);
    cu_name_sub[0] = 0;
    if (cs.chord.quantize != CF_Q_OFF)
        cf_numeral(cs.chord.scale, cs.chord.tonic, (int)root, cu_name_sub);
}
/* the chord shown: the keys' while they sound, else the sequencer's CHORDS track's while it plays (0.5, the owner:
 * "то, что играется в секвенсоре я бы хотел, чтобы на главной отображалось на цветочке"); it stays after both let go */
static void name_update(void)
{
    const cf_chord_out_t *o = &cf_sn.out;
    if (o->sounding && o->n)
        name_set(o->root_pc, o->mask, cs.bass_on ? o->bass_pc : -1, o->single, o->n, o->note);
    else if (cf_sn.sq_on && cf_sn.sq_dn)
        name_set(cf_sn.sq_droot % 12u, cf_sn.sq_dmask, cf_sn.sq_bn ? cf_sn.sq_bass[cf_sn.sq_bn - 1u] % 12 : -1,
                 cf_sn.sq_dsingle, cf_sn.sq_dn, cf_sn.sq_dnote);
    else
        cu_name_live = 0;
}

/* a sound's number in its row, "007" */
static void snd_num(uint32_t idx, char *b)
{
    b[0] = (char)('0' + (idx + 1u) / 100u % 10u);
    b[1] = (char)('0' + (idx + 1u) / 10u % 10u);
    b[2] = (char)('0' + (idx + 1u) % 10u);
    b[3] = 0;
}
/* the top bar's sound: number, engine, name; the pattern's letter after it when the sound is that pattern's own
 * (0.5: PRESETS / SELECT then change the pattern's, not HOME's) */
static void draw_top_sound(uint32_t part, int xmax)   /* xmax: where the chips on the right begin */
{
    char num[4], l[2], nm[13];
    int x = 4, own, room;
    uint32_t cur = sq_cur_pat(), k;
    own = cur < SQ_NPAT && part < 3u && cfps.r[cur][part].own;
    l[0] = (char)('A' + cur % SQ_NPAT);
    l[1] = 0;
    snd_num(cf_snd_pos(part), num);
    x += fb_text(&CF_FONT_UI, x, Y_TOP + 1, num, FL_INK3, 1) + 5;
    /* the name as fits before the chips (ARP, SEQ, REC), the pattern's letter always after it; short of room the
     * engine's label goes first, then the name's end */
    str_cpy(nm, cf_snd[part].name, sizeof nm);
    room = xmax - x - (own ? 7 + fb_text_w(&CF_FONT_UI, l, 1) : 0);
    k = (uint32_t)fb_text_w(&CF_FONT_UI, cf_engine_name(cf_snd[part].engine), 1) + 5u;
    if (fb_text_w(&CF_FONT_UI, nm, 1) + (int)k <= room) {
        x += fb_text(&CF_FONT_UI, x, Y_TOP + 1, cf_engine_name(cf_snd[part].engine), C_SEC(1), 1) + 5;
        room -= (int)k;
    }
    for (k = str_len(nm); k && fb_text_w(&CF_FONT_UI, nm, 1) > room; )
        nm[--k] = 0;
    x += fb_text(&CF_FONT_UI, x, Y_TOP + 1, nm, FL_INK, 1) + 4;
    if (own) {
        fb_rect(x, Y_TOP + 5, 2, 2, FL_INK3);
        fb_text(&CF_FONT_UI, x + 5, Y_TOP + 1, l, C_ACC(0), 1);
    }
}

#include "cf_views.c"                           /* (the heads, the pager, the central pictures) */
#include "cf_seqv.c"                            /* (the SEQUENCER screens' pictures) */
static void draw_title(const char *t, const char *note);
/* ------------------------------------------------------------------- FX --- */
/* the selected effect's own little picture, x0..x0+w along, its foot on yb, at most h tall */
static void draw_fx_graph(uint32_t e, int x0, int yb, int w, int h)
{
    const uint8_t *v = cs.fx.v[e];
    uint8_t c = cfx_is_on(e) ? C_ACC(1) : FL_INK3, c2 = cfx_is_on(e) ? C_ACC(2) : FL_BG3;
    int x, px = -1, py = 0;
    fb_hline(x0, yb, w, FL_BG3);
    for (x = 0; x < w; x++) {
        int32_t y = 0;
        switch (e) {
        case CFX_FILTER: {                       /* the response: flat, the corner (a bump for the resonance), a slope */
            int cx = (int)v[1] * (w - 1) / 99, d = x - cx, bump = (int)v[2] * h / 160;
            int lp = d < 0 ? h * 2 / 3 : h * 2 / 3 - d * h / 28, a = d < 0 ? -d : d;
            if (a < 8)
                lp += bump * (8 - a) / 8;
            if (v[0] == 1)                       /* band pass: both sides fall */
                lp = h * 2 / 3 - a * h / 28 + (a < 8 ? bump * (8 - a) / 8 : 0);
            else if (v[0] == 2)                  /* high pass: the mirror */
                lp = d > 0 ? h * 2 / 3 : h * 2 / 3 + d * h / 28 + (a < 8 ? bump * (8 - a) / 8 : 0);
            y = lp < 0 ? 0 : lp > h ? h : lp;
            break;
        }
        default:
            break;
        }
        if (e == CFX_FILTER) {
            if (px >= 0)
                fb_line(x0 + px, yb - py, x0 + x, yb - (int)y, c);
            px = x;
            py = (int)y;
        }
    }
    if (e == CFX_REVERB || e == CFX_DELAY) {     /* bars: a tail decaying, or the echoes */
        int i, n = e == CFX_REVERB ? 56 : 12, step = e == CFX_REVERB ? 4 : 6 + (int)v[0] * 40 / 100;
        int32_t a = h * 256 * (50 + (int)v[2]) / 150, k = e == CFX_REVERB ? 205 + (int)v[1] * 45 / 100 : 80 + (int)v[1] * 17 / 10;
        for (i = 0; i < n && i * step < w; i++) {
            int bh = (int)(a >> 8);
            if (e == CFX_REVERB)                 /* (a grain of noise in the tail) */
                bh = bh * (12 + (int)((uint32_t)(i * 2654435761u) >> 29)) / 16;
            if (bh > 0)
                fb_rect(x0 + i * step, yb - bh, e == CFX_REVERB ? 2 : 3, bh, i ? c : C_ACC(0));
            a = a * k / 256;
        }
    } else if (e == CFX_DIST) {                  /* the transfer curve: tanh of the driven input */
        int32_t g = CFX_DRIVE[v[0] > 100u ? 100u : v[0]];
        for (x = 0; x < w; x++) {
            int32_t in = (x * 2 - w) * 32768 / w, y = softclip((int32_t)(((int64_t)in * g) >> 8));
            int yy = yb - h / 2 - (int)(y * (h / 2) / 32768);
            if (px >= 0)
                fb_line(x0 + px, py, x0 + x, yy, c);
            px = x;
            py = yy;
        }
    } else if (e == CFX_CHORUS || e == CFX_PHASER) {   /* the sweep: a sine moving with the rate */
        uint32_t ph = (uint32_t)cu.now * (uint32_t)(20 + v[0] * 6) * 1000u;
        int amp = (h / 2 - 1) * (10 + (int)v[1]) / 110;
        for (x = 0; x < w; x++) {
            int yy = yb - h / 2 - (osc_sine(ph + (uint32_t)x * (e == CFX_PHASER ? 60000000u : 28000000u)) * amp) / 32768;
            if (px >= 0)
                fb_line(x0 + px, py, x0 + x, yy, x & 1 ? c : c2);
            px = x;
            py = yy;
        }
    }
}

/* GRN's picture: a wizard's wand. The grains are its sparkles. The shaft is the ring (the handle end the oldest sound,
 * the tip the newest): the lit stretch is where the grains read (POSITION) and how long they are (SIZE). Every grain the
 * audio side starts flies off the tip as a spark: to the side it is panned to, up at the speed of its pitch (a reverse
 * grain falls back in), a layer voice in the hat's other colour as a bigger star. While grains play the wand trembles,
 * the tip glows with their number and flashes at each new one; on the tempo (SYNC) every step also throws a ring; frozen
 * the tip is a still crystal that twinkles; with MIX 0 the wand is dim and quiet. The colours are the hat's (palette
 * 60..63: they follow the chord's root, like the wizard's) and the ice */
#define CF_KEYSTRIP 0
#define CF_SKYHOME 1
static void spell_disc(int cx, int cy, int rx, int ry, int col);
static void spell_spark(int x, int y, int r);
static void wand_glow(int cx, int cy, int r, uint8_t c)       /* a dithered disc: a glow that lets the picture through */
{
    int x, y;
    for (y = -r; y <= r; y++)
        for (x = -r; x <= r; x++)
            if (x * x + y * y <= r * r && ((x + y) & 1) == 0)
                fb_pset(cx + x, cy + y, c);
}
static void wand_ring(int cx, int cy, int r, uint8_t c)
{
    int x, y, r2 = r * r, r3 = (r - 1) * (r - 1);
    for (y = -r; y <= r; y++)
        for (x = -r; x <= r; x++) {
            int d = x * x + y * y;
            if (d <= r2 && d >= r3 && ((x + y) & 1) == 0)
                fb_pset(cx + x, cy + y, c);
        }
}
static void draw_grain_graph(int x0, int top, int w, int h)
{
    static uint32_t last_hits, t_hit;
    const int bot = top + h, tx = x0 + w * 60 / 100, ty = top + h * 42 / 100, hx = x0 + 6, hy = bot - 8;
    int on = cgr.mix != 0, frozen = cgr.freeze != 0, na = 0, i, k, pxv[17], pyv[17];
    uint32_t now = cu.now, since, hits = cg.hits;
    int amp, glow, u0, ul;
    if (hits != last_hits) {                                  /* a grain started since the last frame */
        last_hits = hits;
        t_hit = now;
    }
    since = now - t_hit;
    for (k = 0; k < CG_NG; k++)
        na += cg.g[k].on;
    fb_rbox(x0 - 4, top - 2, w + 8, bot - top + 4, 3, cf_panel(1));      /* a dark pane behind: the wand reads on it */
    fb_clip(x0 - 4, top - 2, x0 + w + 4, bot + 2);
    /* the wobble: a standing wave along the shaft, stronger the more grains play, still when frozen or off */
    amp = (!on || frozen) ? 0 : (na + 1) / 2 + (since < 120u ? 1 : 0);
    amp = amp > 3 ? 3 : amp;
    for (i = 0; i <= 16; i++) {
        int sx = hx + (tx - hx) * i / 16, sy = hy + (ty - hy) * i / 16;
        int wv = amp ? (osc_sine(now * 9000000u + (uint32_t)i * 120000000u) * amp * i) / (32768 * 16) : 0;
        pxv[i] = sx;
        pyv[i] = sy + wv;
    }
    /* the tip's glow: by MIX and by the grains playing, a flash at each new grain */
    glow = on ? 3 + (int)cgr.mix / 30 + na + (since < 160u ? (int)(160u - since) / 40 : 0) : 2;
    if (frozen)
        glow += 3;
    wand_glow(tx, pyv[16], glow + 4, FL_S2);
    wand_glow(tx, pyv[16], glow + 1, on ? (cgr.layer ? FL_I_ICE3 : FL_S3) : FL_S2);
    if (on && cgr.sync) {                                     /* on the tempo: a ring thrown at every step */
        uint32_t tk = since < 400u ? since : 400u;
        if (since < 400u)
            wand_ring(tx, pyv[16], 8 + (int)tk / 12, FL_S3);
    }
    /* the shaft: three lines (light, body, shade); the grain window lit, the handle's grip dark */
    u0 = (int)cgr.pos * 11 / 100;                              /* segments from the tip: POSITION */
    ul = 1 + (int)cgr.size * 5 / 100;                          /* .. and SIZE */
    for (i = 0; i < 16; i++) {
        int seg = 15 - i;                                      /* (i from the handle; seg from the tip) */
        int lit = on && seg >= u0 && seg < u0 + ul;
        int grip = i < 3;
        uint8_t c1 = lit ? FL_I_ICE : grip ? FL_BG3 : FL_S1, c0 = lit ? FL_S3 : grip ? FL_INK3 : FL_S0,
                c2 = lit ? FL_S3 : grip ? FL_I_OUT : FL_S2;
        if (!on) {
            c1 = grip ? FL_BG3 : FL_S0;
            c0 = grip ? FL_INK3 : FL_S2;
            c2 = FL_I_OUT;
        }
        fb_line(pxv[i], pyv[i] - 1, pxv[i + 1], pyv[i + 1] - 1, c1);
        fb_line(pxv[i], pyv[i], pxv[i + 1], pyv[i + 1], c0);
        fb_line(pxv[i], pyv[i] + 1, pxv[i + 1], pyv[i + 1] + 1, c2);
    }
    fb_line(pxv[3], pyv[3] - 2, pxv[3], pyv[3] + 2, FL_I_OUT);     /* the ferrule between grip and shaft */
    /* the tip: a four-point star, or while frozen a crystal */
    if (frozen) {
        int cy = pyv[16], tw = (now / 180u) & 1u;
        for (k = -5; k <= 5; k++) {
            int wd = 5 - (k < 0 ? -k : k);
            fb_hline(tx - wd / 2 - 1, cy + k, wd + 2, FL_S3);
            fb_hline(tx - wd / 3, cy + k, wd * 2 / 3 + 1, FL_I_ICE);
        }
        fb_vline(tx, cy - 5, 11, FL_INK);
        if (tw)
            spell_spark(tx + 7, cy - 6, 2);
        else
            spell_spark(tx - 8, cy + 5, 2);
    } else {
        spell_spark(tx, pyv[16], on ? 3 + (since < 100u) : 2);
        fb_pset(tx, pyv[16], FL_INK);
    }
    /* the sparks: every grain started throws one (a grain lasts 10 .. 250 ms, a spark half a second: it is the
     * picture's, not the sound's; the audio side keeps the last eight starts in cg.ev). A reverse grain's falls */
    {
        static struct { uint32_t t; int8_t pan; uint8_t rr, kind, rev; } sp[24];
        static uint32_t seen;
        static uint8_t nxt;
        uint32_t nn = hits - seen;
        if (nn > 8u)
            nn = 8u;
        for (k = 0; k < (int)nn; k++) {
            uint32_t e = (hits - nn + (uint32_t)k) & 7u;
            sp[nxt].t = now;
            sp[nxt].pan = cg.ev[e].pan;
            sp[nxt].rr = cg.ev[e].rr;
            sp[nxt].kind = cg.ev[e].kind;
            sp[nxt].rev = cg.ev[e].rev;
            nxt = (uint8_t)((nxt + 1u) % 24u);
        }
        seen = hits;
        for (k = 0; k < 24; k++) {
            uint32_t life = cgr.sync ? 900u : 650u, age = now - sp[k].t;
            int f, dx, dy, rise, sx, sy;
            uint8_t c;
            if (!sp[k].t || age >= life) {
                continue;
            }
            f = (int)(age * 256u / life);
            rise = 12 + (int)sp[k].rr / 5;                     /* pitch 1.0 -> 38 px, 0.5 -> 25, 4.0 -> 60 px */
            if (rise > 52)
                rise = 52;
            dx = sp[k].pan * f * 56 / (128 * 256) + f * 22 / 256;
            dy = sp[k].rev ? f * 22 / 256 + (f * f) / 4096 : -(f * rise) / 256 + (f * f) / 6000;
            sx = tx + dx;
            sy = pyv[16] + dy;
            c = f < 70 ? FL_INK : f < 150 ? FL_I_ICE : f < 210 ? FL_I_ICE2 : FL_I_ICE3;
            if (sp[k].kind) {                                  /* a layer voice: a bigger star in the hat's other colour */
                int r = f < 120 ? 3 : f < 200 ? 2 : 1;
                for (i = 1; i <= r; i++) {
                    uint8_t cc = (uint8_t)(i == 1 ? FL_S1 : FL_S3);
                    fb_pset(sx - i, sy, cc);
                    fb_pset(sx + i, sy, cc);
                    fb_pset(sx, sy - i, cc);
                    fb_pset(sx, sy + i, cc);
                }
                fb_pset(sx, sy, FL_INK);
            } else {
                int r = f < 110 ? 2 : 1;
                for (i = 1; i <= r; i++) {
                    fb_pset(sx - i, sy, c);
                    fb_pset(sx + i, sy, c);
                    fb_pset(sx, sy - i, c);
                    fb_pset(sx, sy + i, c);
                }
                fb_pset(sx, sy, c);
            }
        }
    }
    /* frozen: the held moment twinkles round the crystal; off: a faint star now and then */
    if (frozen) {
        for (k = 0; k < 5; k++) {
            uint32_t hh = (uint32_t)(k + 1) * 2654435761u + (now >> 9) * 40503u;
            int ax = tx - 30 + (int)((hh >> 8) & 63u), ay = pyv[16] - 22 + (int)((hh >> 18) & 31u);
            fb_pset(ax, ay, FL_I_ICE);
        }
    } else if (!on && ((now >> 8) & 7u) == 0)
        fb_pset(tx + 5, pyv[16] - 6, FL_I_ICE2);
    fb_clip(0, Y_VIEW, FB_W, Y_KNOBS);
    /* what the rhythm and the layer are, small, at the handle */
    if (on && (cgr.sync || cgr.layer)) {
        char t[16];
        t[0] = 0;
        if (cgr.sync)
            str_cpy(t, CG_SYNC_NAME[cgr.sync % 8u], sizeof t);
        if (cgr.layer) {
            if (t[0])
                str_cpy(t + str_len(t), "  ", 3);
            str_cpy(t + str_len(t), CG_LAYER_NAME[cgr.layer % 5u], (uint32_t)(sizeof t - str_len(t)));
        }
        fb_text(&CF_FONT_UI, x0 + 2, top + 2, t, FL_INK2, 1);
    }
}

/* ---- the wizard's hands (REVERB and PHASER's pictures) ---- */
/* the pane the FX pictures sit in (the space the key strip had) */
static void fx_pane(int x, int y, int w, int h)
{
    fb_rbox(x - 4, y - 2, w + 8, h + 4, 3, cf_panel(1));
}
static int wz_cos(uint32_t ph) { return osc_sine(ph + 0x40000000u); }
/* one hand at (cx, cy) with its fingers pointing along dir (+1 right, -1 left): the sleeve behind it in the hat's colour
 * out to the pane's edge, the palm and four fingers in ice, fanned by `spread` (0 .. 8) */
static void wz_hand(int cx, int cy, int dir, int spread, int sleeve)
{
    static const uint8_t FL[4] = {7, 9, 9, 7};
    const int sc = 2;                                                         /* drawn at twice the size */
    int wx = cx - dir * 6 * sc, i, t, sx = dir > 0 ? wx - sleeve : wx, xl = dir > 0 ? cx - 5 * sc : cx - 4 * sc;
    int fx = cx + dir * 5 * sc, ph = 12 * sc;
    fb_rbox(sx - 1, cy - 9 * sc - 1, sleeve + 2, 18 * sc + 2, 4, FL_I_OUT);   /* the sleeve and its cuff */
    fb_rbox(sx, cy - 9 * sc, sleeve, 18 * sc, 4, FL_S0);
    fb_rect(sx + 2, cy - 9 * sc + 2, sleeve - 4, 3, FL_S1);
    fb_rect(sx + 2, cy + 9 * sc - 6, sleeve - 4, 4, FL_S2);
    fb_rect(dir > 0 ? wx - 6 : wx, cy - 9 * sc, 6, 18 * sc, FL_S1);
    fb_rect(dir > 0 ? wx - 6 : wx + 5, cy - 9 * sc, 1, 18 * sc, FL_I_OUT);
    fb_rbox(xl - 1, cy - ph / 2 - 1, 10 * sc + 2, ph + 2, 4, FL_I_OUT);       /* the palm */
    fb_rbox(xl, cy - ph / 2, 10 * sc, ph, 3, FL_I_ICE);
    fb_hline(xl + 2, cy + ph / 2 - 3, 10 * sc - 4, FL_I_ICE2);
    fb_hline(xl + 2, cy + ph / 2 - 2, 10 * sc - 4, FL_I_ICE2);
    for (i = 0; i < 4; i++) {                                                 /* the fingers: outline, then fill */
        int y0 = cy - ph / 2 + 2 + i * 3 * sc, y1 = y0 + (i * 2 - 3) * spread * sc / 4, tx = fx + dir * FL[i] * sc;
        fb_line(fx, y0 - 1, tx, y1 - 1, FL_I_OUT);
        fb_line(fx, y0 + 2 * sc - 1, tx, y1 + 2 * sc - 1, FL_I_OUT);
        for (t = 0; t < 2; t++)
            fb_pset(tx + dir, y1 + t * 2, FL_I_OUT);
        fb_pset(tx + dir, y1 + 1, FL_I_OUT);
        fb_pset(tx + dir, y1 + 2, FL_I_OUT);
    }
    for (i = 0; i < 4; i++) {
        int y0 = cy - ph / 2 + 2 + i * 3 * sc, y1 = y0 + (i * 2 - 3) * spread * sc / 4, tx = fx + dir * FL[i] * sc;
        for (t = 0; t < 2 * sc - 1; t++)
            fb_line(fx, y0 + t, tx, y1 + t, t < sc - 1 ? FL_I_ICE : FL_I_ICE2);
    }
    {                                                                         /* the thumb */
        int bx = cx - dir * 2 * sc, by = cy - ph / 2, ex = cx + dir * 5 * sc, ey = by - 6 * sc - spread;
        for (t = -1; t <= 2 * sc - 1; t++)
            fb_line(bx, by + (t < 0 || t > 2 * sc - 2 ? t : 1), ex, ey + t, t < 0 || t > 2 * sc - 2 ? FL_I_OUT : FL_I_ICE);
    }
}
/* REVERB: both hands raised round a spell that grows with the reverb (MIX) and sends out echo rings (DECAY: more of
 * them, slower to go); the hall and the plate add runes circling it. Off: the hands low and apart, a cold ember */
static void draw_reverb_hands(int x0, int top, int w, int h)
{
    const uint8_t *v = cs.fx.v[CFX_REVERB];
    int on = cfx_is_on(CFX_REVERB), mix = v[2] > 100u ? 100 : (int)v[2], dec = v[1] > 100u ? 100 : (int)v[1];
    int type = v[0] > 2u ? 2 : (int)v[0], cx = x0 + w / 2, i, n, R, cy, hx, hy, tr = 0;
    uint32_t now = cu.now;
    R = on ? 4 + mix * 26 / 100 + (osc_sine(now * 6000000u) * 2) / 32768 : 3;
    cy = top + h * 40 / 100;
    hx = R + 36 + (on ? 0 : 14);
    hy = cy + 14 + (on ? 0 : 10);
    if (on && mix > 40)
        tr = (int)((now >> 5) & 1u) * (mix > 75 ? 2 : 1) - 1;               /* the hands tremble with a big spell */
    fx_pane(x0, top, w, h);
    fb_clip(x0 - 4, top - 2, x0 + w + 4, top + h + 2);
    if (on) {
        wand_glow(cx, cy, R + 8, FL_S2);
        wand_glow(cx, cy, R + 4, FL_S3);
    }
    n = on ? 1 + dec / 34 : 0;                                                 /* the echoes: rings going out, fading */
    for (i = 0; i < n; i++) {
        uint32_t per = 800u + (uint32_t)dec * 12u, ph = (now + (uint32_t)i * per / (uint32_t)(n + 1)) % per;
        int rr = R + 3 + (int)(ph * 22u / per);
        wand_ring(cx, cy, rr, ph < per / 3u ? FL_I_ICE : ph < per * 2u / 3u ? FL_I_ICE2 : FL_I_ICE3);
    }
    spell_disc(cx, cy, R + 1, R + 1, on ? FL_I_OUT : FL_S2);                  /* the orb */
    spell_disc(cx, cy, R, R, on ? FL_S3 : FL_S2);
    if (on) {
        spell_disc(cx, cy, R * 3 / 4, R * 3 / 4, FL_I_ICE3);
        spell_disc(cx - R / 5, cy - R / 5, R / 2, R / 2, FL_I_ICE2);
        spell_disc(cx - R / 4, cy - R / 3, R / 5 + 1, R / 5 + 1, FL_I_ICE);
    }
    if (on) {                                                                  /* the runes circling it */
        int nr = 2 + type * 2 + mix / 40;
        for (i = 0; i < nr; i++) {
            uint32_t ph = now * (3000000u + (uint32_t)type * 800000u) + (uint32_t)i * (0xFFFFFFFFu / (uint32_t)nr);
            int rx = R + 10, ex = (osc_sine(ph) * rx) / 32768, ey = (wz_cos(ph) * (rx * 6 / 10)) / 32768;
            spell_spark(cx + ex, cy + ey, i & 1 ? 1 : 2);
        }
    }
    wz_hand(cx - hx, hy + tr, 1, on ? 2 + mix / 14 : 0, cx - hx - x0 + 2);     /* the hands, palms to the spell */
    wz_hand(cx + hx, hy - tr, -1, on ? 2 + mix / 14 : 0, x0 + w - (cx + hx) + 2);
    fb_clip(0, Y_VIEW, FB_W, Y_KNOBS);
}
/* PHASER: one hand shooting energy beams to the right: RATE brings more beams, faster, and more hectic (their aim
 * jumps about more often and further); DEPTH makes them wave; MIX their glow. Off: the hand low, a single faint spark */
static void draw_phaser_hand(int x0, int top, int w, int h)
{
    const uint8_t *v = cs.fx.v[CFX_PHASER];
    int on = cfx_is_on(CFX_PHASER), rate = v[0] > 100u ? 100 : (int)v[0], depth = v[1] > 100u ? 100 : (int)v[1];
    int mix = v[2] > 100u ? 100 : (int)v[2], cy = top + h / 2 + (on ? 0 : 14), hxc = x0 + 46, fx = hxc + 10 + 18, nb, i;
    int xend = x0 + w - 6, plen = xend - fx;
    uint32_t now = cu.now;
    fx_pane(x0, top, w, h);
    fb_clip(x0 - 4, top - 2, x0 + w + 4, top + h + 2);
    nb = on ? 1 + rate * 7 / 100 : 0;
    for (i = 0; i < nb; i++) {
        uint32_t seed = (uint32_t)(i + 1) * 2654435761u, re = now / (uint32_t)(260 - rate * 2) + seed;   /* hectic: the aim re-rolls faster */
        int span = h / 2 - 8, yb = cy + (nb > 1 ? (i * 2 - (nb - 1)) * span / nb : 0);
        int jit = (int)((re * 2246822519u) >> 24) - 128, ye;
        uint32_t pos = (now * (uint32_t)(40 + rate * 5) / 1000u) * 256u / (uint32_t)plen + (uint32_t)i * 97u, t;
        int seg, px = -1, py = 0;
        ye = yb + jit * (rate * span / 100) / 160;                              /* the end of the beam, shaken by the rate */
        ye = ye < top + 4 ? top + 4 : ye > top + h - 4 ? top + h - 4 : ye;
        t = pos % 330u;                                                         /* (the head runs a little past the end) */
        if (mix > 25) {                                                         /* the faint path under the beam */
            int xx;
            for (xx = fx; xx < xend; xx += 3)
                fb_pset(xx, cy + (ye - cy) * (xx - fx) / plen, FL_S2);
        }
        for (seg = 0; seg <= 12; seg++) {                                       /* a beam: a head and a fading tail */
            int tt = (int)t - seg * 12, X, Y, wob;
            uint8_t col;
            if (tt < 0 || tt > 256)
                continue;
            X = fx + plen * tt / 256;
            wob = (osc_sine(now * 12000000u + (uint32_t)i * 900000000u + (uint32_t)tt * 14000000u) * (depth * 7 / 100)) / 32768;
            Y = cy + (ye - cy) * tt / 256 + wob;
            col = seg < 2 ? FL_INK : seg < 4 ? FL_I_ICE : seg < 7 ? FL_I_ICE2 : seg < 10 ? FL_S3 : FL_S2;
            if (px >= 0) {
                fb_line(px, py, X, Y, col);
                if (seg < 7)
                    fb_line(px, py + 1, X, Y + 1, seg < 3 ? FL_I_ICE2 : seg < 5 ? FL_I_ICE3 : FL_S2);
            }
            px = X;
            py = Y;
            if (seg == 0)
                spell_spark(X, Y, 1 + (rate > 60));
        }
    }
    if (on) {                                                                   /* the charge at the fingertips */
        int gr = 4 + rate / 18 + (int)((now >> 6) & 1u) + mix / 40;
        wand_glow(fx, cy, gr + 3, FL_S2);
        wand_glow(fx, cy, gr, FL_S3);
        spell_spark(fx + 2, cy, 2 + (rate > 50));
    } else if (((now >> 8) & 3u) == 0)
        fb_pset(fx + 3, cy - 3, FL_I_ICE2);
    wz_hand(hxc, cy, 1, on ? 3 + rate / 16 : 0, hxc - x0 + 2);
    fb_clip(0, Y_VIEW, FB_W, Y_KNOBS);
}

/* FX: the chain as a row of tiles in its order (on: raised, off: dim), the sound running along it, the selected
 * effect named above and pictured below. After the chain one more tile, GRN: the granular effect on everything that
 * plays (cf_gran.c, three pages that ALGORITHM turns) */
static void draw_fx(void)
{
    int grain = cfx_on_grain();
    uint32_t sel = cu.fx_pos % (CFX_N + 1u), e = cs.fx.order[(sel < CFX_N ? sel : CFX_N - 1u) % CFX_N] % CFX_N, i;
    int ntile = CFX_N + (CF_OLD_GRAIN ? 1 : 0), tw = CF_OLD_GRAIN ? 29 : 34, gap = CF_OLD_GRAIN ? 3 : 4;
    int x0 = 8, y0 = Y_VIEW + 58, run = ntile * tw + (ntile - 1) * gap;
    char sub[24];
    flora_render(&flo, &fl_cv);
    draw_plant(Y_VIEW);
    fb_veil(0, Y_VIEW, FB_W, H_VIEW, FL_BG);   /* (the plant faint: the picture leads) */
    fb_rect(0, Y_VIEW + H_VIEW, FB_W, Y_KNOBS - Y_VIEW - H_VIEW, FL_BG);   /* (the strip under it, no keys now) */
    if (grain) {
        str_cpy(sub, cgr.mix ? "on  " : "off  ", sizeof sub);
        str_cpy(sub + str_len(sub), CG_PAGE_NAME[cfx_gpage % CG_NPAGE], 8);
        draw_title("Grain", sub);
    } else {
        str_cpy(sub, e == CFX_FILTER ? CFX_FTYPE[cs.fx.v[e][0] % 3u] : e == CFX_REVERB ? CFX_RTYPE[cs.fx.v[e][0] % 3u] : "",
                sizeof sub);
        if (sub[0])
            str_cpy(sub + str_len(sub), "  ", 3);
        str_cpy(sub + str_len(sub), cfx_is_on(e) ? "on" : "off", 4);
        draw_title(CFX_NAME[e], sub);
    }
    fb_hline(x0, y0 + 13, run, FL_BG3);          /* the signal's way, a dot running along it */
    {
        int d = (int)((cu.now / 10u) % (uint32_t)run);
        fb_rect(x0 + d, y0 + 12, 2, 3, C_ACC(1));
    }
    for (i = 0; i < (uint32_t)ntile; i++) {
        int g = i >= CFX_N;
        uint32_t id = g ? 0u : cs.fx.order[i] % CFX_N;
        int x = x0 + (int)i * (tw + gap), on = g ? cgr.mix != 0 : cfx_is_on(id);
        fb_rbox(x, y0, tw, 26, 2, on ? cf_panel(2) : cf_panel(1));
        if (i == sel)
            fb_rframe(x, y0, tw, 26, C_ACC(1));
        fb_text_c(&CF_FONT_UI, x + tw / 2, y0 + 3, g ? "GRN" : CFX_SHORT[id], on ? FL_INK : FL_INK3, 1);
        fb_rect(x + tw / 2 - 3, y0 + 19, 6, 2, on ? C_ACC(1) : FL_BG3);
    }
    fb_rect(x0 + (int)sel * (tw + gap) + tw / 2 - 1, y0 + 29, 3, 2, C_ACC(1));   /* (under the selected one) */
    if (grain)
        draw_grain_graph(x0, Y_VIEW + 92, run, Y_KNOBS - 4 - (Y_VIEW + 92));
    else if (e == CFX_REVERB)
        draw_reverb_hands(x0, Y_VIEW + 92, run, Y_KNOBS - 4 - (Y_VIEW + 92));
    else if (e == CFX_PHASER)
        draw_phaser_hand(x0, Y_VIEW + 92, run, Y_KNOBS - 4 - (Y_VIEW + 92));
    else {
        fx_pane(x0, Y_VIEW + 92, run, Y_KNOBS - 4 - (Y_VIEW + 92));
        draw_fx_graph(e, x0, Y_KNOBS - 14, run, 70);
    }
    {
        static const char *const H[4] = {"SELECT", "pick", "ALGO", "move"};
        static const char *const HG[4] = {"SELECT", "pick", "ALGO", "page"};
        draw_hints(grain ? HG : H, 2);
    }
}

/* ------------------------------------------------------------------ HOME --- */
static int spell_diff(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a != *b;
}

/* the chord's name as a spell: an ice cloud left of the wizard puffs up round the whole name (root big, the quality
 * after it) whenever the chord changes, with a burst of sparkles, and twinkles gently while it stays */
static void spell_disc(int cx, int cy, int rx, int ry, int col)
{
    int x, y;
    if (rx < 1 || ry < 1)
        return;
    for (y = -ry; y <= ry; y++)
        for (x = -rx; x <= rx; x++)
            if (x * x * ry * ry + y * y * rx * rx <= rx * rx * ry * ry)
                fb_pset(cx + x, cy + y, (uint8_t)col);
}
static void spell_spark(int x, int y, int r)
{
    int k;
    for (k = -r; k <= r; k++) {
        fb_pset(x + k, y, 4);
        fb_pset(x, y + k, 4);
    }
    if (r > 1) {
        fb_pset(x - 1, y - 1, 54);
        fb_pset(x + 1, y - 1, 54);
        fb_pset(x - 1, y + 1, 54);
        fb_pset(x + 1, y + 1, 54);
    }
}
static void draw_spell(void)
{
    static char last[16];
    static uint32_t t0;
    char rb[8], sb[16], full[16];
    int n = cu_name_single ? (int)str_len(cu_name) : 1 + (cu_name[0] && (cu_name[1] == '#' || cu_name[1] == 'b'));
    int rs = 4, ss = 2, rw, sw, w, hw, hh, cx, cy, i, grow, th, ry0, sy0;
    uint32_t age;
    str_cpy(full, cu_name, sizeof full);
    if (spell_diff(full, last)) {
        str_cpy(last, full, sizeof last);
        t0 = cu.now;
    }
    age = cu.now - t0;
    str_cpy(rb, cu_name, (uint32_t)(n < 4 ? n : 4) + 1u);
    fb_signs(rb, rb, sizeof rb);
    fb_signs(cu_name + n, sb, sizeof sb);
    rw = fb_text_w(&CF_FONT_UI, rb, rs);
    if (rw > 46) {                                /* a root like "Db" or a note list: a size down */
        rs = 3;
        rw = fb_text_w(&CF_FONT_UI, rb, rs);
    }
    sw = sb[0] ? fb_text_w(&CF_FONT_UI, sb, ss) : 0;
    if (sw > 56) {                                /* a long quality: smaller, still on its own line */
        ss = 1;
        sw = fb_text_w(&CF_FONT_UI, sb, ss);
    }
    w = rw > sw ? rw : sw;
    th = 8 * rs + (sb[0] ? 4 + 8 * ss : 0);       /* the caps' height of the block */
    hw = w / 2 + 12;
    if (hw > 38)
        hw = 38;
    hh = th / 2 + 9;
    cx = 3 + hw;
    cy = Y_VIEW + 62 - (age < 250u ? (int)((250u - age) / 20u) : 0);
    grow = age < 260u ? (int)(age * 256u / 260u) : 256;     /* the cloud puffs up in a quarter second */
    {
        int pw = hw * grow / 256, ph = hh * grow / 256;
        spell_disc(cx, cy + 3, pw, ph, 55);                  /* the shade underneath */
        spell_disc(cx - pw * 6 / 10, cy + 3, pw * 5 / 10, ph * 7 / 10, 54);
        spell_disc(cx + pw * 6 / 10, cy + 3, pw * 5 / 10, ph * 7 / 10, 54);
        spell_disc(cx - pw / 3, cy - ph * 5 / 10, pw * 5 / 10, ph * 6 / 10, 54);
        spell_disc(cx + pw / 4, cy - ph * 6 / 10, pw * 45 / 100, ph * 55 / 100, 54);
        spell_disc(cx, cy, pw, ph, 54);
        spell_disc(cx, cy, pw * 9 / 10, ph * 9 / 10, 53);
        spell_disc(cx - pw / 3, cy - ph * 5 / 10, pw * 4 / 10, ph * 5 / 10, 53);
    }
    ry0 = cy - th / 2;                                       /* the caps' top of the root */
    sy0 = ry0 + 8 * rs + 4;
    if (age > 120u) {
        fb_text_c(&CF_FONT_UI, cx, ry0 - 3 * rs, rb, 52, rs);
        if (sb[0])
            fb_text_c(&CF_FONT_UI, cx, sy0 - 3 * ss, sb, 52, ss);
    }
    for (i = 0; i < 11; i++) {                               /* twinkles round the cloud, forever */
        uint32_t ph = (cu.now / 200u + (uint32_t)i * 5u) % 9u;
        if (ph < 3u) {
            uint32_t h = (uint32_t)i * 2654435761u + (cu.now / 1800u) * 40503u;
            int sx = cx + (int)(h % (uint32_t)(2 * hw + 20)) - hw - 10, sy = cy + (int)((h >> 8) % (uint32_t)(2 * hh + 16)) - hh - 8;
            spell_spark(sx, sy, ph == 1u ? 2 : 1);
        }
    }
    if (age < 800u) {                                        /* the burst on a new chord */
        for (i = 0; i < 16; i++) {
            uint32_t h = (uint32_t)i * 40503u + 977u;
            int ang = (int)(h % 1024u);
            int d = (int)(age / 12u) + 6 + (int)(h % 13u);
            int sx = cx + ((fl_fsin(ang) * d * hw / 20) >> 14), sy = cy + ((fl_fsin(ang + 256) * d * hh / 20) >> 14);
            spell_spark(sx, sy, age < 400u ? 2 : 1);
        }
    }
}

#include "sky_view.c"                       /* SKYLYNNE: the skyline home screen */
static void draw_home(void)
{
    if (CF_SKYHOME) {
        draw_sky_home();
        return;
    }
    char key[16], k2[8];
    flora_render(&flo, &fl_cv);
    draw_plant(Y_VIEW);
    name_update();
    if (cu_name[0])
        draw_spell();
    /* the key in the corner */
    if (cs.chord.quantize != CF_Q_OFF) {
        kpc(k2, cs.chord.tonic);
        str_cpy(key, k2, sizeof key);
        str_cpy(key + str_len(key), " ", 2);
        str_cpy(key + str_len(key), CF_SCALES[cs.chord.scale].shrt, 8);
    } else {
        str_cpy(key, "CHROMATIC", sizeof key);
    }
    fb_text_halo(&CF_FONT_UI, 234 - fb_text_w(&CF_FONT_UI, key, 1), Y_VIEW + 4, key, FL_INK2, FL_BG, 1);
    if (cf_sn.out.hold)
        fb_text_halo(&CF_FONT_UI, 234 - fb_text_w(&CF_FONT_UI, "HOLD", 1), Y_VIEW + 32, "HOLD", C_ACC(0), FL_BG, 1);
    if (cs.chord.octave) {
        char b[8];
        b[0] = 'O';
        b[1] = 'C';
        b[2] = 'T';
        b[3] = ' ';
        b[4] = cs.chord.octave > 0 ? '+' : '-';
        b[5] = (char)('0' + (cs.chord.octave > 0 ? cs.chord.octave : -cs.chord.octave));
        b[6] = 0;
        fb_text_halo(&CF_FONT_UI, 234 - fb_text_w(&CF_FONT_UI, b, 1), Y_VIEW + 18, b, C_ACC(1), FL_BG, 1);
    }
    if (cf_sn.running) {                       /* the beat */
        int i;
        for (i = 0; i < cs.beats; i++) {
            int on = i == (cf_sn.beat + cs.beats - 1) % cs.beats;
            fb_rect(234 - (cs.beats - i) * 6, Y_VIEW + H_VIEW - 8, 4, 4, on ? C_ACC(0) : FL_BG3);
        }
    }
}

/* ------------------------------------------------------- the other screens --- */
static void draw_title(const char *t, const char *note)   /* a head with no pages (cf_views.c) */
{
    draw_head(t);
    draw_head_note(note);
}
/* the picture of a paged screen, sliding in after a turn (cv_slide), clipped under the head */
static void draw_view(void (*view)(int))
{
    fb_clip(0, VIEW_Y - 8, FB_W, Y_VIEW + H_VIEW);
    view(12 + cv_slide());
    fb_clip(0, Y_VIEW, FB_W, Y_VIEW + H_VIEW);
}

/* SEL: the chord button's chord on the pitch-class clock, the bass degree marked */
static void draw_sel(void)
{
    const cf_form_t *f = sel_form();
    char lab[8], b[24];
    flora_render(&flo, &fl_cv);
    draw_plant(Y_VIEW);
    fb_veil(0, Y_VIEW, FB_W, H_VIEW, FL_BG);   /* (the plant faint: the picture leads) */
    if (cf_is_off(&cs.chord, cu.sel_btn)) {
        draw_title("HOLD", "Holds notes after you let go");
    } else {                                     /* the button as its key shows it; its chord in words */
        cf_form_label(f, lab);
        str_cpy(b, cf_type_name(f->type), sizeof b);
        if (f->add) {
            str_cpy(b + str_len(b), " + ", 4);
            str_cpy(b + str_len(b), cf_adds_name(cf_adds_index(f->add)), 10);
        }
        draw_title(lab, b);
    }
    draw_view(view_sel);
    draw_sel_pointer();
    {
        static const char *const H[2] = {"SELECT", "button"};
        draw_hints(H, 1);
    }
}

/* ENV: the envelope's shape, drawn over the plant */
static void draw_env(void)
{
    static const char *const TGT[4] = {"Chord + bass", "Chord", "Bass", "Layer"};
    int a = cf_env_get(env_part(), 0), d = cf_env_get(env_part(), 1), sl = cf_env_get(env_part(), 2);
    int r = cf_env_get(env_part(), 3), x0 = 16, yb = Y_VIEW + H_VIEW - 14, yt = Y_VIEW + 50;
    int xa = x0 + 8 + a * 44 / 99, xd = xa + 8 + d * 44 / 99, xs = xd + 40, xr = xs + 8 + r * 50 / 99;
    int ys = yb - (yb - yt) * sl / 99;
    flora_render(&flo, &fl_cv);
    draw_plant(Y_VIEW);
    fb_veil(0, Y_VIEW, FB_W, H_VIEW, FL_BG);   /* (the plant faint: the picture leads) */
    draw_head(TGT[cu.env_tgt % 4u]);
    draw_pager((int)(cu.env_tgt % 4u), 4, 1, 0);
    fb_hline(x0, yb, 210, FL_BG3);
    fb_line(x0, yb, xa, yt, C_ACC(1));
    fb_line(xa, yt, xd, ys, C_ACC(1));
    fb_line(xd, ys, xs, ys, C_ACC(1));
    fb_line(xs, ys, xr, yb, C_ACC(1));
    fb_rect(xa - 1, yt - 1, 3, 3, C_ACC(0));
    fb_rect(xd - 1, ys - 1, 3, 3, C_ACC(0));
    fb_rect(xs - 1, ys - 1, 3, 3, C_ACC(0));
    {
        static const char *const H[2] = {"SELECT", "sound"};   /* (chord + bass, chord, bass, the LAYER) */
        draw_hints(H, 1);
    }
}

/* EDIT: the sound, the section and its page; FM: the six operators, the carriers below */
static void draw_edit(void)
{
    uint32_t part = cu.edit_part, np = cfe_npages(part, cu.edit_sec);
    char t[24];
    flora_render(&flo, &fl_cv);
    draw_plant(Y_VIEW);
    fb_veil(0, Y_VIEW, FB_W, H_VIEW, FL_BG);   /* (the plant faint: the picture leads) */
    /* the title: the section (FM: "OP 1", its page's name by the pager), or the VA's page ("OSC 1", its second page
     * the same name). The sound edited: in the top bar, after SOUND (cf_ui_draw) */
    if (cfe_fm6(part)) {
        const char *k = cfe_key_name(part, cu.edit_sec);
        static const char *const FMSEC[2] = {"Pitch", "Voice"};
        if (cu.edit_sec < 6u) {
            str_cpy(t, "OP ", sizeof t);
            t[3] = k[2];
            t[4] = 0;
        } else {
            str_cpy(t, FMSEC[(cu.edit_sec - 6u) & 1u], sizeof t);
        }
        draw_head(t);
        {                                        /* the page's name, without the title's word ("Pitch Rate": Rate) */
            const char *pn = cfe_page_title(part, cu.edit_sec, cu.page[SCR_EDIT]);
            uint32_t n = str_len(t), i;
            for (i = 0; i < n && pn[i] == t[i]; i++)
                ;
            if (i == n && (!pn[n] || pn[n] == ' '))
                pn += pn[n] ? n + 1u : n;
            draw_pager(cu.page[SCR_EDIT], (int)np, 0, pn);
        }
    } else {
        uint32_t n;
        str_cpy(t, cfe_page_title(part, cu.edit_sec, cu.page[SCR_EDIT]), sizeof t);
        n = str_len(t);
        if (n && t[n - 1] == '+')
            t[n - 1] = 0;
        draw_head(t);
        draw_pager(cu.page[SCR_EDIT], (int)np, 0, 0);
#if FELUCCA_VA
        if (cfe_va(part))
            draw_view(view_va);
#endif
#if FELUCCA_GRAIN
        if (cfe_grain(part))
            draw_view(view_grain_wave);
#endif
    }
    if (cfe_fm6(part))
        draw_view(view_fm);
    {
        static const char *const H[4] = {"SELECT", "page", "KEYS", "section"};
        draw_hints(H, 2);
    }
}

static void draw_generic(void)
{
    static const char *const PG_GLO[8] = {"Display", "MIDI", "Channels", "Play", "Reset", "Grain", "Pitch", "Hold"};
    flora_render(&flo, &fl_cv);
    draw_plant(Y_VIEW);
    fb_veil(0, Y_VIEW, FB_W, H_VIEW, FL_BG);   /* (the plant faint: the picture leads) */
    switch (cu.scr) {
    case SCR_GLO: {
        static const char *const H[2] = {"SELECT", "page"};
        static void (*const V[8])(int) = {view_display, view_midi, view_midi, view_play, view_reset, view_grain,
                                          view_grain, view_grain};   /* (MIDI and Channels: one picture; the three
                                                                      * GRAIN pages another) */
        uint32_t p = cu.page[SCR_GLO] % (5u + CG_NPAGE);
        draw_head(PG_GLO[p]);
        draw_pager((int)p, (int)(5u + CG_NPAGE), 1, 0);
        draw_view(V[p]);
        draw_hints(H, 1);
        break;
    }
    case SCR_BASS: {
        char b[16], a[4];
        int i;
        b[0] = 0;
        for (i = 0; i < cf_sn.nb && i < 2; i++) {   /* the bass's notes now, "C2  C3" */
            cf_pc_name(cf_sn.bass[i] % 12u, &cu_spell, a);
            str_cpy(b + str_len(b), a, 4);
            kint(b + str_len(b), cf_sn.bass[i] / 12 - 1, 0);
            if (i + 1 < cf_sn.nb)
                str_cpy(b + str_len(b), "  ", 3);
        }
        draw_title(cf_snd[CF_PART_BASS].name, b);
        draw_view(view_bass);
        break;
    }
    case SCR_SEQ: {
        char b[12], s2[8];
        kint(b, cs.bpm, " BPM");
        kint(s2, cs.beats, 0);
        str_cpy(s2 + str_len(s2), cs.beats == 6 ? "/8" : "/4", 4);
        draw_title(b, s2);
        /* (the owner's words, in the middle of the picture: on one line it ran edge to edge, at the foot it sat on
         * the keys; "по центру экрана", 2026-10-19) */
        draw_chip_text_c(Y_VIEW + 70, "Press [SEQ] again", FL_INK2);
        draw_chip_text_c(Y_VIEW + 84, "to open sequencer", FL_INK2);
        {
            static const char *const H[2] = {"PLAY", "start"};   /* (the click sounds only while the beat runs) */
            static const char *const H2[2] = {"PLAY", "stop"};
            draw_hints(cf_sn.running ? H2 : H, 1);
        }
        break;
    }
    case SCR_ARP: {                              /* the mode; Pattern and Feel; the pattern pictured */
        static const char *const PG[2] = {"Pattern", "Feel"};
        static const char *const H[2] = {"SELECT", "page"};
        draw_head(CF_ARP_NAME[cs.arp.mode % CF_ARP_NMODES]);
        draw_pager(cu.page[SCR_ARP] & 1, 2, 1, PG[cu.page[SCR_ARP] & 1]);
        fb_text_r(&CF_FONT_UI, 234, HEAD_LINE, cu.arp_on ? (cf_grid_shown() ? "on the beat" : "on") : "off",
                  cu.arp_on ? C_ACC(1) : FL_INK3, 1);
        draw_view(view_arp);
        draw_hints(H, 1);
        break;
    }
    case SCR_SAVE: {                             /* the name being given, its letter under edit; Chord / Bass / Layer */
        static const char *const PG[3] = {"Chord", "Bass", "Layer"};
        static const char *const H[4] = {"SELECT", "part", "SAVE", "hold"};
        uint32_t part = cf_save_part();
        char pre[13];
        draw_head(cu.save_name[0] ? cu.save_name : " ");
        str_cpy(pre, cu.save_name, (uint32_t)cu.save_cur + 1u);
        fb_rect(6 + fb_text_w(&CF_FONT_UI, pre, 2), HEAD_Y + 25, 13, 2, C_ACC(0));   /* (under the letter K3 sets) */
        draw_pager((int)part, 3, 1, PG[part]);
        if (cu.edited[part])
            fb_text_r(&CF_FONT_UI, 234, HEAD_LINE, "edited", C_ACC(1), 1);
        draw_view(view_save);
        draw_hints(H, 2);
        break;
    }
    default: break;
    }
}

/* ---------------------------------------------------------------- popups --- */
static void draw_pop_sound(uint32_t part)
{
    int x = 16, y = Y_VIEW + 26, w = 208, h = 94, i;
    uint32_t n = cf_snd_count(part), pos = cf_snd_pos(part);
    char a[4], b[4], t[12];
    draw_panel(x, y, w, h);
    fb_text(&CF_FONT_UI, x + 8, y + 5, part == 2u ? "LAYER SOUND" : part ? "BASS SOUND" : "CHORD SOUND", C_ACC(0), 1);
    snd_num(pos, a);
    snd_num(n ? n - 1u : 0u, b);
    str_cpy(t, a, sizeof t);
    str_cpy(t + 3, " / ", 4);
    str_cpy(t + 6, b, 4);
    fb_text_r(&CF_FONT_UI, x + w - 8, y + 5, t, FL_INK2, 1);
    for (i = -2; i <= 2; i++) {
        uint32_t idx = n ? (pos + n * 2u + (uint32_t)(i + 2) - 2u) % n : 0u, e, pp;
        char nm[13], num[4];
        int yy = y + 22 + (i + 2) * 14;
        cf_snd_at(part, idx, &e, &pp);
        pool_name(e, pp, nm);
        snd_num(idx, num);
        if (!i)
            fb_rect(x + 4, yy - 1, w - 8, 14, FL_BG2);
        if (i && idx == 0u)                      /* the row starts again here */
            fb_hline(x + 8, yy - 2, w - 16, C_ACC(3));
        fb_text(&CF_FONT_UI, x + 8, yy, num, i ? FL_INK3 : FL_INK2, 1);
        fb_text(&CF_FONT_UI, x + 38, yy, cf_engine_name(e), i ? FL_INK3 : C_SEC(0), 1);
        fb_text(&CF_FONT_UI, x + 64, yy, nm, i ? FL_INK2 : FL_INK, 1);
    }
}

static void draw_pop_algo(void)
{
    int x = 14, y = Y_VIEW + 36, w = 212, h = 80, k;
    draw_panel(x, y, w, h);
    fb_text(&CF_FONT_UI, x + 8, y + 5, "CHORD ALGORITHM", C_ACC(0), 1);
    fb_text_r(&CF_FONT_UI, x + w - 8, y + 5, ALG_NAME[cs.chord.algo], FL_INK, 1);
    /* the keyboard in small: chord buttons in the accent, roots in grey */
    for (k = 0; k < CF_NKEYS; k++) {
        int kx, ky, kw, kh, role = cf_key_role(cs.chord.algo, k, 0);
        key_box(k, &kx, &ky, &kw, &kh);
        kx = x + 15 + kx * 3 / 4;
        ky = y + 30 + (ky - Y_KEYS) * 3 / 4;
        fb_rbox(kx, ky, kw * 3 / 4, kh * 3 / 4, 1, role == CF_ROLE_BUTTON ? C_ACC(1) : FL_BG3);
    }
    fb_text(&CF_FONT_UI, x + 8, y + h - 18, cs.chord.algo & 1 ? "buttons latch" : "hold the buttons", FL_INK2, 1);
}

static void draw_popup(void)
{
    switch (cu.pop) {
    case POP_SOUND_CHORD: draw_pop_sound(CF_PART_CHORD); break;
    case POP_SOUND_BASS: draw_pop_sound(CF_PART_BASS); break;
    case POP_SOUND_LAYER: draw_pop_sound(CF_PART_LAYER); break;
    case POP_ALGO: draw_pop_algo(); break;
    case POP_OCTAVE: {
        char b[12];
        draw_panel(60, Y_VIEW + 52, 120, 44);
        str_cpy(b, "OCT ", 12);
        b[4] = cs.chord.octave > 0 ? '+' : cs.chord.octave < 0 ? '-' : ' ';
        b[5] = (char)('0' + (cs.chord.octave < 0 ? -cs.chord.octave : cs.chord.octave));
        b[6] = 0;
        fb_text_c(&CF_FONT_UI, 120, Y_VIEW + 61, b, FL_INK, 2);
        break;
    }
    case POP_MSG: {                              /* one line, or two (a '\n'); a button's name as a chip */
        char a[64];
        int i;
        str_cpy(a, cu.msg, sizeof a);
        for (i = 0; a[i] && a[i] != '\n'; i++)
            ;
        if (a[i]) {
            a[i] = 0;
            draw_panel(14, Y_VIEW + 50, 212, 48);
            draw_chip_text_c(Y_VIEW + 59, a, FL_INK);
            draw_chip_text_c(Y_VIEW + 75, a + i + 1, FL_INK);
        } else {
            draw_panel(20, Y_VIEW + 56, 200, 36);
            draw_chip_text_c(Y_VIEW + 67, a, FL_INK);
        }
        break;
    }
    default: break;
    }
}

/* -------------------------------------------------------------- the frame --- */
static int cf_intro_draw(uint32_t dt);
static void cf_ui_draw(void)
{
    char top[24];
    fb_unclip();
    if (cf_intro_draw(cu.frame_dt))
        return;
    if (cu.scr == SCR_HOME) {
        /* the layout's name, and a padlock for the Lock ones */
        const char *an = cs.chord.algo >= CF_ALG_SIDE ? "Side" : "Top";
        int lk = cs.chord.algo == CF_ALG_TOP_LOCK || cs.chord.algo == CF_ALG_SIDE_LOCK, xr = lk ? 195 : 206;
        draw_topbar("", 0);
        fb_text_r(&CF_FONT_UI, xr, Y_TOP + 1, an, FL_INK3, 1);
        if (lk)
            draw_lock(xr + 3, Y_TOP + 3, FL_INK3);
        {
            int x = xr - fb_text_w(&CF_FONT_UI, an, 1) - 6;
            if (cu.arp_on) {                     /* the arpeggiator plays: a chip before the algorithm */
                int w = fb_text_w(&CF_FONT_UI, "ARP", 1) + 6;
                x -= w;
                fb_rbox(x, Y_TOP + 2, w, 11, 2, C_ACC(1));
                fb_text(&CF_FONT_UI, x + 3, Y_TOP + 1, "ARP", FL_BG, 1);
                x -= 4;
            }
            if (cf_sn.sq_on && !sqk.on) {        /* the sequencer plays: its pattern (the song's) and bar */
                char b[12];
                int w;
                str_cpy(b, cf_sn.sq_song ? "SONG " : "SEQ ", sizeof b);
                b[str_len(b) + 1] = 0;
                b[str_len(b)] = (char)('A' + cf_sn.sq_pat % SQ_NPAT);   /* (the letter alone: "A3" read as a note) */
                w = fb_text_w(&CF_FONT_UI, b, 1) + 12;
                x -= w;
                fb_rbox(x, Y_TOP + 2, w, 11, 2, cf_panel(2));
                fb_rect(x + 3, Y_TOP + 5, 4, 4, C_ACC(0));
                fb_text(&CF_FONT_UI, x + 9, Y_TOP + 1, b, FL_INK, 1);
                x -= 4;
            }
            if (sqk.on) {                        /* a take from HOME: its count-in, then its bar of 8 */
                char b[12];
                int w, rec = sqk.rolled;
                if (rec) {
                    str_cpy(b, "REC ", sizeof b);
                    kint(b + 4, (int)(sqk.nbar % SQ_BARS) + 1, "/8");
                } else {
                    str_cpy(b, "REC in ", sizeof b);
                    b[7] = (char)('0' + (cf_sn.sq_cin + SQ_PPQN - 1u) / SQ_PPQN % 10u);
                    b[8] = 0;
                }
                w = fb_text_w(&CF_FONT_UI, b, 1) + 12;
                x -= w;
                fb_rbox(x, Y_TOP + 2, w, 11, 2, rec ? FL_C(FL_R_FIRE, 2) : FL_BG2);
                fb_rect(x + 3, Y_TOP + 5, 4, 4, rec ? FL_INK : FL_C(FL_R_FIRE, 1));
                fb_text(&CF_FONT_UI, x + 9, Y_TOP + 1, b, FL_INK, 1);
                x -= 4;
            }
            draw_top_sound(CF_PART_CHORD, x);    /* (after the chips: the name fits before them) */
        }
        (void)top;
    } else {
        draw_topbar(cu.scr == SCR_SQED ? (squ.trk == SQ_DRUMS ? "DRUMS" : squ.trk == SQ_BASS ? "BASS" :
                                          squ.trk == SQ_LAYER ? "LAYER" : "CHORDS") :
                    SCR_NAME[cu.scr], 0);
        if (cu.scr == SCR_EDIT)                  /* the sound edited, after SOUND */
            fb_text(&CF_FONT_UI, 4 + fb_text_w(&CF_FONT_UI, SCR_NAME[cu.scr], 1) + 7, Y_TOP + 1,
                    cf_snd[cu.edit_part].name, FL_INK3, 1);
    }
    cu_hints = 0;                                /* (the screen names its hints while it draws: draw_hints) */
    fb_clip(0, Y_VIEW, FB_W, cu.scr == SCR_FX || (CF_SKYHOME && cu.scr == SCR_HOME) ? Y_KNOBS : Y_VIEW + H_VIEW);   /* (FX has the key strip's space too) */
    if (cu.scr == SCR_HOME)
        draw_home();
    else if (cu.scr == SCR_SEL)
        draw_sel();
    else if (cu.scr == SCR_ENV)
        draw_env();
    else if (cu.scr == SCR_EDIT)
        draw_edit();
    else if (cu.scr == SCR_FX)
        draw_fx();
    else if (sq_screen(cu.scr))
        draw_seq();
    else
        draw_generic();
    draw_popup();
    fb_unclip();
    if (cu.scr == SCR_SQED) {                    /* the open track and its sound (PRESETS turns it) */
        const char *tn = squ.trk == SQ_DRUMS ? "DRUMS" : squ.trk == SQ_BASS ? "BASS" : squ.trk == SQ_LAYER ? "LAYER" :
                         "CHORDS";
        const char *sn = squ.trk == SQ_DRUMS ? sq_kit_name(sq_cur()) : cf_snd[squ.trk == SQ_LAYER ? CF_PART_LAYER :
                         squ.trk == SQ_BASS ? CF_PART_BASS : CF_PART_CHORD].name;
        int x = 4 + fb_text_w(&CF_FONT_UI, tn, 1) + 7;
        fb_text(&CF_FONT_UI, x, Y_TOP + 1, sn, FL_INK3, 1);
        draw_top_hints(x + fb_text_w(&CF_FONT_UI, sn, 1));
    } else if (cu.scr != SCR_HOME)
        draw_top_hints(4 + fb_text_w(&CF_FONT_UI, SCR_NAME[cu.scr], 1) +
                       (cu.scr == SCR_EDIT ? 7 + fb_text_w(&CF_FONT_UI, cf_snd[cu.edit_part].name, 1) : 0));
    if (cu.upd_left) {                           /* the update gesture: a countdown over everything */
        char b[2];
        b[0] = (char)('0' + cu.upd_left % 10u);
        b[1] = 0;
        fb_fill(FL_BG);
        fb_text_c(&CF_FONT_UI, 120, 70, b, C_ACC(0), 4);
        fb_text_c(&CF_FONT_UI, 120, 140, cu.upd_stub ? "update mode" : "USB boot", FL_INK, 1);
        fb_text_c(&CF_FONT_UI, 120, 156, "let go to cancel", FL_INK2, 1);
        cf_present();
        return;
    }
    if (cu.scr != SCR_HOME && cu.scr != SCR_FX)
    fb_rect(0, Y_VIEW + H_VIEW, FB_W, Y_KEYS - Y_VIEW - H_VIEW, FL_BG);   /* the gap over the keys: the intro's plant
                                                  * (cf_splash.c, 4 px lower) left its stem's foot there, and no
                                                  * screen draws it (the owner saw it over the m6 key, 2026-10-19) */
    if (cu.scr == SCR_HOME && CF_SKYHOME) {
        /* (the skyline has the whole space down to the knobs) */
    } else if (cu.scr == SCR_HOME) {                    /* no keys drawn: the snow runs on under the wizard */
        int y0 = Y_VIEW + H_VIEW;
        fb_rect(0, y0, FB_W, Y_KNOBS - y0, fl_px[(FLH - 1) * FLW + 10]);
        fb_rect(0, y0, FB_W, 1, 55);
        cf_snow(0, y0, FB_W, Y_KNOBS - y0, flo.t, 10, 0x77u);
    } else if (cu.scr != SCR_FX) {               /* (FX's pictures use the whole space down to the knobs) */
        int y0 = Y_VIEW + H_VIEW;
        fb_rect(0, y0, FB_W, Y_KNOBS - y0, FL_BG);
        fb_rect(0, y0, FB_W, 1, FL_BG3);
        cf_snow(0, y0, FB_W, Y_KNOBS - y0, flo.t, 10, 0x77u);
    }
    if (CF_KEYSTRIP)                             /* (3IS: the keyboard picture is off everywhere, the owner's word 2026-10-24:
                                                  * the wizard says what is played) */
        draw_keys();
    draw_knobs();
    cf_present();
}
