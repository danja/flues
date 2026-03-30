# Shifty Plugin Requirements

## Summary

Shifty is an LV2 audio effect that applies transport-synchronised pitch shifts to incoming audio. It follows the timing model used by `lv2/p-mix`: the host transport determines bar-relative position, and that position selects the currently active pitch-shift segment.

The effect is intended as a pattern-driven shifter rather than a continuously automated pitch effect. Users define a repeating block in bars, divide that block into equal segments, and assign a semitone value to each segment.

## Core Behaviour

- Processes stereo audio input and output.
- Uses host `time:Position` when available.
- Repeats on a bar-relative block timeline.
- Each block contains a configurable number of equal divisions.
- Each division has an integer semitone shift value.
- The active semitone shift is determined by the current transport position within the block.
- If host time is unavailable or transport is stopped, the effect should fall back to clean pass-through.

## Timing Model

- `block_bars` defines the repeating block length in bars.
- `division_count` defines how many equal segments the block is split into.
- The block is measured in musical bars, not seconds or samples.
- The active division is derived from:
  - absolute host bar position
  - `block_bars`
  - `division_count`

Recommended first-version ranges:

- `block_bars`: `1..8`
- `division_count`: `1..16`

## Pitch Controls

- There should be a fixed maximum number of semitone controls exposed as LV2 ports.
- The UI should show editable text boxes for the active subset determined by `division_count`.
- Hidden divisions should default to `0` semitones and be ignored by DSP when outside the active count.

Recommended first-version limits:

- `16` division semitone ports
- semitone range `-24 .. +24`
- integer values only

## DSP Behaviour

The first version should prefer a simple, robust, real-time implementation over a high-end spectral one.

Recommended implementation path:

- stereo circular delay buffer per channel
- resampling-based pitch shift using a playback-rate ratio
- ratio derived from semitones: `2^(semitones / 12)`
- smoothing or crossfading when the active division changes

Important requirements:

- abrupt shift changes should not click badly at division boundaries
- both stereo channels should use the same active shift value
- transport rewind or stop should reset any internal phase cleanly

## First-Version DSP Scope

Required:

- transport-synchronised segment selection
- pitch ratio change based on active semitone value
- basic smoothing at shift transitions
- stereo support
- safe bypass/pass-through behaviour when timing is invalid
- a pragmatic first-pass real-time pitch shifter, without promising high-end quality

Not required in v1:

- high-quality phase vocoder or granular time-stretch preservation
- formant preservation
- tempo-independent free-running mode
- per-channel independent shift patterns

## Controls

### Required Controls

- `block_bars`
  - integer
  - repeating block size in bars
- `division_count`
  - integer
  - number of active divisions in the block
- `shift_1` .. `shift_16`
  - integer semitone values
  - range `-24 .. +24`

### Recommended Additional Controls

- `mix`
  - wet percentage control
  - useful because strong pitch shifting may be more effective blended than fully wet
  - recommended range `0..100`
  - recommended default `100`
- `smooth_ms`
  - transition smoothing time for shift changes

## UI Requirements

The UI should follow the established Flues raw X11/Cairo pattern.

Suggested layout:

- `Global`
  - block bars
  - division count
  - optional mix and smoothing controls
- `Divisions`
  - row or grid of editable text boxes
  - only show active boxes up to `division_count`
- `Status`
  - current active division
  - current semitone shift

Useful UI behaviour:

- active division should be visually highlighted during playback
- empty text box input should resolve to `0`
- invalid input should clamp to the valid semitone range

## Persistence

The first version should rely on standard LV2 control-port persistence.

This is sufficient because:

- the division shifts are numeric control values
- the UI text boxes are just an editing surface for those numeric ports
- there is no need to persist separate text payloads

## Transport Rules

- playback position should be derived from host bar-relative timing
- the same host bar position should always select the same division
- if transport stops, the DSP should reset any transient shift state cleanly
- if transport rewinds or jumps backward, the active division and smoothing state should be recomputed from the new position

## Implementation Notes

The most relevant existing references in this repository are:

- `lv2/p-mix`
  - for transport-synchronised audio effect scheduling
- `lv2/e-mix`
  - for multi-channel audio effect structure and host time handling
- `lv2/bassgen`
  - for compact X11/Cairo UI patterns, dropdown/text-oriented interaction, and status presentation

Recommended initial structure:

- transport/time reader
- active-division selector
- pitch-shift engine
- LV2 wrapper
- X11/Cairo UI

## First Version Scope

The first version should include:

- stereo audio in/out
- host-synchronised block/division timing
- `block_bars`, `division_count`, and `16` semitone controls
- editable division text boxes
- active-division UI highlight
- compile-safe, transport-safe effect behaviour

The first version does not need:

- advanced pitch quality guarantees
- arbitrary number of divisions beyond the fixed maximum
- preset system
- modulation beyond the transport-selected shifts
