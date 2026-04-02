# Outsider MVP Plan

## Implementation Status

Status snapshot: 2026-04-02.

The project has moved past pure planning and into a working localhost prototype:

- standalone X11/Cairo server application under `outsider/`
- LV2 stereo client under `lv2/outsider-client/`
- live authority selection, endpoint/session tracking, and `Semaphore` editing
- server-side `P-Mix` / `E-Mix` models with client-side gain and fade application
- server-to-client command timing aligned to local bar/step boundaries in the client
- minimal X11 UIs on both server and client

Important current limitation:

- the live transport is currently raw localhost TCP with JSON lines, not WebSocket framing yet

For the current build state and next recommended implementation steps, see `docs/outsider-next-steps.md`.

## Goal

Build the first useful version of Outsider as a localhost control-plane system:

- one standalone desktop server application
- one LV2 stereo audio client plugin
- one shared server-side feature tab called `Semaphore`
- no server-side audio return path in phase 1

The point of the MVP is to prove that an external shared arrangement engine is worth having before attempting general routing or networked DSP.

## Recommended Scope Reduction

The original idea is broader than the first version should be. For the MVP, the following simplifications are recommended:

1. Control only, not media streaming
- The client sends transport snapshots and lightweight status only.
- The server sends decisions only.
- Audio remains local to the LV2 client.

2. Stereo audio client only
- No MIDI streaming in phase 1.
- No arbitrary routing graph in phase 1.

3. Localhost only
- Assume the server and DAW run on the same Linux machine.
- Do not design for authentication, WAN use, or multi-user sessions yet.

4. Numeric endpoint identity in the plugin
- Use integer `session_slot` and `endpoint_slot` controls in the first LV2 client.
- Keep human-readable naming on the server side.
- Avoid string editing and patch-property complexity inside the first plugin version.

5. Block-accurate timing, not sample-accurate network timing
- Commands should target a musical boundary.
- The client applies them on the next matching local block boundary.

## Concrete MVP

### What the server does

- Accept client connections over WebSockets.
- Track sessions and endpoints.
- Select one transport authority endpoint per session.
- Run `Semaphore` logic using `p-mix` and `e-mix` style models.
- Send state-change commands to each client.
- Display current endpoint state in an X11/Cairo UI.

### What the client does

- Report transport state and local status to the server on a worker thread.
- Receive commands from the server on a worker thread.
- Apply mute/gain/fade locally in the audio callback.
- Fail open if the server disconnects.

### What the MVP explicitly does not do

- send audio to the server for processing
- receive processed audio back from the server
- transport MIDI blocks
- host arbitrary DSP modules inside the server

## Recommended Repository Layout

```text
flues/
├── outsider/
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── outsider/
│   │       ├── protocol.hpp
│   │       ├── transport_snapshot.hpp
│   │       ├── command_packet.hpp
│   │       └── semaphore_models.hpp
│   └── src/
│       ├── app/
│       │   ├── main.cpp
│       │   ├── outsider_app.cpp
│       │   └── outsider_app.hpp
│       ├── net/
│       │   ├── ws_server.cpp
│       │   └── ws_server.hpp
│       ├── session/
│       │   ├── session_registry.cpp
│       │   └── session_registry.hpp
│       ├── transport/
│       │   ├── transport_authority.cpp
│       │   └── transport_authority.hpp
│       ├── semaphore/
│       │   ├── semaphore_engine.cpp
│       │   ├── p_mix_model.cpp
│       │   └── e_mix_model.cpp
│       └── ui/
│           ├── outsider_ui_x11.cpp
│           └── outsider_ui_x11.hpp
├── lv2/
│   └── outsider-client/
│       ├── CMakeLists.txt
│       ├── outsider-client.lv2/
│       │   ├── manifest.ttl
│       │   └── outsider-client.ttl
│       └── src/
│           ├── outsider_client_plugin.cpp
│           ├── outsider_client_state.hpp
│           ├── outsider_client_net.cpp
│           ├── outsider_client_net.hpp
│           ├── spsc_queue.hpp
│           └── ui/
│               └── outsider_client_ui_x11.c
└── docs/
    ├── outsider-idea.md
    ├── outsider-plan.md
    ├── outsider-protocol.md
    └── outsider-next-steps.md
```

## Core Design Decisions

### 1. Reuse the existing plugin algorithms as pure models

Do not port the whole `p-mix` and `e-mix` LV2 plugins into the server.

Instead, extract just their decision logic into pure server-side models:

- `PMixModel`
- `EMixModel`

These should have no LV2 dependency and no audio buffer dependency. They should consume normalized transport state and parameter structs, then emit a high-level decision:

- target audible or muted
- transition type
- transition duration
- next reevaluation point

### 2. Keep gain ramps in the client

The server decides:

- what should happen
- when it should happen
- how long the transition should take

The client performs:

- envelope ramping
- mute/unmute state changes
- pass-through audio output

This keeps the audio callback local and deterministic.

### 3. One transport authority per session

Only one endpoint should be allowed to drive transport for a given session.

MVP rule:

- explicit `authority` toggle on the client
- if multiple clients claim authority, the server keeps the first valid one and flags the conflict in the UI

## LV2 Client Specification

### Plugin type

- stereo audio effect
- audio in L/R
- audio out L/R
- atom input for `time:Position`
- X11/Cairo status UI
- LV2 State persistence for numeric config

### Recommended control ports

- `Enable` toggle
- `Session Slot` integer
- `Endpoint Slot` integer
- `Authority` toggle
- `Reconnect` toggle
- `Fallback Gain` float

### Recommended output/status ports

- `Connected`
- `Server Seen`
- `Current Gain`
- `Current State`
- `Current Mode`
- `Authority Active`

### Client persistence

Persist:

- session slot
- endpoint slot
- authority flag
- reconnect policy
- fallback gain

Do not attempt user-editable strings in the first version.

### Client audio-thread behavior

The audio thread must:

- parse host `time:Position`
- write compact `TransportSnapshot` records into an outbound SPSC queue
- read the newest available `CommandPacket` from an inbound SPSC queue
- apply gain/mute/fade locally
- produce pass-through audio when disconnected

The audio thread must not:

- perform socket I/O
- allocate memory
- parse JSON
- lock mutexes

## Server Application Specification

### UI layout

The first server UI should have four visible regions:

1. Header bar
- server running state
- connected endpoint count
- selected session
- current authority endpoint

2. Transport panel
- playing/stopped
- bar/beat
- BPM
- drift warnings

3. Endpoint list
- endpoint slot
- online/offline
- authority
- current mode
- current gain/state

4. `Semaphore` editor
- selected endpoint or selected group
- mode selector: `Bypass`, `P-Mix`, `E-Mix`
- parameter editor for the active mode
- next transition preview

### Session state model

Each endpoint row on the server should maintain:

- `session_slot`
- `endpoint_slot`
- `connected`
- `authority_claimed`
- `last_heartbeat_ms`
- `last_transport`
- `current_mode`
- `current_state`
- `current_gain`
- `last_command_id`
- `algorithm_params`

## Protocol

Use JSON messages over one WebSocket connection per client for the MVP.

Binary frames can be added later for the media plane.

The authoritative wire specification now lives in `docs/outsider-protocol.md`.

## Shared Internal Types

These should exist as C++ structs first and only then be serialized to JSON:

### `TransportSnapshot`

```cpp
struct TransportSnapshot {
    uint16_t session_slot;
    uint16_t endpoint_slot;
    bool authority;
    bool playing;
    uint64_t block_counter;
    double bar;
    double beat;
    double beats_per_bar;
    double bpm;
    double sample_rate;
    uint32_t block_size;
};
```

### `CommandPacket`

```cpp
enum class OutsiderMode : uint8_t { Bypass, PMix, EMix };
enum class TargetState : uint8_t { Play, Mute, FadeIn, FadeOut };

struct CommandPacket {
    uint64_t command_id;
    OutsiderMode mode;
    TargetState target_state;
    float target_gain;
    float duration_beats;
    uint32_t apply_at_bar;
    uint8_t apply_at_step16;
};
```

## Semaphore Module

### Supported modes

- `Bypass`
- `P-Mix`
- `E-Mix`

### `P-Mix` parameters

Directly mirror the current LV2 plugin behavior:

- `granularity_bars`
- `maintain_weight`
- `fade_weight`
- `cut_weight`
- `fade_dur_max_fraction`
- `bias_percent`
- `seed`

### `E-Mix` parameters

Directly mirror the current LV2 plugin behavior:

- `total_bars`
- `division`
- `steps`
- `offset`
- `fade_bars`

### Decision output

For each endpoint, `Semaphore` should emit:

- mode
- target state
- target gain
- duration in beats
- next apply boundary

### Important implementation note

For `E-Mix`, the server should not stream every sample or every gain value. It should emit only boundary commands:

- enter active block
- leave active block
- fade duration

The client constructs the actual gain ramp.

## Build Sequence

### Phase 0: Shared model extraction

- Create pure C++ `PMixModel` and `EMixModel`.
- Verify they reproduce the same decisions as the current LV2 implementations for identical transport/parameter inputs.
- Keep them independent of UI and network code.

### Phase 1: Server skeleton

- Create `outsider/` desktop app target.
- Implement X11/Cairo shell and a simple endpoint table.
- Add a WebSocket server with connect/disconnect logging.
- Add in-memory session registry.

### Phase 2: Client skeleton

- Create `lv2/outsider-client/`.
- Implement stereo pass-through LV2 plugin.
- Add time parsing from `time:Position`.
- Add state persistence.
- Add client X11 status UI.

### Phase 3: Real-time-safe queues and network thread

- Add bounded SPSC queues:
  - audio thread -> network thread
  - network thread -> audio thread
- Implement JSON encode/decode on the network thread only.
- Add reconnect and heartbeat behavior.

### Phase 4: Loopback command harness

Before connecting to the real server, add a local synthetic command generator in the client or a tiny test harness process.

This step is recommended because it proves:

- the audio-thread command application works
- fades behave correctly
- fail-open behavior works
- status ports/UI reflect command state correctly

### Phase 5: End-to-end connection

- Connect the real server and real LV2 client.
- Display connected endpoints.
- Display transport authority state.
- Round-trip `hello`, `transport`, `status`, `welcome`, and `command`.

### Phase 6: Semaphore mode

- Add `Bypass`, `P-Mix`, and `E-Mix` editing in the server UI.
- Generate authoritative commands from the server models.
- Show next transition preview.

### Phase 7: Host validation

- Test with multiple tracks in Reaper.
- Confirm transport sync across insert instances.
- Confirm disconnect safety.
- Confirm the workflow is actually easier than manual automation.

## Validation Checklist

### Client checks

- With no server running, the plugin passes audio through unchanged.
- When the server connects, state changes occur at the intended block boundary.
- When the server disconnects mid-session, the plugin returns to pass-through safely.
- Command ids prevent stale command reapplication.

### Server checks

- Connected endpoints appear and disappear correctly.
- Only one authority endpoint is active.
- Endpoint state display updates in real time.
- `P-Mix` and `E-Mix` decisions remain stable across transport start/stop.

### Musical checks

- `P-Mix` behavior feels the same as the existing plugin, just centrally coordinated.
- `E-Mix` boundaries are musically correct.
- Grouping several tracks under one `Semaphore` session creates useful arrangement behavior.

## Risks And Mitigations

### Risk: WebSocket library choice becomes a build problem

Mitigation:

- hide the transport behind a small `INetworkTransport` interface
- keep protocol structs separate from network code
- allow replacement with Unix sockets later if needed

### Risk: Transport drift between client instances

Mitigation:

- one authority endpoint per session
- only use others for status and validation

### Risk: LV2 plugin networking breaks real-time safety

Mitigation:

- worker thread only for sockets and JSON
- bounded queues only
- no locks on the audio thread

### Risk: The architecture is useful in theory but awkward in practice

Mitigation:

- keep the first feature narrow
- measure usefulness with `Semaphore`
- do not build the media plane until this is clearly worth it

## Recommended Immediate Next Step

The protocol is now specified in `docs/outsider-protocol.md`.

The next concrete action after this plan should be:

1. scaffold `outsider/` as a standalone server target
2. scaffold `lv2/outsider-client/` as a stereo pass-through LV2 plugin
3. add the shared internal types and loopback command harness before real networking

That sequence keeps the client real-time behavior testable before WebSocket details are added.
