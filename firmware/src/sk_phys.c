/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* SKYLYNNE's physics sequencer: a polygon turns, balls fall inside it, and a ball that touches a side plays a note.
 *
 * The side it touches is the scale degree (so a hexagon is a six-note phrase), the ball is a voice (ball 1 plays voice 1
 * in its register and with its sound), the speed of the hit is the velocity. The shape's SPIN carries the balls round
 * and throws them; GRAVITY pulls them down; BOUNCE is how much of a hit is kept (above 100 % the walls add energy, so
 * the balls never settle). All integer, a fixed step of 1/60 s whatever the frame rate; the walls are the exact
 * sides of the polygon, moving with it. Included by sk_ui.c's unit; drawn on the scene's area. */

#define PH_MAXB 6
#define PH_CX 120
#define PH_CY 94
#define PH_R 82                          /* the polygon's circumradius, pixels */
#define PH_BR 3                          /* a ball's radius */

typedef struct {
    int32_t x, y, vx, vy;                /* Q8 pixels from the centre; Q8 pixels a step */
    int16_t tx[6], ty[6];                /* the trail, pixels */
    uint8_t flash;                       /* 255 at a hit, falling */
    int8_t side;                         /* the side last touched */
    uint16_t cool;                       /* steps before it may sound again */
} ph_ball_t;

static struct {
    uint8_t on;                          /* the balls strike their voices */
    uint8_t sides;                       /* 3 .. 8 */
    uint8_t nb;                          /* balls, 0 .. PH_MAXB */
    uint8_t grav, bounce;                /* 0..127 */
    int8_t spin;                         /* -64 .. 63 */
    uint16_t ang;                        /* Q16 turns */
    uint32_t last;                       /* ms of the last step */
    uint8_t edge[8];                     /* a side's glow, 255 at a hit */
    uint8_t edge_v[8];                   /* .. in whose colour */
    ph_ball_t b[PH_MAXB];
    uint32_t hits;                       /* all hits (the screen's counter) */
} ph = {1, 6, 3, 40, 100, 6};

static int ph_cos(uint32_t a16) { return SK_SIN[((a16 >> 6) + 256u) & 1023u]; }
static int ph_sin(uint32_t a16) { return SK_SIN[(a16 >> 6) & 1023u]; }

static void ph_reset_ball(uint32_t k, uint32_t seed)
{
    ph_ball_t *b = &ph.b[k];
    uint32_t h = sc_hash(seed + k * 977u);
    int ix = (int)(h % 41u) - 20;
    b->x = (ix * 2 + ((int)k - 2) * 7) << 8;
    b->y = (-20 - (int)((h >> 8) % 30u)) << 8;
    b->vx = (int)((h >> 16) % 129u) - 64;
    b->vy = 0;
    b->flash = 0;
    b->side = -1;
    b->cool = 0;
    {
        int i;
        for (i = 0; i < 6; i++) {
            b->tx[i] = (int16_t)(PH_CX + (b->x >> 8));
            b->ty[i] = (int16_t)(PH_CY + (b->y >> 8));
        }
    }
}

static void ph_init(void)
{
    uint32_t k;
    for (k = 0; k < PH_MAXB; k++)
        ph_reset_ball(k, 0x5A17u);
}

/* one physics step */
static void ph_step(void)
{
    static uint8_t tr;
    int g = 14 + ph.grav * 62 / 127;                       /* Q8 pixels per step^2 (0.05 .. 0.3) */
    int apo = ((PH_R << 8) >> 2) * SK_SIN[256 - 512 / ph.sides] >> 13;
    int w = ph.spin * 75 / 256;                             /* radians a step, Q8 (the angle moves spin * 12 / 65536 turns) */
    int e = 64 + ph.bounce * 72 / 127;                      /* Q7 .. 0.5 .. 1.1 */
    uint32_t k, s;
    int i;
    ph.ang = (uint16_t)(ph.ang + (int)ph.spin * 12);
    for (i = 0; i < 8; i++)
        if (ph.edge[i])
            ph.edge[i] = (uint8_t)(ph.edge[i] > 6u ? ph.edge[i] - 6u : 0u);
    tr++;
    for (k = 0; k < ph.nb; k++) {
        ph_ball_t *b = &ph.b[k];
        b->vy += g;
        b->x += b->vx;
        b->y += b->vy;
        if (b->cool)
            b->cool--;
        if (b->flash)
            b->flash = (uint8_t)(b->flash > 5u ? b->flash - 5u : 0u);
        for (s = 0; s < ph.sides; s++) {
            uint32_t phi = ph.ang + (2u * s + 1u) * 32768u / ph.sides;
            int c = ph_cos(phi), sn = ph_sin(phi);
            int proj = ((b->x >> 2) * c + (b->y >> 2) * sn) >> 13;
            int over = proj + (PH_BR << 8) - apo;
            if (over > 0) {
                int wx = -((w * b->y) >> 8), wy = (w * b->x) >> 8;   /* the wall's velocity here */
                int vn = (((b->vx - wx) >> 1) * c + ((b->vy - wy) >> 1) * sn) >> 14;
                b->x -= ((over * c) >> 15) + 1;
                b->y -= ((over * sn) >> 15) + 1;
                if (vn > 0) {
                    int imp = vn * (128 + e) >> 7;
                    b->vx -= (imp * c) >> 15;
                    b->vy -= (imp * sn) >> 15;
                    b->vx -= b->vx >> 6;                    /* a little grip */
                    if (vn > 160 && !b->cool && ph.on) {
                        uint32_t vel = 30u + (uint32_t)(vn > 1400 ? 1400 : vn) * 90u / 1400u;
                        int deg = (int)s + (int)k * 2 - ph.sides / 2;
                        sk_play(k, sk_note_of(k, deg), vel);
                        b->cool = 8;
                    }
                    if (vn > 160) {
                        b->flash = 255;
                        b->side = (int8_t)s;
                        ph.edge[s] = 255;
                        ph.edge_v[s] = (uint8_t)k;
                        ph.hits++;
                    }
                }
            }
        }
        if (b->x > (200 << 8) || b->x < -(200 << 8) || b->y > (200 << 8) || b->y < -(200 << 8))
            ph_reset_ball(k, ph.hits + k);
        if ((tr & 3u) == 0u) {
            int t;
            for (t = 5; t > 0; t--) {
                b->tx[t] = b->tx[t - 1];
                b->ty[t] = b->ty[t - 1];
            }
            b->tx[0] = (int16_t)(PH_CX + (b->x >> 8));
            b->ty[0] = (int16_t)(PH_CY + (b->y >> 8));
        }
    }
}

/* run the steps owed by the clock (60 a second, at most 4 at once) */
static void ph_update(uint32_t now)
{
    uint32_t n = 0;
    if (now - ph.last > 250u)
        ph.last = now;
    while (now - ph.last >= 17u && n < 4u) {
        ph.last += 17u;
        ph_step();
        n++;
    }
}

static void ph_draw(uint32_t now)
{
    uint32_t s, k;
    int vx[8], vy[8];
    (void)now;
    /* the backdrop: the scene's sky, dimmed, with the same stars */
    fb_rect(0, 0, FB_W, SC_H, C_BG);
    sc_stars(now);
    for (s = 0; s < ph.sides; s++) {
        uint32_t a = ph.ang + s * 65536u / ph.sides;
        vx[s] = PH_CX + (PH_R * ph_cos(a) >> 15);
        vy[s] = PH_CY + (PH_R * ph_sin(a) >> 15);
    }
    for (s = 0; s < ph.sides; s++) {
        uint32_t n = (s + 1u) % ph.sides;
        /* side s lies between vertex s and s+1; its glow is edge[s] */
        int g = ph.edge[s];
        uint8_t c = g > 160 ? SC_COL[ph.edge_v[s] % SK_NV] : (g > 40 ? C_ACC : C_INK2);
        fb_line(vx[s], vy[s], vx[n], vy[n], c);
        if (g > 40) {
            fb_line(vx[s] + 1, vy[s], vx[n] + 1, vy[n], c);
            fb_line(vx[s], vy[s] + 1, vx[n], vy[n] + 1, c);
        }
    }
    for (s = 0; s < ph.sides; s++)
        fb_rect(vx[s] - 1, vy[s] - 1, 3, 3, C_INK2);
    for (k = 0; k < ph.nb; k++) {
        const ph_ball_t *b = &ph.b[k];
        uint8_t c = SC_COL[k % SK_NV];
        int t, x = PH_CX + (b->x >> 8), y = PH_CY + (b->y >> 8);
        for (t = 5; t > 0; t--)
            fb_pset(b->tx[t], b->ty[t], t > 2 ? C_PANEL2 : C_ACC);
        if (b->flash > 100)
            fb_disc(x, y, PH_BR + 2 + b->flash / 90, C_PANEL2);
        fb_disc(x, y, PH_BR, c);
        fb_pset(x - 1, y - 1, C_INK);
    }
}
