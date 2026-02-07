# MetaLV

MetaLV is an LV2 plugin that hosts other LV2 plugins (4 slots) and exposes an MCP server for agent control. It supports stdio MCP and a TCP MCP backend (default `127.0.0.1:5566`) so it can be used in DAWs like Reaper.

## Quick Start (Reaper)

1. Insert MetaLV in your track.
2. Connect your MCP client to `127.0.0.1:5566`.
3. Load plugins and map parameters via MCP.

If you need to change or disable TCP MCP:

```sh
export METALV_MCP_TCP=5566   # change port
export METALV_MCP_TCP=0      # disable TCP (stdio only)
```

## Slots + Routing

- 4 parallel slots.
- Each slot receives the main input and can output audio/MIDI.
- Outputs are summed to the main outputs.
- Per-slot bypass and gain.

## MCP Commands (line-delimited JSON)

Each request is a single JSON line. Responses are single-line JSON.

Commands:
- `{"cmd":"list_plugins"}`
- `{"cmd":"load_slot","slot":0,"uri":"..."}`
- `{"cmd":"list_slot_ports","slot":0}`
- `{"cmd":"map_param","slot":0,"bank":0,"port":3}`
- `{"cmd":"set_param","slot":0,"bank":0,"value":0.5}`
- `{"cmd":"slot_state","slot":0}`

### Example: Load a Plugin in Slot 1

```sh
printf '{"cmd":"list_plugins"}\n' | nc 127.0.0.1 5566
printf '{"cmd":"load_slot","slot":0,"uri":"https://danja.github.io/flues/plugins/p-mix"}\n' | nc 127.0.0.1 5566
printf '{"cmd":"list_slot_ports","slot":0}\n' | nc 127.0.0.1 5566
```

### Example: Map + Set a Parameter

```sh
printf '{"cmd":"map_param","slot":0,"bank":0,"port":17}\n' | nc 127.0.0.1 5566
printf '{"cmd":"set_param","slot":0,"bank":0,"value":0.5}\n' | nc 127.0.0.1 5566
```

## Using with Codex or Claude (MCP)

MetaLV speaks MCP over stdio and TCP.

### Option A: stdio via a lightweight LV2 host

```sh
jalv.nogui https://danja.github.io/flues/plugins/metalv
```

Then configure your AI client to use that process stdin/stdout.

### Option B: TCP (recommended for Reaper)

MetaLV listens on `127.0.0.1:5566` by default. Configure your AI client to connect to that TCP port and send line-delimited JSON.

## Build & Install

From the repo root:

```sh
cmake -S lv2/metalv -B lv2/metalv/build
cmake --build lv2/metalv/build
cmake --install lv2/metalv/build --prefix ~/.lv2
```

Or:

```sh
./metalv-install.sh
```

Dependencies:
- LV2 headers
- lilv (and deps)
- X11 + Cairo (UI)
- CMake + a C/C++ toolchain

## Development Notes

- DSP/host: `lv2/metalv/src/metalv_plugin.cpp`
- Host engine: `lv2/metalv/src/host/HostEngine.*`
- MCP server: `lv2/metalv/src/mcp/MCPServer.*`
- UI: `lv2/metalv/src/ui/metalv_ui_x11.c`
- Metadata: `lv2/metalv/metalv.lv2/*.ttl`
