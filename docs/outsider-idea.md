# Outsider

## Overview

Outsider is a client-server system for coordination that is awkward inside a DAW's normal per-track plugin model.

The underlying problem is not "audio processing" on its own. The real problem is that cross-track choreography, shared decision-making, and central routing logic are difficult when each plugin instance only sees its own track and the host offers limited inter-plugin communication.

The Outsider idea is to move that shared logic into a separate desktop application:

- The server is a Linux desktop X11 application.
- It hosts the global UI, routing logic, pattern engines, MIDI logic, and eventually DSP pipelines.
- LV2 client plugins sit on DAW tracks and act as endpoints.
- Clients forward transport state and, depending on mode, audio/MIDI blocks to the server.
- The server returns decisions, instructions, and later processed media.

## Key Design Decision

This needs to be treated as two layers, not one:

### 1. Control plane

This carries:

- transport position
- play/stop state
- plugin identity and session membership
- parameter changes
- routing commands
- gate/fade/mute decisions
- status and metering

This is the first useful version and should be enough for the initial proof of concept.

### 2. Media plane

This carries:

- audio blocks
- MIDI event blocks
- optional processed return audio/MIDI

This is much riskier because LV2 plugins run in a real-time audio context and network traffic must never block that thread. It should be designed from the start, but not made the first dependency for the project to be usable.

That means the first Outsider milestone should be musically useful even if the server is only sending control decisions back to the clients.

## Working Goal

Build a system where multiple LV2 instances can join the same Outsider session and be directed by one shared server-side engine, with safe failover and transport-synced behavior.

## First Version Scope

The first version should be intentionally narrow:

- One standalone X11 server app.
- One LV2 client plugin type.
- One server tab called `Semaphore`.
- `Semaphore` implements the decision logic currently found in `lv2/p-mix` and `lv2/e-mix`.
- The client remains responsible for applying gain, mute, and fade locally.
- The server remains responsible for deciding when those state changes happen.

This avoids round-tripping audio through WebSockets in phase 1 while still proving the central idea.

## Functional Requirements

### Session model

- Multiple client instances can connect to the same Outsider server.
- Each client belongs to a named session.
- Each client advertises a stable endpoint id, display name, and type.
- The server shows connected endpoints and their current state.

### Transport model

- The system must operate from DAW transport timing, not wall clock alone.
- At least one client in a session acts as transport authority.
- The server receives bar, beat, BPM, play/stop, and sample/block timing information.
- Other clients in the same session are assumed to be on the same host timeline, but the server should still validate gross drift.

### Client behavior

- The LV2 client must be real-time safe.
- No socket I/O, memory allocation, or blocking calls may happen on the audio thread.
- Communication with the server must happen on a worker thread using lock-free or bounded queues between DSP and network code.
- If the server disconnects or times out, the client must fail open:
  - audio effect mode: pass audio through unchanged
  - MIDI mode: pass MIDI through unchanged
  - control mode: ignore stale commands

### Server behavior

- The server must track sessions, endpoints, transport state, and current decisions.
- It must be able to compute track decisions from one shared pattern engine and distribute them to many clients.
- It must expose those decisions clearly in the UI.
- It must degrade safely if one client disappears.

### Control semantics

For the first version, the server must be able to send per-client:

- target state: `play`, `mute`, `fade-in`, `fade-out`
- transition duration
- effective gain
- algorithm mode selection
- pattern parameters

### Persistence

- Server-side sessions should be saveable and reloadable.
- Client-side LV2 instances should persist at least:
  - session name
  - endpoint name
  - role
  - reconnect policy

## Non-Goals For The First Version

The following should be explicitly deferred:

- sample-accurate server-side audio DSP returned live over WebSockets
- general arbitrary graph routing between clients
- plugin discovery/loading inside Outsider
- remote network collaboration across machines
- security/authentication beyond localhost assumptions

Those can come later, but they would make the proof of concept much harder if included from the start.

## Architecture Sketch

### Server

The server application should contain:

- X11 UI shell
- WebSocket server
- session manager
- transport manager
- algorithm tabs/modules
- command dispatcher
- optional audio/MIDI block processors for later phases

Logical modules:

1. `SessionRegistry`
- tracks clients, sessions, names, roles, and heartbeat status

2. `TransportAuthority`
- selects or confirms the active transport source
- exposes normalized timing to algorithm modules

3. `SemaphoreEngine`
- runs `p-mix` and `e-mix` style decisions centrally
- outputs per-endpoint state changes

4. `CommandRouter`
- sends commands to endpoints
- keeps a copy of last-known command state

5. `MediaRouter`
- future module for audio/MIDI block forwarding and DSP

### LV2 client

The first LV2 client should be an effect-style endpoint with:

- stereo input/output
- MIDI input/output optional, but not required for phase 1
- a local dry/wet or pass-through engine
- state ports for session name, endpoint name, role, and mode

Internal client threads:

1. audio thread
- reads incoming commands from a lock-free queue
- applies gain/mute/fade locally
- writes transport snapshots and meters into outbound queue

2. network thread
- owns the WebSocket connection
- serializes outbound messages
- receives server commands
- pushes compact command packets into inbound queue

## Proof Of Concept: Semaphore

`Semaphore` is the first concrete feature, not just a demo.

It should centralize the behavior of `p-mix` and `e-mix`:

- probabilistic on/off decisions from `p-mix`
- Euclidean on/off block scheduling from `e-mix`
- server UI decides which algorithm drives each endpoint
- endpoints apply the resulting state locally

### Semaphore requirements

- show all connected endpoints in a list
- allow endpoints to be grouped
- allow each endpoint to select:
  - `Bypass`
  - `P-Mix`
  - `E-Mix`
- show current state for each endpoint:
  - on
  - muted
  - fading
  - next transition time
- allow a global transport display
- allow parameter editing per endpoint or per group

### Minimal Semaphore workflow

1. User inserts Outsider client on several tracks.
2. Each client joins the same session.
3. Server UI shows the endpoints.
4. User assigns `P-Mix` or `E-Mix` mode to each endpoint.
5. DAW transport runs.
6. Server computes bar/step decisions.
7. Clients receive state changes and apply local gain/fade.

This is enough to prove that a shared external "arrangement brain" is useful.

## Protocol Sketch

One WebSocket connection per client is enough initially.

Use:

- JSON for control-plane messages in phase 1
- binary frames only when media streaming is added

### Client to server

`hello`
- endpoint id
- session id
- display name
- client version
- capabilities

`transport`
- playing
- bar
- beat
- beats per bar
- bpm
- sample rate
- block size
- monotonic block counter

`status`
- current local state
- last applied command id
- peak/RMS meters

`media-audio`
- future binary packet

`media-midi`
- future binary packet

### Server to client

`welcome`
- assigned session info
- reconnect hints

`command`
- command id
- target state
- transition type
- transition duration
- effective gain
- effective start bar/beat or block index

`parameters`
- algorithm selection
- parameter set for active mode

`heartbeat`
- keepalive and timing sanity checks

## Timing Requirements

The timing model must be explicit:

- decisions are musical, not sample-perfect in the first version
- server should schedule by future bar/step boundaries where possible
- clients should apply commands on the next valid local boundary
- commands should include enough timing information to survive network jitter

For phase 1, "correct on the next block at the intended musical boundary" is acceptable.

## Reliability Requirements

- Client and server must both tolerate reconnects.
- Missed commands should not leave a client stuck muted forever.
- The server should resend current authoritative state after reconnect.
- The client should apply only the newest command id and discard stale ones.
- If transport disappears, server algorithms should freeze or revert to safe defaults.

## UI Requirements

The server UI should not try to solve everything at once.

Phase 1 UI:

- connection/session panel
- transport display
- `Semaphore` tab
- per-endpoint rows with mode, parameters, and current state
- event log panel for debugging

Later UI:

- routing matrix
- MIDI monitor
- audio meters/waveforms
- scripting or instruction timeline

## Phase Plan

### Phase 1: Server skeleton

- X11 app shell
- WebSocket server
- session registry
- transport display
- dummy endpoint list

### Phase 2: LV2 control client

- stereo pass-through LV2 plugin
- worker-thread WebSocket client
- session/endpoint state persistence
- transport reporting
- safe disconnect behavior

### Phase 3: Semaphore engine

- port `p-mix` decision logic to server
- port `e-mix` Euclidean scheduling to server
- per-endpoint command generation
- server UI for parameters and live state

### Phase 4: Validation

- test with several tracks in Reaper
- verify transport sync, reconnect behavior, and fail-open behavior
- verify group control is useful enough to justify the architecture

### Phase 5: Media plane experiments

- add optional audio block uplink
- add optional MIDI block uplink
- measure latency and jitter
- decide whether WebSockets remain acceptable or whether Unix sockets/shared memory are needed for serious DSP

## Success Criteria For The Proof Of Concept

The proof of concept is successful if:

- multiple LV2 clients can join one session reliably
- the server can drive them from one shared `Semaphore` tab
- `p-mix` and `e-mix` style decisions happen in sync with host transport
- disconnecting the server does not break the DAW session
- the workflow feels meaningfully easier than doing the same arrangement manually inside the DAW

## Risks

### 1. WebSocket audio may be the wrong long-term transport

This is the biggest technical risk. It may be fine for control and coarse block streaming, but not for low-latency round-trip DSP.

Mitigation:
- make phase 1 control-first
- keep audio processing local in the client until proven necessary
- only promote server-side media processing after measurement

### 2. Transport authority ambiguity

If several clients report slightly different timing snapshots, the server can make inconsistent decisions.

Mitigation:
- choose one explicit authority client per session
- treat others as validation only

### 3. Real-time safety in the client

Any accidental blocking call in the plugin will make the whole idea unusable.

Mitigation:
- strict separation between DSP thread and network thread
- bounded queues only
- fail-open local behavior

## Open Questions

- Should the first client be audio-only, or audio plus MIDI from day one?
- Should one client be explicitly marked as session leader, or should the server auto-select the first transport-valid client?
- Should session persistence live on the server only, or should clients also cache the last session they joined?
- When media streaming arrives, is localhost-only an acceptable assumption, or is cross-machine use part of the goal?

## Recommended Next Step

Do not start with general routing.

Build `Outsider` first as:

- one X11 server app
- one pass-through LV2 client
- one `Semaphore` tab
- one JSON control protocol

If that works and feels useful in a real DAW session, then the broader routing and DSP ambitions become much easier to justify and design.

See also:

- `docs/outsider-plan.md` for the concrete MVP plan derived from this idea note.
- `docs/outsider-protocol.md` for the MVP wire protocol.
