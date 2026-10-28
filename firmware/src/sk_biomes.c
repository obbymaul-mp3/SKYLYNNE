/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors */
/* SKYLYNNE's factory biomes: a biome is a whole soundscape, six sustained voices, one tempo, one scale and a room.
 *
 * The sound they are after: slow, hazy, weightless. Long drones that breathe, thin whistles high above them that
 * glide from note to note, a deep sub, a wide pad, a wash of air, all of it in a very large room. Nothing is struck.
 * Notes arrive every few seconds and take seconds to swell and fade, so the voices overlap and the harmony drifts.
 *
 * The voice columns are: type, base note, n, k, rot (the Euclidean pattern: a new note when it hits), prob, vel, pan,
 * len (a note's length in sixteenths at SUSTAIN 0: 48 is about 12 s at 60 bpm), hardness, reverb send, grain send,
 * span (the walk's reach). The biome's: name, key, scale, bpm, reverb time, delay sixteenths, grain size, grain
 * layers, macros (density, motion, colour, space). */

#define SK_MINOR {0, 2, 3, 5, 7, 8, 10, 0}
#define SK_DORIAN {0, 2, 3, 5, 7, 9, 10, 0}
#define SK_AEOLIAN_P {0, 3, 5, 7, 10, 0, 0, 0}                       /* minor pentatonic */
#define SK_LYDIAN {0, 2, 4, 6, 7, 9, 11, 0}
#define SK_PHRYGIAN {0, 1, 3, 5, 7, 8, 10, 0}

static const sk_biome_t SK_BIOMES[] = {
    /* 1: the signature: a low drone, a pad, two whistles and a breath over a slow sub */
    {"Velvet Mist", 9, 7, SK_MINOR, 60, 28, 3, 70, 3, {50, 40, 50, 90},
     {
         {VT_DRONE, 45, 32, 1, 0, 255, 76, -16, 88, 40, 70, 60, 1},
         {VT_PAD, 57, 32, 1, 12, 230, 70, 24, 64, 50, 100, 70, 2},
         {VT_WHISTLE, 93, 24, 1, 3, 190, 62, -40, 24, 60, 110, 90, 3},
         {VT_WHISTLE, 84, 32, 1, 20, 170, 56, 42, 28, 56, 110, 100, 3},
         {VT_AIR, 64, 24, 1, 8, 220, 60, 0, 44, 60, 110, 70, 2},
         {VT_SUB, 33, 64, 1, 0, 255, 90, 0, 160, 20, 40, 10, 1},
     }},
    /* 2: a harbour in fog: low and slow */
    {"Harbour Fog", 2, 5, SK_AEOLIAN_P, 48, 30, 2, 90, 5, {44, 30, 35, 100},
     {
         {VT_DRONE, 38, 40, 1, 0, 255, 78, -20, 110, 34, 80, 60, 1},
         {VT_DRONE, 50, 32, 1, 14, 210, 66, 30, 90, 40, 100, 70, 1},
         {VT_PAD, 55, 32, 1, 5, 220, 62, -30, 72, 44, 110, 80, 2},
         {VT_WHISTLE, 86, 40, 1, 2, 150, 54, 44, 30, 50, 120, 100, 2},
         {VT_AIR, 57, 24, 1, 9, 230, 62, 0, 60, 50, 120, 60, 2},
         {VT_SUB, 26, 64, 1, 0, 255, 92, 0, 180, 16, 50, 0, 1},
     }},
    /* 3: snow under a streetlight: bright, sparse, high */
    {"Snowlight", 7, 7, SK_LYDIAN, 66, 26, 2, 40, 3, {40, 46, 70, 80},
     {
         {VT_WHISTLE, 96, 24, 1, 0, 200, 60, 20, 24, 70, 110, 100, 3},
         {VT_WHISTLE, 88, 32, 1, 10, 180, 56, -36, 28, 64, 110, 100, 3},
         {VT_PAD, 64, 32, 1, 6, 220, 58, 0, 60, 66, 100, 80, 2},
         {VT_DRONE, 43, 40, 1, 0, 255, 70, -12, 96, 50, 80, 50, 1},
         {VT_AIR, 76, 24, 1, 4, 200, 56, 36, 40, 76, 120, 90, 2},
         {VT_SUB, 31, 64, 1, 0, 255, 80, 0, 160, 18, 40, 0, 1},
     }},
    /* 4: long drive: a deeper, darker one with a little more going on */
    {"Long Drive", 5, 7, SK_DORIAN, 72, 26, 2, 50, 3, {62, 56, 40, 70},
     {
         {VT_DRONE, 41, 24, 1, 0, 255, 84, -10, 72, 40, 60, 40, 2},
         {VT_DRONE, 53, 32, 1, 9, 230, 72, 30, 80, 46, 90, 70, 2},
         {VT_PAD, 57, 32, 1, 4, 230, 64, 40, 56, 52, 90, 60, 2},
         {VT_WHISTLE, 89, 24, 2, 3, 170, 56, -44, 20, 56, 110, 80, 3},
         {VT_AIR, 69, 24, 1, 0, 230, 52, -20, 40, 50, 110, 70, 2},
         {VT_SUB, 29, 48, 1, 0, 255, 96, 0, 120, 20, 30, 0, 1},
     }},
    /* 5: frost choir: whistles in a ring, a phrygian edge */
    {"Frost Choir", 4, 7, SK_PHRYGIAN, 54, 29, 3, 60, 7, {58, 60, 70, 100},
     {
         {VT_WHISTLE, 91, 20, 1, 0, 220, 60, 10, 22, 76, 110, 110, 4},
         {VT_WHISTLE, 84, 28, 1, 9, 200, 58, -34, 26, 70, 110, 110, 4},
         {VT_WHISTLE, 98, 36, 1, 17, 160, 50, 46, 24, 80, 120, 110, 3},
         {VT_PAD, 52, 32, 1, 5, 230, 62, -10, 60, 60, 110, 80, 2},
         {VT_DRONE, 40, 40, 1, 0, 255, 70, 16, 100, 44, 90, 50, 1},
         {VT_SUB, 28, 64, 1, 0, 255, 82, 0, 160, 18, 40, 0, 1},
     }},
    /* 6: tide: slow swells and a lot of air */
    {"Tide", 0, 5, SK_AEOLIAN_P, 42, 31, 3, 100, 5, {36, 56, 30, 110},
     {
         {VT_PAD, 48, 32, 1, 0, 255, 70, -30, 100, 40, 120, 60, 2},
         {VT_PAD, 60, 40, 1, 14, 220, 62, 30, 90, 46, 120, 70, 2},
         {VT_AIR, 67, 24, 1, 0, 255, 70, -10, 80, 50, 127, 40, 2},
         {VT_AIR, 55, 32, 1, 11, 220, 62, 20, 70, 44, 127, 50, 2},
         {VT_WHISTLE, 84, 40, 1, 5, 150, 48, -44, 36, 46, 120, 90, 2},
         {VT_SUB, 24, 64, 1, 0, 255, 90, 0, 200, 14, 50, 0, 1},
     }},
    /* 7: rain wire: thin and close, a wire humming in wind */
    {"Rain Wire", 10, 5, SK_AEOLIAN_P, 60, 24, 2, 30, 2, {48, 60, 60, 50},
     {
         {VT_WHISTLE, 94, 16, 1, 0, 210, 58, -20, 18, 84, 70, 60, 3},
         {VT_AIR, 79, 16, 1, 5, 240, 54, 30, 24, 84, 90, 30, 2},
         {VT_DRONE, 46, 32, 1, 0, 255, 70, -10, 64, 56, 70, 40, 1},
         {VT_PAD, 58, 32, 1, 9, 200, 56, 40, 48, 66, 70, 50, 2},
         {VT_WHISTLE, 86, 24, 1, 7, 180, 50, 24, 22, 80, 80, 60, 3},
         {VT_SUB, 34, 64, 1, 0, 255, 82, 0, 140, 22, 20, 0, 1},
     }},
    /* 8: afterglow: the lightest, a lydian dawn */
    {"Afterglow", 2, 7, SK_LYDIAN, 50, 30, 3, 60, 7, {40, 46, 70, 100},
     {
         {VT_PAD, 62, 32, 1, 0, 230, 66, -24, 80, 56, 120, 80, 2},
         {VT_WHISTLE, 90, 32, 1, 4, 190, 58, 30, 30, 60, 120, 100, 3},
         {VT_DRONE, 50, 40, 1, 0, 255, 72, -10, 96, 42, 100, 50, 1},
         {VT_AIR, 67, 24, 1, 6, 230, 56, 0, 60, 58, 120, 70, 2},
         {VT_WHISTLE, 98, 40, 1, 14, 150, 48, -40, 28, 64, 120, 100, 2},
         {VT_SUB, 38, 64, 1, 0, 255, 80, 0, 180, 18, 30, 10, 1},
     }},
};
#define SK_NBIOME ((uint32_t)(sizeof SK_BIOMES / sizeof SK_BIOMES[0]))
