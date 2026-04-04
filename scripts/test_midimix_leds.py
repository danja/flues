#!/usr/bin/env python3
"""Standalone Akai MIDImix LED tester using ALSA sequencer via aplaymidi.

This intentionally avoids third-party Python MIDI packages. It creates a tiny
format-0 MIDI file on the fly and plays it to an ALSA sequencer destination.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


MUTE_NOTES = [1, 4, 7, 10, 13, 16, 19, 22]
SOLO_MUTE_NOTES = [2, 5, 8, 11, 14, 17, 20, 23]
REC_ARM_NOTES = [3, 6, 9, 12, 15, 18, 21, 24]
BANK_NOTES = [25, 26, 27]


@dataclass
class MidiPort:
    port_id: str
    client_name: str
    port_name: str


def require_tool(name: str) -> str:
    path = shutil.which(name)
    if not path:
        raise SystemExit(f"Required tool not found: {name}")
    return path


def parse_ports() -> list[MidiPort]:
    require_tool("aplaymidi")
    try:
        result = subprocess.run(
            ["aplaymidi", "-l"],
            check=True,
            text=True,
            capture_output=True,
        )
    except subprocess.CalledProcessError as exc:
        stderr = (exc.stderr or "").strip()
        stdout = (exc.stdout or "").strip()
        detail = stderr or stdout or "Unknown ALSA sequencer error."
        raise SystemExit(f"`aplaymidi -l` failed: {detail}")

    ports: list[MidiPort] = []
    for line in result.stdout.splitlines():
        match = re.match(r"^\s*(\d+:\d+)\s+(.+?)\s{2,}(.+?)\s*$", line)
        if not match:
            continue
        ports.append(
            MidiPort(
                port_id=match.group(1),
                client_name=match.group(2).strip(),
                port_name=match.group(3).strip(),
            )
        )
    return ports


def choose_port(selector: str) -> MidiPort:
    if re.fullmatch(r"\d+:\d+", selector):
        return MidiPort(port_id=selector, client_name="manual", port_name="manual")

    ports = parse_ports()
    if not ports:
        raise SystemExit("No ALSA sequencer MIDI output ports found via `aplaymidi -l`.")

    selector_lower = selector.lower()
    matches = [
        port for port in ports
        if selector_lower in port.client_name.lower()
        or selector_lower in port.port_name.lower()
        or selector_lower in f"{port.client_name} {port.port_name}".lower()
    ]

    if not matches:
        raise SystemExit(f"No ALSA sequencer port matched '{selector}'.")
    if len(matches) > 1:
        descriptions = "\n".join(
            f"  {port.port_id}: {port.client_name} / {port.port_name}"
            for port in matches
        )
        raise SystemExit(
            f"Multiple ALSA sequencer ports matched '{selector}'. Be more specific:\n{descriptions}"
        )
    return matches[0]


def encode_varlen(value: int) -> bytes:
    if value < 0:
        raise ValueError("Variable-length MIDI values must be non-negative")
    buffer = value & 0x7F
    output = bytearray([buffer])
    value >>= 7
    while value:
        buffer = 0x80 | (value & 0x7F)
        output.insert(0, buffer)
        value >>= 7
    return bytes(output)


def build_midi_bytes(notes: list[int], channel: int, on_ms: int, off_ms: int, velocity: int, off_style: str) -> bytes:
    if not 0 <= channel <= 15:
        raise ValueError("channel must be 0-15")
    if not 1 <= velocity <= 127:
        raise ValueError("velocity must be 1-127")

    track = bytearray()

    # Tempo: 1,000,000 us/qn with division 1000 => 1 tick = 1 ms.
    track.extend(b"\x00\xFF\x51\x03\x0F\x42\x40")

    status_on = 0x90 | channel
    status_off = 0x80 | channel

    first = True
    for note in notes:
        if not 0 <= note <= 127:
            raise ValueError(f"note out of range: {note}")

        if first:
            track.extend(encode_varlen(0))
            first = False
        else:
            track.extend(encode_varlen(off_ms))
        track.extend(bytes([status_on, note, velocity]))

        track.extend(encode_varlen(on_ms))
        if off_style == "note-off":
            track.extend(bytes([status_off, note, 0]))
        else:
            track.extend(bytes([status_on, note, 0]))

    track.extend(b"\x00\xFF\x2F\x00")

    header = b"MThd" + (6).to_bytes(4, "big") + (0).to_bytes(2, "big") + (1).to_bytes(2, "big") + (1000).to_bytes(2, "big")
    chunk = b"MTrk" + len(track).to_bytes(4, "big") + bytes(track)
    return header + chunk


def write_temp_midi(notes: list[int], channel: int, on_ms: int, off_ms: int, velocity: int, off_style: str) -> Path:
    payload = build_midi_bytes(notes, channel, on_ms, off_ms, velocity, off_style)
    handle, path = tempfile.mkstemp(prefix="midimix-led-test-", suffix=".mid")
    with os.fdopen(handle, "wb") as f:
        f.write(payload)
    return Path(path)


def note_set(name: str) -> list[int]:
    key = name.lower()
    if key == "mute":
        return MUTE_NOTES[:]
    if key == "solo":
        return SOLO_MUTE_NOTES[:]
    if key == "rec":
        return REC_ARM_NOTES[:]
    if key == "bank":
        return BANK_NOTES[:]
    if key == "all":
        return MUTE_NOTES + SOLO_MUTE_NOTES + REC_ARM_NOTES + BANK_NOTES
    raise SystemExit(f"Unknown note set: {name}")


def parse_notes(raw: str) -> list[int]:
    notes: list[int] = []
    for part in raw.split(","):
        token = part.strip()
        if not token:
            continue
        if "-" in token:
            start_text, end_text = token.split("-", 1)
            start = int(start_text)
            end = int(end_text)
            step = 1 if end >= start else -1
            notes.extend(list(range(start, end + step, step)))
        else:
            notes.append(int(token))
    if not notes:
        raise SystemExit("No notes parsed from --notes.")
    return notes


def main() -> int:
    parser = argparse.ArgumentParser(description="Send LED test notes directly to an Akai MIDImix via ALSA sequencer.")
    parser.add_argument("--list", action="store_true", help="List ALSA sequencer output ports and exit.")
    parser.add_argument("--port", help="ALSA sequencer port id (e.g. 24:0) or a case-insensitive name substring.")
    parser.add_argument("--set", default="all", choices=["all", "mute", "solo", "rec", "bank"], help="Built-in MIDImix note set to test.")
    parser.add_argument("--notes", help="Explicit note list/ranges, e.g. '1-27' or '1,4,7,10'. Overrides --set.")
    parser.add_argument("--channel", type=int, default=0, help="Zero-based MIDI channel. Default: 0 (MIDI channel 1).")
    parser.add_argument("--velocity", type=int, default=127, help="Note-on velocity. Default: 127.")
    parser.add_argument("--on-ms", type=int, default=180, help="LED on duration per note in milliseconds. Default: 180.")
    parser.add_argument("--off-ms", type=int, default=70, help="Gap after each note in milliseconds. Default: 70.")
    parser.add_argument("--off-style", default="note-off", choices=["note-off", "zero-velocity"], help="How to turn LEDs off.")
    parser.add_argument("--dry-run", action="store_true", help="Print the chosen settings without sending MIDI.")
    args = parser.parse_args()

    if args.list:
        ports = parse_ports()
        if not ports:
            print("No ALSA sequencer MIDI output ports found.")
            return 1
        for port in ports:
            print(f"{port.port_id}\t{port.client_name}\t{port.port_name}")
        return 0

    if not args.port:
        parser.error("--port is required unless --list is used")

    port = choose_port(args.port)
    notes = parse_notes(args.notes) if args.notes else note_set(args.set)

    print(f"Target port: {port.port_id} ({port.client_name} / {port.port_name})")
    print(f"Notes: {notes}")
    print(f"Channel: {args.channel + 1}")
    print(f"On/Off: {args.on_ms}ms / {args.off_ms}ms")
    print(f"Off style: {args.off_style}")

    if args.dry_run:
        return 0

    midi_file = write_temp_midi(
        notes=notes,
        channel=args.channel,
        on_ms=args.on_ms,
        off_ms=args.off_ms,
        velocity=args.velocity,
        off_style=args.off_style,
    )

    try:
        subprocess.run(
            ["aplaymidi", "-p", port.port_id, str(midi_file)],
            check=True,
        )
    finally:
        try:
            midi_file.unlink()
        except FileNotFoundError:
            pass

    return 0


if __name__ == "__main__":
    sys.exit(main())
