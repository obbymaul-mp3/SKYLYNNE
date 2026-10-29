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
#define SK_MAJPENT {0, 2, 4, 7, 9, 0, 0, 0}
#define SK_MAJOR {0, 2, 4, 5, 7, 9, 11, 0}
#define SK_WHOLE {0, 2, 4, 6, 8, 10, 0, 0}
#define SK_HIRAJOSHI {0, 2, 3, 7, 8, 0, 0, 0}

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

    /* ---- the cathedral: choirs, an organ, horns, a bell that rings now and then; the forge and the stomp very sparse ---- */
    /* 9: a gilded nave: two choirs over an organ, a horn far off, a bell */
    {"Gilded Nave", 2, 7, SK_DORIAN, 46, 30, 3, 80, 3, {45, 40, 55, 95},
     {
         {VT_CHOIR, 62, 32, 1, 0, 230, 70, -22, 70, 60, 120, 70, 2},
         {VT_CHOIR, 74, 40, 1, 9, 170, 56, 30, 56, 80, 120, 90, 3},
         {VT_ORGAN, 50, 32, 1, 4, 240, 66, 0, 96, 40, 100, 50, 1},
         {VT_BRASS, 46, 48, 1, 20, 130, 62, 40, 40, 50, 110, 40, 2},
         {VT_BELL, 74, 64, 1, 30, 90, 54, -40, 56, 60, 127, 90, 3},
         {VT_SUB, 26, 64, 1, 0, 255, 80, 0, 180, 20, 40, 0, 1},
     }, 1},
    /* 10: the smith's hall: a low phrygian drone, an anvil far down the hall, a choir of men */
    {"Smith's Hall", 4, 7, SK_PHRYGIAN, 42, 26, 2, 40, 5, {40, 50, 45, 80},
     {
         {VT_DRONE, 40, 40, 1, 0, 255, 71, -14, 110, 36, 90, 50, 1},
         {VT_FORGE, 64, 96, 1, 10, 120, 51, -30, 16, 70, 127, 60, 2},
         {VT_CHOIR, 52, 32, 1, 6, 220, 55, 20, 66, 30, 110, 70, 2},
         {VT_BRASS, 40, 48, 1, 16, 140, 53, 36, 44, 44, 110, 40, 2},
         {VT_STOMP, 30, 128, 1, 40, 150, 51, 0, 18, 40, 110, 0, 1},
         {VT_SUB, 28, 64, 1, 0, 255, 63, 0, 190, 16, 40, 0, 1},
     }, 1},
    /* 11: a stone colossus: heavy and slow, a footfall every minute, horns and a deep bell */
    {"Stone Colossus", 7, 5, SK_AEOLIAN_P, 38, 28, 2, 90, 5, {38, 44, 35, 100},
     {
         {VT_STOMP, 28, 64, 1, 0, 190, 52, 0, 20, 40, 120, 0, 1},
         {VT_BRASS, 43, 48, 1, 9, 150, 56, -30, 52, 40, 110, 50, 2},
         {VT_DRONE, 36, 40, 1, 0, 255, 54, 24, 120, 30, 80, 60, 1},
         {VT_BELL, 55, 96, 1, 22, 100, 40, 40, 70, 50, 127, 80, 2},
         {VT_CHOIR, 50, 40, 1, 14, 190, 48, -40, 60, 24, 120, 90, 2},
         {VT_SUB, 24, 64, 1, 0, 255, 62, 0, 200, 14, 50, 0, 1},
     }, 1},
    /* 12: a fading sun: high voices, a soft organ, a whistle through a window */
    {"Fading Sun", 9, 7, SK_LYDIAN, 52, 31, 3, 60, 7, {38, 46, 70, 100},
     {
         {VT_CHOIR, 69, 32, 1, 0, 220, 48, -26, 64, 70, 120, 90, 3},
         {VT_CHOIR, 81, 40, 1, 11, 160, 38, 34, 52, 80, 120, 100, 3},
         {VT_ORGAN, 57, 32, 1, 5, 230, 52, 0, 88, 24, 110, 60, 1},
         {VT_WHISTLE, 93, 40, 1, 14, 150, 46, 44, 28, 66, 120, 100, 2},
         {VT_BELL, 81, 96, 1, 40, 100, 36, -44, 60, 90, 127, 100, 2},
         {VT_SUB, 33, 64, 1, 0, 255, 72, 0, 170, 18, 40, 10, 1},
     }, 1},

    /* ---- the deep: whale song, glass and bowls, a drop now and then, ancient bells and engines ---- */
    /* 13: a sunken court: whales over glass, drips from the vault */
    {"Sunken Court", 5, 7, SK_LYDIAN, 44, 30, 3, 90, 3, {42, 52, 45, 100},
     {
         {VT_WHALE, 50, 48, 1, 0, 150, 66, -24, 60, 50, 120, 60, 2},
         {VT_GLASS, 66, 32, 1, 8, 220, 60, 28, 64, 60, 110, 90, 2},
         {VT_DRIP, 80, 24, 3, 5, 120, 62, -40, 8, 70, 127, 60, 3},
         {VT_PAD, 57, 32, 1, 12, 220, 60, 36, 80, 40, 120, 70, 2},
         {VT_AIR, 62, 24, 1, 6, 220, 56, 0, 60, 44, 120, 60, 2},
         {VT_SUB, 29, 64, 1, 0, 255, 84, 0, 190, 16, 40, 0, 1},
     }, 2},
    /* 14: coral lanterns: bright, buoyant, a lot of glass */
    {"Coral Lanterns", 7, 5, SK_MAJPENT, 56, 28, 2, 60, 5, {50, 54, 70, 90},
     {
         {VT_GLASS, 74, 24, 1, 0, 230, 62, -20, 40, 76, 110, 90, 3},
         {VT_GLASS, 62, 32, 1, 9, 200, 56, 26, 52, 66, 110, 80, 2},
         {VT_DRIP, 86, 20, 3, 4, 150, 60, -44, 8, 80, 127, 70, 3},
         {VT_BELL, 79, 96, 1, 24, 110, 50, 44, 40, 80, 127, 90, 2},
         {VT_WHISTLE, 93, 40, 1, 12, 150, 48, 12, 28, 60, 120, 90, 2},
         {VT_PAD, 55, 32, 1, 5, 220, 56, 0, 70, 50, 110, 60, 2},
     }, 2},
    /* 15: the abyss engine: sub, a drone, a whale far below, a gong, a drop in the dark */
    {"Abyss Engine", 1, 7, SK_PHRYGIAN, 38, 29, 3, 100, 5, {36, 50, 25, 110},
     {
         {VT_SUB, 25, 64, 1, 0, 255, 59, 0, 200, 14, 50, 0, 1},
         {VT_DRONE, 37, 40, 1, 0, 255, 53, -20, 120, 30, 90, 50, 1},
         {VT_WHALE, 43, 64, 1, 12, 130, 55, 30, 80, 36, 127, 70, 2},
         {VT_BELL, 49, 128, 1, 40, 110, 37, -36, 80, 40, 127, 90, 2},
         {VT_DRIP, 62, 32, 1, 10, 90, 45, 40, 8, 40, 127, 60, 3},
         {VT_AIR, 50, 24, 1, 0, 230, 43, 0, 70, 36, 120, 60, 2},
     }, 2},
    /* 16: light through water: voices without words, slow whales, high glass */
    {"Light Through Water", 2, 7, SK_LYDIAN, 50, 31, 3, 70, 7, {40, 48, 65, 100},
     {
         {VT_PAD, 62, 32, 1, 0, 230, 64, -26, 80, 56, 120, 80, 2},
         {VT_CHOIR, 69, 40, 1, 7, 170, 50, 30, 60, 14, 120, 90, 2},
         {VT_WHALE, 62, 64, 1, 18, 120, 56, -40, 64, 60, 127, 70, 2},
         {VT_GLASS, 81, 32, 1, 6, 200, 52, 40, 56, 80, 120, 100, 3},
         {VT_WHISTLE, 88, 40, 1, 14, 150, 46, 0, 28, 60, 120, 100, 2},
         {VT_AIR, 74, 24, 1, 8, 220, 52, 20, 60, 56, 120, 70, 2},
     }, 2},

    /* ---- Columbia: a floating neoclassical city, heavenly and a little wrong ---- */
    /* 16 */
    {"Sky Garden", 7, 7, SK_LYDIAN, 52, 29, 2, 60, 7, {45, 45, 65, 95},
     {
         {VT_STRING, 62, 32, 1, 0, 220, 58, -24, 64, 70, 110, 70, 2},
         {VT_HARP, 74, 24, 3, 4, 170, 56, 30, 14, 70, 120, 80, 3},
         {VT_GLASS, 79, 32, 1, 9, 200, 52, -40, 56, 80, 110, 100, 3},
         {VT_CHOIR, 69, 40, 1, 14, 150, 46, 40, 50, 80, 120, 90, 2},
         {VT_PAD, 55, 32, 1, 5, 220, 54, 0, 80, 60, 100, 60, 2},
         {VT_SUB, 31, 64, 1, 0, 255, 66, 0, 170, 18, 40, 0, 1},
     }, 3},
    /* 17 */
    {"Marble Hymn", 0, 7, SK_MAJOR, 48, 30, 3, 70, 3, {40, 38, 55, 95},
     {
         {VT_ORGAN, 48, 32, 1, 0, 240, 52, 0, 100, 50, 100, 50, 1},
         {VT_CHOIR, 65, 32, 1, 7, 210, 54, -30, 66, 70, 120, 80, 2},
         {VT_STRING, 60, 40, 1, 12, 190, 52, 34, 60, 60, 110, 70, 2},
         {VT_BELL, 77, 64, 1, 30, 100, 46, -40, 56, 70, 127, 90, 3},
         {VT_BRASS, 53, 48, 1, 20, 120, 50, 40, 44, 50, 110, 40, 2},
         {VT_SUB, 24, 64, 1, 0, 255, 64, 0, 180, 16, 40, 0, 1},
     }, 3},
    /* 18 */
    {"Warped Carousel", 3, 5, SK_HIRAJOSHI, 60, 27, 2, 50, 5, {50, 60, 40, 80},
     {
         {VT_MBOX, 72, 20, 3, 0, 150, 52, -30, 12, 70, 120, 60, 3},
         {VT_MBOX, 84, 28, 3, 5, 120, 46, 34, 12, 80, 120, 70, 3},
         {VT_STRING, 55, 32, 1, 9, 200, 50, -12, 60, 30, 110, 60, 2},
         {VT_DRONE, 39, 40, 1, 0, 255, 62, 20, 100, 30, 90, 60, 1},
         {VT_WHISTLE, 91, 40, 1, 14, 140, 44, 44, 26, 60, 120, 90, 3},
         {VT_SUB, 27, 64, 1, 0, 255, 66, 0, 150, 18, 40, 0, 1},
     }, 3},
    /* 19 */
    {"Under the Clouds", 5, 7, SK_MINOR, 40, 30, 3, 90, 5, {38, 44, 30, 100},
     {
         {VT_PAD, 53, 32, 1, 0, 230, 56, -24, 100, 40, 120, 70, 2},
         {VT_STRING, 48, 40, 1, 10, 200, 52, 28, 80, 30, 110, 60, 2},
         {VT_BELL, 65, 96, 1, 26, 100, 46, 40, 60, 40, 127, 80, 2},
         {VT_DRONE, 41, 40, 1, 0, 255, 62, -8, 120, 26, 90, 50, 1},
         {VT_AIR, 62, 24, 1, 7, 220, 50, 0, 70, 40, 120, 60, 2},
         {VT_SUB, 29, 64, 1, 0, 255, 66, 0, 190, 14, 50, 0, 1},
     }, 3},

    /* ---- Babel: a tower to heaven; brass, voices of many tongues, a Shepard rise that never arrives ---- */
    /* 20 */
    {"Brick and Sun", 9, 7, SK_DORIAN, 44, 28, 2, 50, 5, {42, 44, 45, 85},
     {
         {VT_BRASS, 45, 48, 1, 0, 170, 56, -28, 44, 50, 110, 40, 2},
         {VT_CHOIR, 57, 32, 1, 9, 210, 52, 26, 66, 30, 110, 70, 2},
         {VT_FORGE, 60, 96, 1, 10, 110, 50, -36, 14, 70, 127, 60, 2},
         {VT_DRONE, 33, 40, 1, 0, 255, 60, 0, 120, 34, 80, 50, 1},
         {VT_STOMP, 28, 128, 1, 40, 140, 52, 10, 18, 40, 110, 0, 1},
         {VT_SUB, 26, 64, 1, 0, 255, 62, 0, 190, 14, 40, 0, 1},
     }, 4},
    /* 21 */
    {"Spiral Ascent", 4, 7, SK_LYDIAN, 50, 31, 3, 80, 7, {40, 50, 55, 100},
     {
         {VT_ASCENT, 24, 64, 1, 0, 255, 60, -30, 200, 60, 120, 60, 1},
         {VT_ASCENT, 31, 64, 1, 20, 255, 54, 30, 200, 80, 120, 70, 1},
         {VT_STRING, 52, 32, 1, 6, 210, 50, 0, 70, 60, 110, 60, 2},
         {VT_CHOIR, 64, 40, 1, 11, 160, 46, -40, 56, 70, 120, 90, 2},
         {VT_BELL, 76, 96, 1, 30, 100, 44, 40, 60, 60, 127, 90, 3},
         {VT_SUB, 28, 64, 1, 0, 255, 62, 0, 180, 16, 40, 0, 1},
     }, 4},
    /* 22 */
    {"Many Tongues", 2, 7, SK_PHRYGIAN, 46, 29, 3, 70, 5, {48, 56, 40, 100},
     {
         {VT_CHOIR, 52, 32, 1, 0, 230, 54, -34, 60, 50, 120, 80, 2},
         {VT_CHOIR, 60, 40, 1, 9, 200, 52, 10, 56, 60, 120, 90, 3},
         {VT_CHOIR, 67, 48, 1, 17, 170, 46, 40, 50, 80, 120, 100, 3},
         {VT_WHISTLE, 86, 40, 1, 5, 140, 44, -12, 28, 60, 120, 90, 2},
         {VT_BELL, 62, 96, 1, 32, 100, 44, 24, 60, 40, 127, 80, 2},
         {VT_SUB, 26, 64, 1, 0, 255, 62, 0, 190, 14, 40, 0, 1},
     }, 4},
    /* 23 */
    {"Heaven's Gate", 7, 7, SK_MAJOR, 54, 30, 3, 60, 7, {40, 40, 60, 100},
     {
         {VT_ORGAN, 50, 32, 1, 0, 240, 50, -10, 96, 60, 100, 50, 1},
         {VT_CHOIR, 69, 32, 1, 8, 200, 50, 30, 60, 80, 120, 90, 3},
         {VT_GLASS, 81, 40, 1, 13, 180, 48, -40, 56, 80, 110, 100, 3},
         {VT_ASCENT, 36, 64, 1, 0, 255, 50, 20, 200, 80, 120, 70, 1},
         {VT_BELL, 88, 128, 1, 50, 100, 42, 40, 64, 80, 127, 100, 2},
         {VT_SUB, 31, 64, 1, 0, 255, 62, 0, 180, 16, 40, 0, 1},
     }, 4},

    /* ---- Lothlorien: golden trees and elevated dwellings, harps and voices ---- */
    /* 24 */
    {"Golden Wood", 4, 7, SK_LYDIAN, 54, 29, 2, 60, 7, {45, 45, 60, 95},
     {
         {VT_HARP, 76, 20, 3, 0, 150, 54, -30, 16, 70, 120, 70, 3},
         {VT_CHOIR, 71, 32, 1, 8, 200, 48, 26, 56, 70, 120, 80, 2},
         {VT_STRING, 59, 40, 1, 13, 200, 52, -10, 64, 60, 110, 70, 2},
         {VT_WHISTLE, 93, 40, 1, 5, 150, 44, 40, 28, 70, 120, 90, 3},
         {VT_PAD, 64, 32, 1, 10, 220, 50, 0, 80, 50, 110, 60, 2},
         {VT_SUB, 28, 64, 1, 0, 255, 62, 0, 170, 16, 40, 0, 1},
     }, 5},
    /* 25 */
    {"Evenstar Flet", 9, 5, SK_MAJPENT, 46, 30, 3, 70, 7, {40, 42, 65, 100},
     {
         {VT_CHOIR, 69, 32, 1, 0, 220, 50, -26, 60, 90, 120, 90, 2},
         {VT_CHOIR, 76, 40, 1, 9, 170, 44, 30, 52, 90, 120, 100, 3},
         {VT_HARP, 81, 32, 2, 5, 130, 50, -40, 16, 70, 127, 80, 3},
         {VT_STRING, 57, 40, 1, 12, 200, 50, 10, 70, 50, 110, 60, 2},
         {VT_WHISTLE, 88, 40, 1, 3, 140, 42, 42, 28, 60, 120, 90, 2},
         {VT_SUB, 33, 64, 1, 0, 255, 60, 0, 180, 16, 40, 10, 1},
     }, 5},
    /* 26 */
    {"Starlit Mallorn", 2, 7, SK_LYDIAN, 56, 28, 3, 60, 7, {48, 52, 70, 95},
     {
         {VT_MBOX, 79, 20, 3, 0, 120, 48, -36, 12, 80, 127, 80, 3},
         {VT_GLASS, 72, 32, 1, 7, 200, 50, 30, 56, 70, 110, 100, 3},
         {VT_PAD, 62, 32, 1, 12, 220, 52, 0, 80, 60, 110, 70, 2},
         {VT_AIR, 69, 24, 1, 5, 220, 48, -16, 60, 60, 120, 60, 2},
         {VT_WHISTLE, 96, 40, 1, 9, 140, 40, 42, 26, 70, 120, 100, 3},
         {VT_SUB, 26, 64, 1, 0, 255, 60, 0, 180, 16, 40, 0, 1},
     }, 5},
    /* 27 */
    {"Leaf Fall", 7, 5, SK_MAJPENT, 50, 28, 2, 70, 5, {44, 48, 55, 90},
     {
         {VT_HARP, 67, 24, 3, 0, 150, 52, -26, 16, 60, 110, 70, 3},
         {VT_DRIP, 79, 32, 3, 6, 120, 46, 36, 8, 70, 127, 60, 3},
         {VT_AIR, 60, 24, 1, 5, 220, 48, 0, 70, 50, 120, 60, 2},
         {VT_STRING, 55, 40, 1, 11, 200, 50, -10, 64, 50, 110, 60, 2},
         {VT_WHISTLE, 90, 40, 1, 3, 140, 42, 40, 26, 60, 120, 90, 2},
         {VT_SUB, 31, 64, 1, 0, 255, 60, 0, 170, 16, 40, 0, 1},
     }, 5},

    /* ---- Yharnam: Victorian rooftops in fog, music boxes and a blood moon ---- */
    /* 28 */
    {"Gaslit Rooftops", 11, 7, SK_MINOR, 42, 28, 2, 50, 5, {42, 50, 35, 90},
     {
         {VT_MBOX, 71, 28, 3, 0, 110, 48, -30, 12, 60, 120, 60, 3},
         {VT_STRING, 47, 40, 1, 9, 200, 50, 24, 70, 30, 110, 60, 2},
         {VT_ORGAN, 47, 32, 1, 4, 220, 46, 0, 90, 30, 100, 40, 1},
         {VT_WHISTLE, 85, 40, 1, 13, 130, 42, 40, 26, 50, 120, 80, 3},
         {VT_DRONE, 35, 40, 1, 0, 255, 58, -14, 120, 26, 80, 50, 1},
         {VT_SUB, 23, 64, 1, 0, 255, 62, 0, 190, 14, 40, 0, 1},
     }, 6},
    /* 29 */
    {"Fog Bells", 4, 7, SK_PHRYGIAN, 38, 30, 3, 100, 5, {38, 44, 30, 105},
     {
         {VT_BELL, 64, 64, 1, 0, 130, 50, -30, 60, 50, 127, 80, 3},
         {VT_CHOIR, 55, 40, 1, 9, 190, 44, 28, 60, 14, 120, 90, 2},
         {VT_STRING, 52, 32, 1, 14, 200, 48, -10, 80, 30, 110, 60, 2},
         {VT_AIR, 50, 24, 1, 6, 230, 50, 0, 80, 36, 127, 60, 2},
         {VT_DRONE, 40, 40, 1, 0, 255, 58, 16, 120, 24, 90, 50, 1},
         {VT_SUB, 28, 64, 1, 0, 255, 62, 0, 190, 14, 40, 0, 1},
     }, 6},
    /* 30 */
    {"Blood Moon", 6, 7, SK_MINOR, 40, 29, 2, 60, 5, {42, 46, 35, 95},
     {
         {VT_ORGAN, 42, 32, 1, 0, 230, 48, -12, 100, 30, 100, 40, 1},
         {VT_BRASS, 42, 48, 1, 10, 130, 50, 30, 44, 40, 110, 40, 2},
         {VT_CHOIR, 57, 40, 1, 15, 180, 44, -34, 60, 30, 120, 80, 2},
         {VT_FORGE, 62, 96, 1, 22, 100, 46, 40, 14, 60, 127, 60, 2},
         {VT_DRONE, 30, 40, 1, 0, 255, 58, 0, 120, 24, 80, 50, 1},
         {VT_SUB, 24, 64, 1, 0, 255, 62, 0, 190, 14, 40, 0, 1},
     }, 6},
    /* 31 */
    {"Hollow Lullaby", 9, 7, SK_MINOR, 46, 29, 3, 60, 5, {44, 50, 40, 95},
     {
         {VT_MBOX, 76, 24, 3, 0, 130, 48, -30, 12, 70, 120, 70, 3},
         {VT_MBOX, 83, 36, 3, 7, 110, 42, 34, 12, 80, 120, 80, 3},
         {VT_STRING, 57, 32, 1, 10, 200, 48, 0, 64, 40, 110, 60, 2},
         {VT_WHISTLE, 86, 40, 1, 5, 120, 40, 42, 26, 50, 120, 80, 2},
         {VT_PAD, 50, 32, 1, 14, 220, 50, -20, 80, 40, 110, 60, 2},
         {VT_SUB, 28, 64, 1, 0, 255, 60, 0, 180, 14, 40, 0, 1},
     }, 6},

    /* ---- Minas Tirith: a white city climbing a mountain, horns and strings ---- */
    /* 32 */
    {"White City", 2, 7, SK_MAJOR, 50, 29, 3, 60, 5, {40, 40, 60, 95},
     {
         {VT_BRASS, 50, 48, 1, 0, 140, 52, -28, 44, 60, 110, 40, 2},
         {VT_CHOIR, 66, 32, 1, 8, 200, 50, 26, 62, 70, 120, 80, 2},
         {VT_STRING, 62, 40, 1, 13, 200, 50, -10, 64, 60, 110, 70, 2},
         {VT_ORGAN, 50, 32, 1, 5, 220, 46, 0, 90, 40, 100, 40, 1},
         {VT_BELL, 74, 64, 1, 30, 100, 44, 40, 56, 60, 127, 90, 3},
         {VT_SUB, 26, 64, 1, 0, 255, 62, 0, 180, 16, 40, 0, 1},
     }, 7},
    /* 33 */
    {"Seven Gates", 7, 7, SK_DORIAN, 42, 28, 2, 50, 5, {40, 46, 40, 90},
     {
         {VT_STOMP, 28, 128, 1, 0, 150, 52, 0, 18, 40, 110, 0, 1},
         {VT_BRASS, 43, 48, 1, 9, 150, 52, -30, 44, 40, 110, 40, 2},
         {VT_CHOIR, 52, 32, 1, 14, 200, 48, 28, 66, 30, 110, 70, 2},
         {VT_DRONE, 31, 40, 1, 0, 255, 58, -12, 120, 30, 80, 50, 1},
         {VT_BELL, 55, 96, 1, 26, 100, 44, 40, 64, 50, 127, 80, 2},
         {VT_SUB, 24, 64, 1, 0, 255, 62, 0, 190, 14, 40, 0, 1},
     }, 7},
    /* 34 */
    {"Dawn Fields", 5, 7, SK_LYDIAN, 52, 30, 3, 60, 7, {42, 44, 65, 100},
     {
         {VT_STRING, 60, 32, 1, 0, 220, 52, -20, 64, 70, 110, 70, 2},
         {VT_WHISTLE, 91, 40, 1, 6, 150, 44, 36, 28, 70, 120, 90, 3},
         {VT_HARP, 74, 24, 3, 4, 140, 50, -36, 16, 70, 120, 80, 3},
         {VT_CHOIR, 72, 40, 1, 12, 170, 44, 30, 54, 80, 120, 90, 2},
         {VT_PAD, 55, 32, 1, 8, 220, 50, 0, 80, 50, 110, 60, 2},
         {VT_AIR, 67, 24, 1, 5, 220, 46, 10, 60, 60, 120, 60, 2},
     }, 7},
    /* 35 */
    {"Silent Citadel", 9, 7, SK_MINOR, 40, 30, 3, 90, 5, {36, 42, 30, 100},
     {
         {VT_ORGAN, 45, 32, 1, 0, 230, 46, 0, 100, 30, 100, 40, 1},
         {VT_BELL, 69, 64, 1, 30, 110, 46, -36, 60, 50, 127, 80, 2},
         {VT_STRING, 57, 40, 1, 9, 200, 48, 30, 80, 40, 110, 60, 2},
         {VT_WHISTLE, 88, 40, 1, 13, 120, 40, 40, 26, 50, 120, 80, 2},
         {VT_DRONE, 33, 40, 1, 0, 255, 58, -14, 120, 26, 80, 50, 1},
         {VT_SUB, 24, 64, 1, 0, 255, 62, 0, 190, 14, 40, 0, 1},
     }, 7},
};
#define SK_NBIOME ((uint32_t)(sizeof SK_BIOMES / sizeof SK_BIOMES[0]))

/* the sounds each world offers: ALGORITHM walks this list for the selected voice */
static const uint8_t SK_WN[8] = {5, 10, 10, 12, 12, 12, 12, 12};
static const uint8_t SK_WL[8][12] = {
    {VT_DRONE, VT_WHISTLE, VT_SUB, VT_PAD, VT_AIR},
    {VT_CHOIR, VT_BRASS, VT_ORGAN, VT_BELL, VT_FORGE, VT_STOMP, VT_DRONE, VT_PAD, VT_SUB, VT_WHISTLE},
    {VT_WHALE, VT_DRIP, VT_GLASS, VT_BELL, VT_PAD, VT_AIR, VT_SUB, VT_DRONE, VT_WHISTLE, VT_CHOIR},
    {VT_STRING, VT_CHOIR, VT_HARP, VT_GLASS, VT_MBOX, VT_ORGAN, VT_BELL, VT_PAD, VT_WHISTLE, VT_DRONE, VT_SUB, VT_AIR},
    {VT_BRASS, VT_CHOIR, VT_FORGE, VT_STOMP, VT_ASCENT, VT_STRING, VT_ORGAN, VT_BELL, VT_DRONE, VT_SUB, VT_PAD, VT_WHISTLE},
    {VT_HARP, VT_CHOIR, VT_STRING, VT_WHISTLE, VT_GLASS, VT_BELL, VT_DRIP, VT_PAD, VT_AIR, VT_MBOX, VT_DRONE, VT_SUB},
    {VT_MBOX, VT_STRING, VT_ORGAN, VT_BELL, VT_CHOIR, VT_BRASS, VT_FORGE, VT_STOMP, VT_WHISTLE, VT_DRONE, VT_SUB, VT_AIR},
    {VT_BRASS, VT_CHOIR, VT_STRING, VT_ORGAN, VT_BELL, VT_HARP, VT_STOMP, VT_WHISTLE, VT_PAD, VT_DRONE, VT_SUB, VT_AIR},
};
/* the next (d = 1) or previous sound in the world's list; a sound it does not list steps to its first */
static uint8_t sk_next_type(uint32_t world, uint8_t cur, int d)
{
    uint32_t n = SK_WN[world & 7u], k, at = n;
    for (k = 0; k < n; k++)
        if (SK_WL[world & 7u][k] == cur)
            at = k;
    if (at == n)
        return SK_WL[world & 7u][0];
    return SK_WL[world & 7u][(at + n * 8u + (uint32_t)d) % n];
}
