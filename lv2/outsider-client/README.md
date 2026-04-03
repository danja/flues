# Outsider Client

`outsider-client` is the LV2 side of the Outsider prototype. At this stage it is a stereo effect plugin with:

- persisted `session_slot` and `endpoint_slot` identity
- `endpoint_slot = 0` means auto-assign a unique runtime endpoint ID for that plugin instance
- a minimal X11/Cairo status UI
- a localhost control client that exchanges JSON-line messages with the Outsider server
- a local loopback harness for exercising command timing when no server is available

## Build

```bash
cd lv2/outsider-client
cmake -S . -B build
cmake --build build
cmake --install build --prefix ~/.lv2
```

## Current Demo Modes

Set `Demo Mode` from the host or generic UI:

- `Off`: plain pass-through using `Fallback Gain`
- `Pulse`: alternates fade-in and fade-out on each bar
- `P-Mix Demo`: issues deterministic 2-bar play/mute/fade decisions derived from `endpoint_slot`
- `E-Mix Demo`: issues 5-in-8 Euclidean fades on 8th-note boundaries

The demo engine requires valid host transport/time. If the host is stopped, the client returns to pass-through.

## How To Exercise It

1. Insert the plugin on a stereo track with incoming audio.
2. Open the plugin UI or generic controls.
3. Set `Enable` on.
4. Leave `Endpoint Slot` at `0` for auto identity, or set a manual slot if you want a stable explicit endpoint number.
5. Choose a `Demo Mode`.
5. Start host transport.
6. Watch `Current Mode`, `Current State`, and `Current Gain` change while audio is gated locally.

With the server running, `Connected` and `Server Seen` should turn on. Without the server, Demo Mode still works as an offline harness.

## Notes

- `Authority` and `Reconnect` now affect the live localhost control connection.
- `session_slot` and `endpoint_slot` are intended to match the control-plane IDs described in `docs/outsider-protocol.md`.
- The current implementation uses localhost WebSocket transport with JSON control messages in text frames.
