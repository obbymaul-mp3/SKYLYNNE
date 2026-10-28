# SKYLYNNE

A self-playing firmware for the M-VAVE FM-1: six resonator voices that play themselves on one shared tempo and
scale, a granular bus that reshapes them, and a night-sky city skyline that lights up with the music.

**Status: just started.** This repository begins as a copy of the 3IS firmware tree (CoralFlower, with ChoralRoot,
Felucca and fm1-x0x underneath), so that the proven hardware layer (boot, screen, audio, keys, flash, second core,
the update loader) is already here. The instrument on top of it is being replaced step by step with a new one.
The old chord instrument is still what this builds until the new engine takes over.

## Plan

1. A new skyline screen and one self-playing resonator voice.
2. Six voices, materials, shared tempo and scale, generative "biomes".
3. The granular bus, with a send per voice, tempo sync, fifth/octave layers and freeze.
4. Feedback from the grains into the resonators; presets; four live macros.
5. Polish and load testing on the real FM-1.

## Licence

GPL-3.0-only, as the hardware layer's authors require (see LICENSE, LICENSING.md and LICENSES/ for the credits and
the licences of the parts this stands on).
