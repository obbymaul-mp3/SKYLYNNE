/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SKYLYNNE contributors; the HAL: Felucca by Leo Kuroshita, Hügelton Instruments */
/* SKYLYNNE: one compilation unit (the HAL is header-only). Six voices that play themselves on one tempo and scale, four
 * macro knobs, a physics sequencer, and a cliff to watch it from.
 *
 *   sk_dsp.c     the voices and the conductor        sk_fx.c      the room: reverb, delay, grains, master
 *   sk_biomes.c  the factory soundscapes             sk_fb.c      the screen (framebuffer, palette, text)
 *   sk_scene.c   the cliff                           sk_phys.c    the physics sequencer
 *   sk_ui.c      the knobs and pages                 sk_hw.c      the FM-1: boot, audio, panel
 *
 * Host check: tools/host/sk_host.c builds the same sources into a program for a PC. The device build is tools/build.py. */

#ifndef FELUCCA_ID
#define FELUCCA_ID "FM-1_930"            /* package identity (build.py: the .fwsc marker string) */
#endif
#ifndef SK_VERSION
#define SK_VERSION "SKYLYNNE"
#endif
#define SK_POOL __attribute__((section(".pool")))

#include <stdint.h>
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#include "fm1_input.h"
#include "fm1_timer.h"
#include "fm1_audio.h"
#include "fm1_adc.h"
#include "fm1_lcd_hw.h"
#include "sk_bootguard.h"

#include "libc.c"
#include "lcd.c"

#include "sk_dsp.c"
#include "sk_fx.c"
#include "sk_biomes.c"
#include "sk_fb.c"
#include "sk_scene.c"
#include "sk_phys.c"
#include "sk_ui.c"
#include "sk_hw.c"
