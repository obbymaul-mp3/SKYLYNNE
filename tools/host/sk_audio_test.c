/* SPDX-License-Identifier: GPL-3.0-only */
/* SKYLYNNE's sound on the host: render a biome to a WAV and report its level and pitches.
 *   cc -O2 -Ifirmware/src tools/host/sk_audio_test.c -o /tmp/sk_audio_test && /tmp/sk_audio_test BIOME SECONDS OUT.wav */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#define SK_POOL
#include "../../firmware/src/sk_dsp.c"
#include "../../firmware/src/sk_fx.c"
#include "../../firmware/src/sk_biomes.c"

/* tone TYPE NOTE: one voice struck once, dry (the room off), to WAV: for checking pitch and level */
static int tone(int type, int note, const char *path)
{
    uint32_t total = 44100 * 12 / SK_CTL * SK_CTL, i, k;
    int16_t *pcm = malloc(total * 4);
    int32_t buf[2 * SK_CTL], l[SK_CTL], r[SK_CTL], rv[SK_CTL], gb[SK_CTL];
    sk_load(&SK_BIOMES[0]);
    sk_s.type[0] = (uint8_t)type;
    sk_s.mute = 0x3E;
    sk_s.hold = 1;
    sk_pan(&sk_v[0], 0);
    sk_start(&sk_v[0], (uint32_t)type, (uint32_t)note, 24, 100, 70, 7000);
    for (i = 0; i < total; i += SK_CTL) {
        for (k = 0; k < SK_CTL; k++)
            l[k] = r[k] = rv[k] = gb[k] = 0;
        sk_voices(l, r, rv, gb, SK_CTL);
        for (k = 0; k < SK_CTL; k++) {
            pcm[2 * (i + k)] = (int16_t)sk_clampi(l[k], -32767, 32767);
            pcm[2 * (i + k) + 1] = (int16_t)sk_clampi(r[k], -32767, 32767);
        }
    }
    (void)buf;
    {
        FILE *f = fopen(path, "wb");
        uint32_t n = total * 4, v;
        fwrite("RIFF", 1, 4, f); v = 36 + n; fwrite(&v, 4, 1, f);
        fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
        { uint16_t a = 1, ch = 2; uint32_t rr = 44100, br = 44100 * 4; uint16_t ba = 4, bits = 16;
          fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f); }
        fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f); fwrite(pcm, 1, n, f); fclose(f);
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 4 && !strcmp(argv[1], "tone"))
        return tone(atoi(argv[2]), atoi(argv[3]), argv[4]);
    int bi = argc > 1 ? atoi(argv[1]) : 0;
    double secs = argc > 2 ? atof(argv[2]) : 20;
    const char *path = argc > 3 ? argv[3] : "/tmp/sk.wav";
    uint32_t total = (uint32_t)(secs * 44100.0) / SK_CTL * SK_CTL, i, k;
    int16_t *pcm = malloc(total * 4);
    int32_t buf[2 * SK_CTL];
    int32_t peak = 0, maxl = 0;
    double sumsq = 0;
    clock_t t0;
    uint32_t strikes[SK_NV] = {0};
    sk_load(&SK_BIOMES[bi]);
    sk_c.rnd = 12345;
    sk_f.master = 3000;
    t0 = clock();
    for (i = 0; i < total; i += SK_CTL) {
        uint8_t c0[SK_NV];
        memcpy(c0, (const void *)sk_ui.cnt, SK_NV);
        sk_render(buf, SK_CTL);
        for (k = 0; k < SK_NV; k++)
            strikes[k] += (uint8_t)(sk_ui.cnt[k] - c0[k]);
        for (k = 0; k < 2 * SK_CTL; k++) {
            int32_t s = buf[k];
            if (s > 32767) s = 32767;
            if (s < -32767) s = -32767;
            pcm[2 * i + k] = (int16_t)s;
            if (abs(s) > peak) peak = abs(s);
            sumsq += (double)s * s;
        }
    }
    printf("biome %d %s: %.1f s, peak %d (%.1f dBFS), rms %.0f (%.1f dBFS), cpu %.2f s for %.1f s audio\n", bi, SK_BIOMES[bi].name,
           secs, peak, 20 * log10(peak / 32767.0 + 1e-9), sqrt(sumsq / (2.0 * total)),
           20 * log10(sqrt(sumsq / (2.0 * total)) / 32767.0 + 1e-9), (double)(clock() - t0) / CLOCKS_PER_SEC, secs);
    for (k = 0; k < SK_NV; k++)
        printf("  voice %u %-9s strikes %u\n", k, SK_TYPE_NAME[sk_s.type[k]], strikes[k]);
    (void)maxl;
    {
        FILE *f = fopen(path, "wb");
        uint32_t n = total * 4, v;
        fwrite("RIFF", 1, 4, f); v = 36 + n; fwrite(&v, 4, 1, f);
        fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
        { uint16_t a = 1, ch = 2; uint32_t r = 44100, br = 44100 * 4; uint16_t ba = 4, bits = 16;
          fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&r, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f); }
        fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f); fwrite(pcm, 1, n, f); fclose(f);
    }
    return 0;
}
