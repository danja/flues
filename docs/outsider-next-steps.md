# Outsider Next Steps

## Progress Snapshot

Status snapshot: 2026-04-03.

### Completed so far

- server scaffold built as a standalone X11/Cairo desktop app
- LV2 stereo client scaffold built with persisted numeric identity/state
- lock-free client queues and worker-thread network path in place
- localhost live control link working with `hello` / `welcome` / `transport` / `status` / `command`
- authority selection and endpoint/session tracking working
- server-side `P-Mix` and `E-Mix` models wired into the `Semaphore` panel
- client applies server commands locally with fail-open pass-through on disconnect
- command timing now respects local bar / step boundaries instead of applying immediately on packet receipt
- server `params` updates are sent to the client so the client UI can show current server config
- localhost WebSocket upgrade and framed text-message transport now replace the earlier raw TCP line transport
- server-side session persistence now saves and restores endpoint mode/params across restart
- session-scoped grouping is now in place with group assignment, follow-group behavior, group-level mode/params, and persistence
- installer script exists at repo root: `install-outsider.sh`

### Current reality check

- the prototype is good enough to try musically, but it is still a work in progress rather than a settled protocol or product
- UI polish is still incremental; the server and client views are functional status panels, not finished interfaces

### Recommended next implementation steps

1. Harden connection handling with explicit stale/offline timeouts and clearer UI status for reconnect and authority conflicts.
2. Add a small local smoke test harness or integration test path outside the audio host so command scheduling and reconnect logic can be regression-tested.
3. Add more `Semaphore` parameter editing breadth in the server UI beyond the current granularity/bias and steps/offset controls.
4. Decide how far browser-facing compatibility should go in the MVP: plain browser observer/editor clients only, or browser peers that can also act as transport-aware endpoints.
5. Decide whether the group model should stop at flat `G1`-`G4` assignment or grow into named scenes/groups later.

## Immediate Sequence

With the idea, MVP plan, and protocol now written down, the sensible implementation order is:

1. Keep the current live server/client path stable while host testing continues.
2. Harden timeout/reconnect/conflict reporting.
3. Add a repeatable smoke-test path outside the DAW.
4. Broaden the `Semaphore` editor controls.
5. Revisit browser-facing editor/client scope after the control path feels stable.

## Why This Order

- It proves real-time-safe command application before networking complicates debugging.
- It keeps the server and client loosely coupled until the protocol is settled.
- It avoids dragging media streaming into the MVP too early.

## First Concrete Deliverables

### Server scaffold

Completed.

- `outsider/CMakeLists.txt`
- X11/Cairo application shell
- live endpoint table
- live transport panel
- interactive `Semaphore` panel

### Client scaffold

Completed.

- `lv2/outsider-client/CMakeLists.txt`
- `outsider-client.ttl`
- stereo pass-through DSP plus server-driven gain/fade application
- state persistence
- minimal X11 status UI with live server status/config display

### Shared internal types

Completed.

- `TransportSnapshot`
- `CommandPacket`
- `OutsiderMode`
- `TargetState`

## Stop Conditions

Pause and reassess before going further if:

- the client cannot remain strictly real-time safe
- block-boundary command application is audibly too sloppy
- the server/client workflow is not noticeably better than existing DAW automation
