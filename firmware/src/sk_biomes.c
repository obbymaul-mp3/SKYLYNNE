/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* SKYLYNNE's factory biomes: a biome is a whole soundscape, six voices, one tempo, one scale and a room.
 *
 * The sound they are after: cold, glittering, weightless. Bells and glass that ring into a huge reverb, pads that sit
 * a little out of tune with themselves, plucks like small ice, breath, and a deep slow sub underneath. Slow tempos,
 * minor and modal scales, few notes with a lot of room around each. Think drained city at night, a harbour in fog,
 * snow under a streetlight.
 *
 * The voice columns are: type, material/variant, base note, n, k, rot (the Euclidean pattern), prob, vel, pan,
 * decay (or length in sixteenths for pads and air), hardness, reverb send, grain send, span (the walk's reach).
 * The biome's: name, key, scale, bpm, reverb time, delay sixteenths, grain size, grain layers, macros
 * (density, motion, colour, space). */

#define SK_MINOR {0, 2, 3, 5, 7, 8, 10, 0}
#define SK_DORIAN {0, 2, 3, 5, 7, 9, 10, 0}
#define SK_AEOLIAN_P {0, 3, 5, 7, 10, 0, 0, 0}                       /* minor pentatonic */
#define SK_LYDIAN {0, 2, 4, 6, 7, 9, 11, 0}
#define SK_PHRYGIAN {0, 1, 3, 5, 7, 8, 10, 0}
#define SK_HIRAJOSHI {0, 2, 3, 7, 8, 0, 0, 0}

static const sk_biome_t SK_BIOMES[] = {
    /* 1: cold glass over a slow sub; the signature sound */
    {"Velvet Mist", 9, 7, SK_MINOR, 68, 26, 3, 70, 3, {58, 40, 70, 80},
     {
         {VT_PAD, 0, 57, 16, 2, 0, 255, 80, -20, 14, 40, 90, 60, 4},      /* a wide pad that breathes */
         {VT_BELL, 1, 81, 16, 5, 2, 200, 84, 30, 14, 70, 110, 80, 5},     /* glass bells, high */
         {VT_PLUCK, 0, 69, 12, 5, 1, 190, 70, -34, 12, 80, 90, 70, 4},    /* small ice plucks */
         {VT_RES, 1, 76, 16, 3, 4, 160, 62, 44, 15, 60, 120, 100, 3},     /* far glass */
         {VT_AIR, 0, 64, 16, 1, 0, 255, 50, 0, 12, 40, 100, 50, 2},       /* a breath */
         {VT_PAD, 0, 33, 16, 1, 0, 255, 90, 0, 24, 20, 40, 10, 1},        /* the sub, one long note */
     }},
    /* 2: a harbour in fog: low, slow, dark bells */
    {"Harbour Fog", 2, 5, SK_AEOLIAN_P, 54, 28, 2, 90, 5, {44, 30, 40, 100},
     {
         {VT_PAD, 0, 50, 16, 2, 0, 255, 76, -30, 20, 30, 110, 40, 3},
         {VT_RES, 0, 62, 16, 3, 0, 170, 78, 20, 17, 50, 120, 90, 4},      /* far bells */
         {VT_BELL, 3, 74, 12, 3, 2, 140, 62, 40, 16, 40, 110, 100, 4},
         {VT_AIR, 0, 55, 16, 1, 0, 255, 60, -10, 16, 30, 120, 40, 2},
         {VT_PLUCK, 3, 58, 16, 3, 5, 150, 66, -44, 14, 40, 90, 60, 3},
         {VT_PAD, 0, 26, 16, 1, 0, 255, 92, 0, 28, 10, 50, 10, 1},
     }},
    /* 3: snow under a streetlight: bright, sparse, high */
    {"Snowlight", 7, 7, SK_LYDIAN, 78, 24, 2, 40, 3, {50, 50, 85, 70},
     {
         {VT_BELL, 0, 84, 16, 5, 0, 220, 80, 20, 13, 90, 100, 90, 4},
         {VT_BELL, 2, 91, 12, 4, 3, 180, 66, -40, 12, 100, 110, 100, 3},
         {VT_PLUCK, 0, 72, 16, 7, 2, 190, 72, 36, 12, 90, 100, 60, 4},
         {VT_RES, 1, 79, 16, 3, 6, 150, 60, -16, 15, 80, 120, 90, 3},
         {VT_PAD, 0, 64, 16, 2, 0, 255, 60, 0, 20, 70, 110, 80, 3},
         {VT_PAD, 0, 36, 16, 1, 0, 255, 80, 0, 26, 10, 40, 0, 1},
     }},
    /* 4: nightdrive: pulsing, a little more movement */
    {"Nightdrive", 5, 7, SK_DORIAN, 92, 22, 2, 50, 3, {72, 62, 55, 60},
     {
         {VT_PLUCK, 3, 53, 16, 5, 0, 255, 90, -10, 12, 60, 60, 20, 3},
         {VT_PLUCK, 0, 65, 16, 7, 3, 220, 78, 30, 12, 90, 90, 40, 4},
         {VT_BELL, 1, 77, 16, 5, 2, 190, 76, -36, 13, 80, 100, 80, 4},
         {VT_PAD, 0, 57, 16, 3, 0, 255, 66, 40, 18, 50, 110, 60, 3},
         {VT_AIR, 0, 69, 16, 1, 0, 255, 44, -44, 10, 50, 110, 60, 2},
         {VT_PAD, 0, 29, 16, 2, 0, 255, 96, 0, 22, 20, 30, 0, 1},
     }},
    /* 5: ice garden: dense and glittering, phrygian edge */
    {"Ice Garden", 4, 7, SK_PHRYGIAN, 64, 27, 3, 60, 7, {84, 70, 90, 90},
     {
         {VT_BELL, 0, 88, 16, 9, 0, 230, 70, 10, 11, 100, 110, 100, 5},
         {VT_BELL, 1, 76, 16, 7, 2, 210, 74, -30, 12, 90, 110, 90, 5},
         {VT_RES, 4, 64, 16, 5, 1, 190, 64, 44, 14, 70, 120, 100, 4},
         {VT_PLUCK, 0, 83, 12, 7, 3, 200, 66, -50, 11, 100, 100, 80, 4},
         {VT_PAD, 0, 52, 16, 2, 0, 255, 60, 0, 22, 70, 120, 90, 3},
         {VT_AIR, 0, 72, 16, 1, 0, 255, 46, 20, 14, 70, 120, 60, 2},
     }},
    /* 6: tide: slow swells and a lot of air */
    {"Tide", 0, 5, SK_AEOLIAN_P, 48, 29, 3, 100, 5, {36, 56, 35, 110},
     {
         {VT_PAD, 0, 48, 16, 1, 0, 255, 74, -30, 30, 40, 120, 60, 3},
         {VT_PAD, 0, 60, 16, 1, 8, 255, 64, 30, 28, 60, 120, 70, 3},
         {VT_AIR, 0, 67, 16, 1, 0, 255, 70, -10, 28, 30, 127, 40, 2},
         {VT_RES, 3, 72, 16, 2, 4, 150, 58, 40, 18, 50, 120, 90, 3},
         {VT_BELL, 1, 79, 16, 3, 1, 140, 60, -40, 16, 70, 120, 90, 3},
         {VT_PAD, 0, 24, 16, 1, 0, 255, 90, 0, 32, 10, 60, 0, 1},
     }},
    /* 7: tin roof rain: wood and metal, close and small */
    {"Tin Roof", 10, 5, SK_AEOLIAN_P, 86, 20, 2, 30, 2, {64, 66, 50, 40},
     {
         {VT_RES, 5, 60, 16, 7, 0, 230, 84, -20, 8, 40, 60, 20, 4},
         {VT_RES, 2, 72, 16, 5, 2, 200, 76, 30, 9, 50, 70, 40, 4},
         {VT_PLUCK, 3, 55, 12, 5, 1, 180, 72, -40, 10, 50, 60, 30, 3},
         {VT_RES, 4, 67, 16, 3, 5, 150, 60, 44, 11, 40, 80, 60, 3},
         {VT_AIR, 0, 79, 16, 2, 0, 255, 44, 10, 8, 60, 90, 30, 2},
         {VT_PAD, 0, 34, 16, 1, 0, 255, 86, 0, 20, 10, 20, 0, 1},
     }},
    /* 8: afterglow: the lightest, a lydian dawn */
    {"Afterglow", 2, 7, SK_LYDIAN, 60, 28, 3, 60, 7, {40, 46, 80, 100},
     {
         {VT_PAD, 0, 62, 16, 2, 0, 255, 70, -24, 18, 70, 120, 80, 3},
         {VT_BELL, 1, 86, 16, 3, 0, 180, 70, 30, 14, 80, 120, 100, 4},
         {VT_PLUCK, 0, 74, 12, 3, 2, 160, 62, -34, 13, 70, 110, 80, 4},
         {VT_AIR, 0, 67, 16, 1, 0, 255, 52, 0, 20, 60, 120, 60, 2},
         {VT_RES, 0, 79, 16, 2, 5, 140, 56, 44, 16, 60, 127, 100, 3},
         {VT_PAD, 0, 38, 16, 1, 0, 255, 82, 0, 28, 20, 60, 10, 1},
     }},
};
#define SK_NBIOME ((uint32_t)(sizeof SK_BIOMES / sizeof SK_BIOMES[0]))
