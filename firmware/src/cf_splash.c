/* SPDX-License-Identifier: GPL-3.0-only
 * CoralFlower: the power-on splash. A seed sprouts and opens into a flower (a coral with the Side algorithms), the
 * name grows in under it. Played by main.c while it boots (IRQs still off: a busy-wait clock, the panel by polled DMA). SAFE MODE
 * shows its notice instead. Included after cf_screens.c. */

#define CF_SPLASH_MS 1400u

/* 3IS shows its own version here; FELUCCA_VERSION stays "CoralFlower ..": the update tool reads it */
#define CF_SHOWN_VERSION "an Obby Mall synth"

static const uint8_t SKYC[6] = {60, 61, 62, 63, 53, 54};
static void cf_splash_frame(uint32_t t)
{
    static const uint8_t SKY[4] = {FL_L0, FL_L1, FL_L2, FL_L3};
    int r;
    /* the night sky, 2 px rows, the bands dithered into each other */
    for (r = 0; r < 120; r++) {
        int tt = r * 768 / 120, i = tt >> 8;
        fb_rect(0, 2 * r, 240, 2, (tt & 255) > 128 && !(r & 1) ? SKY[i + 1] : SKY[i]);
    }
    cf_snow(0, 0, 240, 240, t * 3u, 60, 0x55u);
    /* the name lights up letter by letter between 300 and 1100 ms, over a skyline asleep */
    {
        static const char NM[] = "SKYLYNNE";
        char b[2];
        int i, nshow = t < 300u ? 0 : t >= 1100u ? 8 : (int)((t - 300u) * 8u / 800u) + 1, x = 120 - 8 * 14 / 2 * 2 / 2;
        int cw = fb_text_w(&CF_FONT_UI, "S", 3) + 3, tw = cw * 8 - 3;
        x = 120 - tw / 2;
        for (i = 0; i < 8; i++) {                                    /* the towers' lights below it */
            int bx = 14 + i * 28, bh = 14 + ((i * 37) % 23), on = i < nshow;
            fb_rect(bx, 176 - bh, 22, bh + 12, 52);
            fb_rect(bx + 3, 180 - bh, 3, 4, on ? SKYC[i % 6] : 55);
            fb_rect(bx + 10, 188 - bh, 3, 4, on ? SKYC[(i + 2) % 6] : 55);
            fb_rect(bx + 3, 198 - bh, 3, 4, on ? SKYC[(i + 4) % 6] : 55);
        }
        for (i = 0; i < 8; i++) {
            b[0] = NM[i];
            b[1] = 0;
            if (i < nshow)
                fb_text(&CF_FONT_UI, x + i * cw, 92, b, i == nshow - 1 && t < 1100u ? FL_INK : SKYC[i % 6], 3);
        }
    }
    if (t > 1100u)
        fb_text_c(&CF_FONT_UI, 120, 128, CF_SHOWN_VERSION, 54, 1);
}

/* main.c's splash (IRQs off): the first picture only, the scene before the seed sprouts; the UI's first frames grow it
 * (cf_intro_draw) while the instrument already plays */
static void cf_splash(void)
{
    cs_defaults(&cs);
    cf_settings_load();                          /* (what was set before the power-off, unless SAFE MODE) */
    cf_palette_build();
    fb_full = 1;
    if (cr_safe) {
        fb_fill(FL_BG);
        fb_text_c(&CF_FONT_UI, 120, 90, "SAFE MODE", C_ACC(0), 2);
        fb_text_c(&CF_FONT_UI, 120, 130, "settings and sounds", FL_INK2, 1);
        fb_text_c(&CF_FONT_UI, 120, 144, "are not loaded", FL_INK2, 1);
        cf_present();
        return;
    }
    flora_init(&flo, 0x5EED1u, FLW / 2, FLH + 1);
    flo.bloom = FL_R_ROSE + 1u;                  /* the intro's flower is pink, whatever the key */
    flora_set_key(&flo, cs.chord.tonic);
    flo.backdrop = cs.backdrop;
    flora_set_species(&flo, cs.chord.algo >= CF_ALG_SIDE ? FL_SP_CORAL : FL_SP_PLANT);
    cf_splash_frame(0);                          /* (nothing has grown yet: the scene alone) */
    cf_present();
}

/* the intro: the UI's first CF_SPLASH_MS, the seed grows into a flower and the name comes in. Any key, button or
 * knob ends it at once (the plant stays: it is HOME's) */
static uint8_t cf_intro_on = 1;
static uint32_t cf_intro_t;
static int cf_intro_draw(uint32_t dt)
{
    static const fl_chord_t SEED = {4, {60, 64, 67, 74}, 0, FL_Q_MAJOR, 48};
    if (!cf_intro_on)
        return 0;
    if (!cf_intro_t) {
        flora_set_chord(&flo, &SEED);
        if (cs.boot_sound && !cr_safe)
            cf_boot_go = 1;                      /* (the bells strum a wide chord as the flower opens; cf_fx.c) */
    }
    /* (a step at most 40 ms: the first frame comes long after the boot began, and a jump there showed the name's
     * first letter at once, alone over the bare seed) */
    cf_intro_t += dt ? (dt > 40u ? 40u : dt) : 1u;
    if (cf_intro_t >= CF_SPLASH_MS || cu.idle_ms < cf_intro_t / 2u) {
        cf_intro_on = 0;
        flora_event(&flo, FL_EV_SPIN);
        flo.bloom = 0;                           /* from the first chord on, the colours follow the root */
        fb_full = 1;
        return 0;
    }
    flora_set_level(&flo, (uint16_t)(cf_intro_t < 700u ? cf_intro_t * 40u : 28000u));
    cf_splash_frame(cf_intro_t);
    cf_present();
    return 1;
}
