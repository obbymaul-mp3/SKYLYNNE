/* SKYLYNNE host harness: the whole instrument on a PC, driven by a script, writing screenshots (PPM) and audio (WAV).
 *
 *   sk_host [actions...]       actions:  wait:MS  btn:NAME  hold:NAME  enc:ROLE:STEPS  key:N  shot:FILE.ppm
 *                                        wav:FILE.wav (writes everything rendered so far)  biome:N
 *   names: FX SCL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+
 *   roles: SELECT ALGO PRESET K1 K2 K3 K4
 * The audio runs in step with the clock: 44100 samples a second, the screen redrawn every 33 ms. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define SK_POOL
#include "sk_dsp.c"
#include "sk_fx.c"
#include "sk_biomes.c"
static uint8_t shot[240 * 240 * 3];
static void sk_lcd(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            uint16_t s = p[j * w + i], v = (uint16_t)((s >> 8) | (s << 8));
            uint8_t *o = shot + ((y + j) * 240 + x + i) * 3;
            o[0] = (uint8_t)(((v >> 11) & 31) * 255 / 31);
            o[1] = (uint8_t)(((v >> 5) & 63) * 255 / 63);
            o[2] = (uint8_t)((v & 31) * 255 / 31);
        }
}
static void sk_pump(void) {}
/* the saved slots: a file, so that a second run can load what the first saved (SK_STORE=path) */
static int sk_store_read(void *dst, uint32_t n)
{
    const char *p = getenv("SK_STORE");
    FILE *f = p ? fopen(p, "rb") : NULL;
    size_t got;
    if (!f)
        return -1;
    got = fread(dst, 1, n, f);
    fclose(f);
    return got == n ? 0 : -1;
}
static int sk_store_write(const void *src, uint32_t n)
{
    const char *p = getenv("SK_STORE");
    FILE *f = p ? fopen(p, "wb") : NULL;
    if (!f)
        return -1;
    fwrite(src, 1, n, f);
    fclose(f);
    return 0;
}
#include "sk_fb.c"
#include "sk_scene.c"
#include "sk_phys.c"

#include "sk_ui.c"

static int32_t hs_enc[SE_N];
static uint32_t hs_held, hs_pr, hs_keys, hs_kp;
static uint8_t hs_ledb[SB_N], hs_ledk[27];
static int32_t sk_in_enc(uint32_t r) { int32_t d = hs_enc[r]; hs_enc[r] = 0; return d; }
static void sk_in_buttons(uint32_t *held, uint32_t *pressed) { *held = hs_held; *pressed = hs_pr; hs_pr = 0; }
static uint32_t sk_in_keys(uint32_t *held) { uint32_t p = hs_kp; *held = hs_keys; hs_kp = 0; return p; }
static void sk_led_btn(uint32_t l, uint32_t v) { hs_ledb[l] = (uint8_t)v; }
static void sk_led_key(uint32_t n, uint32_t v) { hs_ledk[n] = (uint8_t)v; }

static const char *const BN[SB_N] = {"FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"};
static const char *const EN[SE_N] = {"SELECT", "ALGO", "PRESET", "K1", "K2", "K3", "K4"};
static int16_t *wav;
static uint32_t wav_n, wav_cap;
static uint32_t now_ms, aud_acc;

static void write_ppm(const char *f)
{
    FILE *o = fopen(f, "wb");
    fprintf(o, "P6\n240 240\n255\n");
    fwrite(shot, 1, sizeof shot, o);
    fclose(o);
}
static void write_wav(const char *f)
{
    FILE *o = fopen(f, "wb");
    uint32_t bytes = wav_n * 4u, v;
    fwrite("RIFF", 1, 4, o); v = 36 + bytes; fwrite(&v, 4, 1, o);
    fwrite("WAVEfmt ", 1, 8, o); v = 16; fwrite(&v, 4, 1, o);
    { uint16_t t[2] = {1, 2}; fwrite(t, 2, 2, o); }
    v = 44100; fwrite(&v, 4, 1, o); v = 44100 * 4; fwrite(&v, 4, 1, o);
    { uint16_t t[2] = {4, 16}; fwrite(t, 2, 2, o); }
    fwrite("data", 1, 4, o); fwrite(&bytes, 4, 1, o);
    fwrite(wav, 4, wav_n, o);
    fclose(o);
}
static void run(uint32_t ms)
{
    uint32_t end = now_ms + ms;
    while (now_ms < end) {
        int32_t buf[64];
        uint32_t want;
        now_ms += 33;
        aud_acc += 44100u * 33u;
        want = aud_acc / 1000u / 32u;
        aud_acc -= want * 32u * 1000u;
        while (want--) {
            uint32_t i;
            sk_render(buf, 32);
            if (wav_n + 32 > wav_cap) {
                wav_cap = wav_cap ? wav_cap * 2 : 1u << 20;
                wav = realloc(wav, wav_cap * 4);
            }
            for (i = 0; i < 32; i++) {
                int32_t l = buf[2 * i], r = buf[2 * i + 1];
                l = l > 32767 ? 32767 : l < -32768 ? -32768 : l;
                r = r > 32767 ? 32767 : r < -32768 ? -32768 : r;
                wav[(wav_n + i) * 2] = (int16_t)l;
                wav[(wav_n + i) * 2 + 1] = (int16_t)r;
            }
            wav_n += 32;
        }
        ui_input(now_ms);
        ui_draw(now_ms);
    }
}

int main(int argc, char **argv)
{
    int a;
    ui_init(0);
    ui_store_load();
    for (a = 1; a < argc; a++) {
        char *s = argv[a];
        if (!strncmp(s, "wait:", 5))
            run((uint32_t)atoi(s + 5));
        else if (!strncmp(s, "btn:", 4)) {
            int k;
            for (k = 0; k < SB_N; k++)
                if (!strcmp(s + 4, BN[k]))
                    hs_pr |= 1u << k;
            run(66);
        } else if (!strncmp(s, "hold:", 5)) {
            int k;
            for (k = 0; k < SB_N; k++)
                if (!strcmp(s + 5, BN[k]))
                    hs_held ^= 1u << k;
        } else if (!strncmp(s, "enc:", 4)) {
            char *c = strchr(s + 4, ':');
            int k;
            *c = 0;
            for (k = 0; k < SE_N; k++)
                if (!strcmp(s + 4, EN[k]))
                    hs_enc[k] += atoi(c + 1);
            run(66);
        } else if (!strncmp(s, "key:", 4)) {
            hs_kp |= 1u << atoi(s + 4);
            run(66);
        } else if (!strncmp(s, "biome:", 6)) {
            ui_biome((uint32_t)atoi(s + 6), now_ms);
            run(33);
        } else if (!strncmp(s, "shot:", 5)) {
            run(33);
            write_ppm(s + 5);
        } else if (!strncmp(s, "wav:", 4))
            write_wav(s + 4);
    }
    return 0;
}
