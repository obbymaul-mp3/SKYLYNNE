# SKYLYNNE

Ambient firmware for the M-VAVE FM-1. Six voices play themselves on one tempo and scale. Four macro knobs
(DENSITY, MOTION, COLOUR, SPACE) shape them, and each voice can be edited: type, level, pan, sustain, tone.
Long drones, thin whistles, a deep sub and a very large reverb.

The presets are grouped in worlds, and each world has its own sounds and its own picture:

| World | Presets | Sounds (ALGORITHM walks this list) |
| --- | --- | --- |
| City at night | 1–8 | drone, whistle, sub, pad, air |
| The cathedral | 9–12 | choir, brass, organ, bell, forge, stomp, drone, pad, sub, whistle |
| The deep | 13–16 | whale, drip, glass, bell, pad, air, sub, drone, whistle, choir |

The struck sounds (bell, forge, stomp, drip) are very sparse by design.

A second page is a physics sequencer: a polygon spins, balls fall inside it, and every wall hit plays a note on
one of the voices.

The screen is a figure on the left looking over water, from a cliff, a terrace of old stone or a fallen column. The scene follows the preset you pick.

Try it in the browser before installing anything: build with `tools/build_web.sh` and open `build/web/play/`
(or use the published page).

## Controls

| Control | Does |
| --- | --- |
| SELECT | pick the voice |
| ALGORITHM | voice type |
| PRESETS | biome (on the physics page: number of sides) |
| K1–K4 | DENSITY, MOTION, COLOUR, SPACE (on a voice page: LEVEL, PAN, DECAY, MATERIAL) |
| OCT− / OCT+ | tempo |
| PLAY | hold |
| keys 1–6 | mute voices; other keys play the selected voice |
| MASTER | output volume |

## Build

- `./build.sh [--release X.Y]` builds the device package (needs the JieLi toolchain; CI does this).
- `tools/build_web.sh` builds the browser version (clang with wasm32 and lld).
- `tools/host/sk_host.c` is a script-driven host harness for screenshots and audio renders.

## Update mode

Hold OCT− at power-on, or inside SKYLYNNE hold OCT− + OCT+ + HOME for 5 seconds.

## Licence

GPL-3.0-only. See `LICENSING.md` for credits (Felucca and ChoralRoot for the hardware layer, Sora and Departure Mono
fonts, the JieLi SDK files).
