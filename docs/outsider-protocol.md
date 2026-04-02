# Outsider MVP Protocol

## Purpose

This document defines the first concrete wire protocol for Outsider MVP.

It is deliberately limited to the control plane:

- client registration
- transport reporting
- status reporting
- server-issued mode and state commands
- heartbeat and reconnect behavior

It does not define media streaming. Audio and MIDI block transport are explicitly out of scope for protocol version `1`.

## Scope

Protocol version `1` assumes:

- localhost only
- one WebSocket connection per LV2 client instance
- JSON text frames only
- one server process
- one authority client per session

## Connection

### Recommended endpoint

The recommended default server endpoint for the MVP is:

```text
ws://127.0.0.1:7342/outsider/v1
```

This is a recommendation, not a hard requirement, but using one default makes early testing and scripting much simpler.

### Transport

- WebSocket over TCP
- UTF-8 JSON text frames only
- no binary frames in protocol version `1`

## General Rules

### Versioning

Every handshake message must include:

- `protocol_version`

For the MVP:

- `protocol_version = 1`

If the server does not support the client version, it must reject the session with an `error` message and close the connection.

### Message envelope

Every JSON message must include:

- `type`

Most messages should also include:

- `session_slot`
- `endpoint_slot`

### Identity model

The MVP uses numeric identity only:

- `session_slot`: positive integer identifying a session
- `endpoint_slot`: positive integer identifying one client within a session

Human-readable endpoint labels remain a server-side concern in version `1`.

### Idempotency

The server may resend the latest authoritative command at any time.

The client must therefore treat `command` messages as idempotent and apply only the newest `command_id`.

### Unknown fields

Both client and server should ignore unknown fields in version `1`.

This makes the protocol forward-tolerant for incremental additions.

## Message Types

### Client -> Server

### `hello`

Sent immediately after WebSocket connection establishment.

Purpose:

- declare protocol version
- identify the session and endpoint
- declare authority intent
- declare client capabilities

Required fields:

- `type`
- `protocol_version`
- `session_slot`
- `endpoint_slot`
- `authority`
- `capabilities`

Example:

```json
{
  "type": "hello",
  "protocol_version": 1,
  "session_slot": 1,
  "endpoint_slot": 3,
  "authority": true,
  "capabilities": {
    "audio_stereo": true,
    "midi": false,
    "control_only": true
  }
}
```

Field notes:

- `authority`: client requests to act as transport authority for the session
- `capabilities.audio_stereo`: true for the MVP client
- `capabilities.midi`: false in MVP
- `capabilities.control_only`: true in MVP

Validation:

- `session_slot >= 1`
- `endpoint_slot >= 1`

### `transport`

Sent regularly from the client worker thread whenever fresh host transport data is available.

Purpose:

- provide authoritative or observational timing
- allow server-side algorithm scheduling

Required fields:

- `type`
- `session_slot`
- `endpoint_slot`
- `playing`
- `bar`
- `beat`
- `beats_per_bar`
- `bpm`
- `sample_rate`
- `block_size`
- `block_counter`

Example:

```json
{
  "type": "transport",
  "session_slot": 1,
  "endpoint_slot": 3,
  "playing": true,
  "bar": 12,
  "beat": 2.5,
  "beats_per_bar": 4.0,
  "bpm": 126.0,
  "sample_rate": 48000.0,
  "block_size": 256,
  "block_counter": 182044
}
```

Field semantics:

- `bar`: zero-based or one-based must be chosen consistently by implementation; the MVP recommendation is zero-based internally and one-based in UI only
- `beat`: beat position within the current bar, fractional allowed
- `block_counter`: strictly monotonic per endpoint while connected

Validation:

- `beats_per_bar > 0`
- `bpm > 0`
- `sample_rate > 0`
- `block_size >= 1`
- `block_counter` must never move backward for a given connection

Rate guidance:

- send once per processed audio block if cheap enough
- otherwise coalesce and send at least often enough for musical boundary accuracy

### `status`

Sent periodically and after significant local state changes.

Purpose:

- let the server display endpoint state
- confirm command application progress
- expose simple meter information

Required fields:

- `type`
- `session_slot`
- `endpoint_slot`
- `connected_to_server`
- `current_gain`
- `current_state`
- `last_command_id`
- `peak_l`
- `peak_r`

Example:

```json
{
  "type": "status",
  "session_slot": 1,
  "endpoint_slot": 3,
  "connected_to_server": true,
  "current_gain": 0.74,
  "current_state": "fade-in",
  "last_command_id": 98,
  "peak_l": 0.62,
  "peak_r": 0.58
}
```

Allowed `current_state` values:

- `play`
- `mute`
- `fade-in`
- `fade-out`
- `bypass`

Validation:

- `0.0 <= current_gain <= 1.0`
- `0.0 <= peak_l`
- `0.0 <= peak_r`

### `heartbeat`

Sent when no `transport` or `status` message has been sent recently.

Purpose:

- keep the connection marked alive
- allow heartbeat-based timeout handling

Required fields:

- `type`
- `session_slot`
- `endpoint_slot`

Optional fields:

- `last_command_id`

Example:

```json
{
  "type": "heartbeat",
  "session_slot": 1,
  "endpoint_slot": 3,
  "last_command_id": 98
}
```

Recommended interval:

- every 500 ms to 1000 ms when otherwise idle

### `goodbye`

Optional courtesy message sent before clean disconnect.

Purpose:

- let the server mark the endpoint offline immediately

Example:

```json
{
  "type": "goodbye",
  "session_slot": 1,
  "endpoint_slot": 3
}
```

### Server -> Client

### `welcome`

Sent in response to a valid `hello`.

Purpose:

- accept the endpoint
- confirm negotiated session identity
- report authority acceptance or refusal

Required fields:

- `type`
- `protocol_version`
- `server_name`
- `session_slot`
- `endpoint_slot`
- `authority_accepted`

Optional fields:

- `server_time_ms`
- `heartbeat_interval_ms`

Example:

```json
{
  "type": "welcome",
  "protocol_version": 1,
  "server_name": "Outsider",
  "session_slot": 1,
  "endpoint_slot": 3,
  "authority_accepted": true,
  "heartbeat_interval_ms": 750
}
```

### `command`

The main control message in the MVP.

Purpose:

- instruct the client to adopt a mode and target state at a musical boundary

Required fields:

- `type`
- `command_id`
- `session_slot`
- `endpoint_slot`
- `mode`
- `target_state`
- `target_gain`
- `duration_beats`
- `apply_at_bar`
- `apply_at_step16`

Example:

```json
{
  "type": "command",
  "command_id": 99,
  "session_slot": 1,
  "endpoint_slot": 3,
  "mode": "p_mix",
  "target_state": "fade-out",
  "target_gain": 0.0,
  "duration_beats": 1.0,
  "apply_at_bar": 13,
  "apply_at_step16": 0
}
```

Allowed `mode` values:

- `bypass`
- `p_mix`
- `e_mix`

Allowed `target_state` values:

- `play`
- `mute`
- `fade-in`
- `fade-out`

Client rules:

- ignore any `command` whose `command_id` is lower than the newest already applied
- if `command_id` matches the newest applied command, treat it as a harmless resend
- if `apply_at_bar` is already in the past when received, apply on the next valid local boundary immediately

Validation:

- `target_gain` should be clamped to `0.0 .. 1.0`
- `duration_beats >= 0.0`
- `apply_at_step16` must be `0 .. 15`

### `params`

Sent when the server changes persistent endpoint algorithm parameters.

Purpose:

- keep the client UI and status view aligned with server mode configuration
- optionally allow a client to show the currently active algorithm config

Required fields:

- `type`
- `session_slot`
- `endpoint_slot`
- `mode`
- `params`

Example:

```json
{
  "type": "params",
  "session_slot": 1,
  "endpoint_slot": 3,
  "mode": "e_mix",
  "params": {
    "total_bars": 128,
    "division": 16,
    "steps": 8,
    "offset": 0,
    "fade_bars": 0
  }
}
```

MVP note:

The client does not need to use `params` for DSP in version `1`. It may simply display them.

### `heartbeat`

Sent when the server has nothing else to send but wants to keep the connection warm and detectable.

Example:

```json
{
  "type": "heartbeat",
  "session_slot": 1,
  "server_time_ms": 452981
}
```

### `error`

Sent when the server rejects a message or cannot continue the session cleanly.

Required fields:

- `type`
- `code`
- `message`

Optional fields:

- `session_slot`
- `endpoint_slot`
- `fatal`

Example:

```json
{
  "type": "error",
  "code": "protocol_version_unsupported",
  "message": "Protocol version 1 required",
  "fatal": true
}
```

Recommended `code` values:

- `protocol_version_unsupported`
- `invalid_identity`
- `duplicate_endpoint`
- `authority_conflict`
- `bad_message`
- `internal_error`

## Authority Semantics

The server must maintain at most one active transport authority per `session_slot`.

Rules:

1. The first connected endpoint with `authority=true` and valid transport becomes authority.
2. Later authority claimants in the same session are refused.
3. Refusal does not disconnect the client; it simply receives `authority_accepted=false` in `welcome` or a later authority-status update if such a message is added.
4. If the current authority disconnects or times out, the server may elect a new authority from remaining clients that requested authority.

MVP simplification:

- no dynamic authority transfer UI is required yet

## Timing Semantics

### Musical boundary

Commands are scheduled to a musical boundary, not an absolute sample index.

The MVP uses:

- `apply_at_bar`
- `apply_at_step16`

where `apply_at_step16` is the sixteenth-note slot inside the bar.

### Client interpretation

The client should:

1. compare incoming command timing against its local host transport
2. queue the command internally
3. apply the command on the next block that crosses the requested boundary

This means the protocol is block-accurate, not sample-accurate.

### Late commands

If a command arrives after its intended boundary:

- apply it at the next local boundary immediately
- do not discard it only because of lateness

### Missing transport

If the client lacks valid host transport:

- it must not invent timing for server commands
- it should remain in fail-open pass-through mode
- it should continue to report status so the server UI shows the problem

## Reconnect Behavior

### Client

On disconnect, the client should:

- switch to fail-open behavior
- preserve latest persistent settings
- attempt reconnect if `Reconnect` is enabled

On reconnect, the client must:

- send a fresh `hello`
- resume `transport` and `status` reporting
- accept a resent authoritative `command`

### Server

On reconnect, the server should:

- treat the endpoint as the same logical endpoint if `session_slot` and `endpoint_slot` match
- replace stale connection state with the new connection
- resend the latest authoritative mode/command state

## Timeout Rules

Recommended server timeout policy:

- endpoint becomes `stale` after 1500 ms without `transport`, `status`, or `heartbeat`
- endpoint becomes `offline` after 4000 ms without activity

Recommended client timeout policy:

- server becomes `missing` after 2000 ms without any server message
- client then reverts to fail-open mode

These numbers are not musically perfect, but they are practical and easy to reason about for the MVP.

## Validation Rules Summary

### Common

- reject malformed JSON
- reject messages missing `type`
- ignore unknown extra fields

### Identity

- `session_slot >= 1`
- `endpoint_slot >= 1`

### Transport

- `bpm > 0`
- `beats_per_bar > 0`
- `sample_rate > 0`
- `block_size >= 1`

### Command

- `command_id` monotonic per endpoint
- `target_gain` clamped to `0.0 .. 1.0`
- `duration_beats >= 0.0`
- `apply_at_step16` in `0 .. 15`

## Example Flow

### 1. Connect

Client opens WebSocket and sends:

- `hello`

Server replies:

- `welcome`

### 2. Establish authority and transport

Client sends:

- `transport`
- `status`

Server marks the session and authority endpoint.

### 3. Server schedules a transition

Server sends:

- `params`
- `command`

Client stores the newest `command_id` and applies it at the next matching musical boundary.

### 4. Client reports application state

Client sends:

- `status`

showing:

- current gain
- current state
- `last_command_id`

### 5. Disconnect

If the server disappears:

- client enters fail-open mode
- client continues local pass-through audio
- client attempts reconnect if configured

## Future Extensions

The following are intentionally deferred beyond protocol version `1`:

- binary audio block uplink/downlink
- MIDI block transport
- session/group editing from the client
- named endpoints
- authentication
- cross-machine discovery
- absolute sample-time scheduling

## Relationship To Other Docs

- [outsider-idea.md](/home/danny/github/flues/docs/outsider-idea.md) explains why Outsider exists.
- [outsider-plan.md](/home/danny/github/flues/docs/outsider-plan.md) explains what the MVP should contain.
- This document defines how the MVP server and client talk to each other.
