# LV2 Plugins Overview

This repository contains a broad set of LV2 instruments, MIDI tools, and audio effects under `lv2/`. The plugins are designed to work together in DAWs and hosts such as Reaper, Ardour, Carla, and Jalv.

## ants
Ants is a cellular-automata MIDI generator driven by a swarm moving on a 16x16 scale grid. It uses pheromone-style behavior and randomness to create evolving melodic and rhythmic output.

## arpiso
ArpIso is a Euclidean gravity arpeggiator built around the Launchpad Mini MK3. Held pads act as wells that drive playheads and generate quantized note output synced to host transport.

## bubbles
Bubbles is a water-inspired LV2 synth focused on flowing, bubbling, and dripping textures. It is useful for ambient sound design and organic percussive layers.

## chatgen
ChatGen converts typed English text into MIDI note and CC streams for speech articulation control. It is designed primarily to drive Chatterbox for text-to-speech style workflows.

## chatterbox
Chatterbox is a formant-based speech synthesizer with larynx/noise sources and F1-F4 shaping. It includes vocal modes and dynamics controls for expressive synthetic voice design.

## chordant
Chordant is a transport-synced Euclidean capture/mix effect. It records and re-triggers captured segments with timing, fade, and behavior controls.

## disyn
Disyn is a distortion-synthesis instrument based on multiple algorithmic oscillator models. It emphasizes rich harmonic and inharmonic spectra with compact parameter sets.

## drumkit
Drumkit is an industrial drum synthesizer with multiple synthesized voices and master FX. It targets aggressive electronic percussion without relying on samples.

## e-mix
E-Mix is a Euclidean audio mixer effect that alternates active and silent blocks over a bar cycle. It supports fades for less abrupt transitions in rhythmic gating patterns.

## euclid
Euclid is a multi-lane Euclidean MIDI rhythm generator mapped for Drumkit voices. Each lane has beats, offset, length, and randomness with global timing controls.

## euclid-mono
EuclidMono is a two-pattern Euclidean generator for a single MIDI note. It combines Pattern A and B through logic operators with optional inversion per pattern.

## euclidean-gate
Euclidean Gate applies Euclidean rhythm gating to incoming audio. It supports gate and mute modes with envelope shaping for tempo-locked chopping.

## eudelay
EuDelay is a delay-focused LV2 effect in the Flues plugin set. It is intended for rhythmic and spatial timing treatments inside the same host workflows as the other Flues effects.

## floozy
Floozy is a hybrid instrument combining Disyn-style source content with PM-Synth style resonant processing. It delivers aggressive tone generation inside a physical-modeling signal path.

## floozy-dev
Floozy Dev is a development variant of the Floozy hybrid concept. It is used for iterating on the hybrid DSP path and feature behavior.

## floozy-poly
Floozy Poly extends the Floozy architecture to polyphonic voice handling. It is aimed at chordal and layered hybrid textures in performance contexts.

## flues-control
Flues Control is a MIDI control utility tailored for controlling `flues-synth`. It provides program/CC-oriented control mapping with a dedicated panel workflow.

## grid-seq
grid-seq is a grid-based MIDI step sequencer with Launchpad Mini MK3 integration. It supports full-range MIDI sequencing with visual interaction on hardware and UI.

## memone
Memone is a predictive audio effect that applies lightweight online sequence modeling. It learns from incoming signal over a warmup period and outputs predicted audio behavior.

## metalv
MetaLV is an LV2 host plugin that can load and route other LV2 plugins in multiple slots. It also exposes MCP control interfaces for external agent-driven orchestration.

## midi-flip
MIDI Flip mirrors note events around a user-defined pivot note. It is a compact utility for inversion-like transformations in melodic material.

## p-mix
P-Mix is a probabilistic mixer that toggles channels on/off at bar boundaries. It is designed for dropout, cycling, and stochastic arrangement effects.

## padseq
PadSeq is a 64-step drum sequencer optimized for Launchpad Mini MK3 workflows. It combines step programming, Euclidean tools, and pattern handling in one instrument.

## pm-synth
PM-Synth is the LV2 port of the Stove physical-modeling engine. It brings interface strategies, delays, filtering, modulation, and reverb into a monophonic instrument.

## quadrangle
Quadrangle is a Launchpad performance instrument with multiple functional zones. It combines sequencing and live play interactions for electronic performance setups.

## quantico
Quantico is a MIDI scale quantizer that snaps notes to a selected key/scale. It is useful for harmonically constraining improvised or generated MIDI input.

## slimmer
Slimmer is a monophonic MIDI filter that reduces polyphonic input to one selected note. It includes multiple selection strategies and timing controls for stable mono lines.

## speculate
Speculate is a spectral modulation effect based on short-time FFT processing. It provides shift, blur, freeze, and dry/wet control for evolving timbral transformations.
