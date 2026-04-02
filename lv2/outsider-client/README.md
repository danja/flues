# Outsider Client

`outsider-client` is the LV2 side of the Outsider prototype. At this stage it is a stereo effect plugin with:

- persisted `session_slot` and `endpoint_slot` identity
- a minimal X11/Cairo status UI
- a stub network layer
- a local loopback harness for exercising command timing without a real server

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
4. Choose a `Demo Mode`.
5. Start host transport.
6. Watch `Current Mode`, `Current State`, and `Current Gain` change while audio is gated locally.

`Connected` and `Server Seen` remain off in this scaffold because there is no live WebSocket transport yet.

## Notes

- `Authority` and `Reconnect` are persisted but currently only feed the stub network layer.
- `session_slot` and `endpoint_slot` are intended to match the control-plane IDs described in `docs/outsider-protocol.md`.
- The next major step is replacing the loopback harness with real protocol I/O while keeping the same runtime command application path.
