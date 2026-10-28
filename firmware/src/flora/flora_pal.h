/* CoralFlower palette: ten four-step ramps after the "TLP" 31-colour pixel palette, on black. (The background was a
 * warm near-black, 0x110A02: in RGB565 it is (16, 8, 0), no blue at all, and the FM-1's panel showed it olive.)
 *
 * Every pixel the flora engine and the UI draw is an index into this table; the platform turns the
 * indices into RGB565 once per frame (fl_pal_rgb). A ramp runs from light (step 0) to dark (step 3).
 */
#ifndef FLORA_PAL_H
#define FLORA_PAL_H

#include <stdint.h>

enum {
    FL_BG = 0,      /* the screen background, also "transparent" for sprites */
    FL_BG1 = 1,     /* panels */
    FL_BG2 = 2,     /* raised panels, inactive outlines */
    FL_BG3 = 3,     /* dividers */
    FL_INK = 4,     /* primary text */
    FL_INK2 = 5,    /* secondary text */
    FL_INK3 = 6,    /* disabled text */
    FL_BLACK = 7,   /* pure shadow */
    FL_RAMP0 = 8    /* first ramp entry */
};

enum {
    FL_R_FIRE = 0,   /* yellow -> orange -> red -> maroon */
    FL_R_PEACH,      /* peach -> terracotta -> brown */
    FL_R_COPPER,     /* peach -> copper -> rust -> brown */
    FL_R_LIME,       /* lime -> leaf green -> forest -> deep */
    FL_R_JADE,       /* lime -> jade -> teal -> deep teal */
    FL_R_SEA,        /* aqua -> sky -> cobalt -> indigo */
    FL_R_ORCHID,     /* blush -> pink -> magenta -> plum */
    FL_R_ROSE,       /* blush -> rose -> red -> wine */
    FL_R_SAND,       /* bone -> sand -> umber -> dark umber */
    FL_R_SAGE,       /* mist -> sage -> slate -> night */
    FL_NRAMPS
};

#define FL_PAL_SIZE (FL_RAMP0 + FL_NRAMPS * 4)

/* palette index of ramp r, step s (0 light .. 3 dark) */
#define FL_C(r, s) ((uint8_t)(FL_RAMP0 + (r) * 4 + (s)))

/* 0xRRGGBB */
static const uint32_t fl_pal_rgb[FL_PAL_SIZE] = {
    0x000000, 0x1A120B, 0x271B12, 0x3A2B22, 0xF1ECE0, 0xA89A88, 0x5E5048, 0x000000,
    0xF9C62A, 0xF1750D, 0xB1301B, 0x6C0E26,   /* FIRE */
    0xF6BA7A, 0xDE6E50, 0x8D3E32, 0x46201E,   /* PEACH */
    0xF6BA7A, 0xCA7535, 0x893E36, 0x46201E,   /* COPPER */
    0xD6E34C, 0x5BB42F, 0x186C44, 0x0E393C,   /* LIME */
    0xD3E34D, 0x22B572, 0x1F6C6B, 0x0D383F,   /* JADE */
    0x48E6D9, 0x0D97D5, 0x1B4D9F, 0x231F6F,   /* SEA */
    0xFAB2A5, 0xE6658B, 0x962179, 0x47115E,   /* ORCHID */
    0xF7B1AC, 0xE86879, 0xB12D24, 0x680C26,   /* ROSE */
    0xEEEEE3, 0xAEA292, 0x6D5E59, 0x3D2F2F,   /* SAND */
    0xE3F0EC, 0x82ABA2, 0x466A74, 0x2A354A,   /* SAGE */
};

/* 3IS: the ice scene. Palette entries 52..55 (the ice: outline, white, pale, mid; C_SEC is C_ACC now) and 56..63: the
 * night sky, dark to the glow at the horizon (FL_L*), then the wizard's hat, its light, its shade and the crystal's
 * cyan (FL_S*; the coral's water is gone). ice_sprites.h is drawn with these indices (tools/gen_ice_sprites.py). */
enum { FL_I_OUT = 52, FL_I_ICE, FL_I_ICE2, FL_I_ICE3 };
enum { FL_L0 = 56, FL_L1, FL_L2, FL_L3, FL_S0, FL_S1, FL_S2, FL_S3 };
static const uint32_t fl_ice_rgb[4] = {0x1C2B66, 0xE6F6FF, 0xB4DEFA, 0x7FB8EA};
static const uint32_t fl_back_rgb[8] = {
    0x0A1030, 0x172A66, 0x2A4F9A, 0x5282C6,   /* night sky -> the glow */
    0x3A4FB8, 0x6B86EE, 0x25358A, 0x7FE3FF,   /* hat, hat light, hat shade, crystal */
};

/* the wizard's hat and crystal by the chord's root (circle of fifths round the colour wheel) */
static const uint32_t fl_hat_rgb[12][4] = {
    {0x3A4FB8, 0x6B86EE, 0x25358A, 0x7FE3FF}, /* 0: the original blue */
    {0x9CB73A, 0xD4F26D, 0x6F8422, 0xE8FF99}, /* 1 */
    {0x943AB7, 0xCC6DF2, 0x692284, 0xE299FF}, /* 2 */
    {0x3AB756, 0x6DF28A, 0x228438, 0x99FFAF}, /* 3 */
    {0xB73A5D, 0xF26D92, 0x84223D, 0xFF99B5}, /* 4 */
    {0x3A9CB7, 0x6DD4F2, 0x226F84, 0x99E8FF}, /* 5 */
    {0xB7943A, 0xF2CC6D, 0x846922, 0xFFE299}, /* 6 */
    {0x563AB7, 0x8A6DF2, 0x382284, 0xAF99FF}, /* 7 */
    {0x5DB73A, 0x92F26D, 0x3D8422, 0xB5FF99}, /* 8 */
    {0xB73A9C, 0xF26DD4, 0x84226F, 0xFF99E8}, /* 9 */
    {0x3AB794, 0x6DF2CC, 0x228469, 0x99FFE2}, /* 10 */
    {0xB7563A, 0xF28A6D, 0x843822, 0xFFAF99}, /* 11 */
};

#endif
