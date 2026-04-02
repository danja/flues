# Outsider Next Steps

## Immediate Sequence

With the idea, MVP plan, and protocol now written down, the sensible implementation order is:

1. Scaffold `outsider/` as a standalone desktop server target.
2. Scaffold `lv2/outsider-client/` as a stereo pass-through LV2 effect.
3. Add `TransportSnapshot`, `CommandPacket`, and bounded SPSC queues on the client side.
4. Add a loopback test harness before real networking.
5. Add real WebSocket connection and `hello` / `welcome` / `transport` / `status` / `command`.
6. Port `p-mix` and `e-mix` decision logic into pure server-side models.
7. Build the `Semaphore` tab around those models.

## Why This Order

- It proves real-time-safe command application before networking complicates debugging.
- It keeps the server and client loosely coupled until the protocol is settled.
- It avoids dragging media streaming into the MVP too early.

## First Concrete Deliverables

### Server scaffold

- `outsider/CMakeLists.txt`
- X11/Cairo application shell
- empty endpoint table
- empty transport panel

### Client scaffold

- `lv2/outsider-client/CMakeLists.txt`
- `outsider-client.ttl`
- stereo pass-through DSP
- state persistence
- minimal X11 status UI

### Shared internal types

- `TransportSnapshot`
- `CommandPacket`
- `OutsiderMode`
- `TargetState`

## Stop Conditions

Pause and reassess before going further if:

- the client cannot remain strictly real-time safe
- block-boundary command application is audibly too sloppy
- the server/client workflow is not noticeably better than existing DAW automation
