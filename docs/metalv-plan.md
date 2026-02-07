# MetaLV Implementation Plan (4 Slots)

MetaLV is an LV2 plugin that hosts other LV2 plugins and exposes an MCP server over stdio for agent control.

## Scope (v1)
- 4 parallel slots, each can load a plugin URI
- Stereo audio in/out + MIDI in/out
- Slot bypass + gain
- Parameter bank mapping per slot (fixed number of exposed params)
- MCP server over stdio (line-delimited JSON)
- X11/Cairo UI showing slot status + mapped params

## Architecture
- `HostEngine` module wraps lilv world discovery and per-slot plugin instances.
- Each slot maintains plugin handle, port map, audio/MIDI buffers, bypass/gain.
- Host graph: all slots receive input, outputs summed to master output.

## Checklist

### 1) Metadata + Ports
- [ ] Create `lv2/metalv/metalv.lv2/manifest.ttl`
- [ ] Create `lv2/metalv/metalv.lv2/metalv.ttl`
- [ ] Define ports: stereo audio in/out, atom MIDI in/out
- [ ] Define per-slot ports: `slot_X_bypass`, `slot_X_gain`, `slot_X_param_bank[0..N]`
- [ ] Define atom control port for `slot_X_load` (string URI)
- [ ] State persistence for slot URIs + param mappings

### 2) Host Engine (lilv)
- [ ] Add `lv2/metalv/src/host/HostEngine.*`
- [ ] Initialize lilv world + enumerate plugins
- [ ] Slot load/unload by URI
- [ ] Connect hosted plugin ports to internal buffers
- [ ] Run hosted instances per audio block

### 3) MCP Server (stdio)
- [ ] Add `lv2/metalv/src/mcp/MCPServer.*`
- [ ] Line-delimited JSON requests/responses on stdio
- [ ] Commands: list_plugins, load_slot, list_slot_ports, map_param, set_param, get_param, slot_state
- [ ] Thread-safe access to HostEngine

### 4) LV2 Plugin Wrapper
- [ ] `lv2/metalv/src/metalv_plugin.cpp`
- [ ] Wire audio/MIDI ports to HostEngine
- [ ] Expose param bank ports to slots
- [ ] Manage state save/restore

### 5) UI
- [ ] `lv2/metalv/src/ui/metalv_ui_x11.c`
- [ ] Slot list with plugin URI/name
- [ ] Bypass/gain controls
- [ ] Mapped param labels + values

### 6) Build + Install
- [ ] `lv2/metalv/CMakeLists.txt` linking lilv + LV2 + X11/Cairo
- [ ] `metalv-install.sh`
- [ ] `lv2/metalv/README.md`

## Defaults
- Slot count: 4
- Param bank size: 8 (per slot)
- Routing: parallel summing
