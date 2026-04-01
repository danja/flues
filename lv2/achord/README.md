# Achord

Launchpad Mini MK3 chord-table LV2 MIDI instrument.

Achord turns the 8x8 Launchpad grid into a modernized accordion-style chord board:

- columns move in fifths
- rows select chord families
- top buttons handle global bank/register/trigger setup
- side buttons handle bass/add9/sus/inversion/spread/accent/voice-leading

## Build

```bash
cmake -S lv2/achord -B lv2/achord/build
cmake --build lv2/achord/build
cmake --install lv2/achord/build --prefix ~/.lv2
```

## Routing

1. Route Launchpad DAW output to `control_in`.
2. Route `launchpad_out` back to the Launchpad.
3. Route `midi_out` to a synth or sampler.
4. Start host transport if you want quantized or repeat triggering.

Direct mode works without host clock.
