# LV2 Plugins Overview

This repository contains a broad set of LV2 instruments, MIDI tools, and audio effects with source code under `lv2/`. The plugins are designed to work together in DAWs and hosts such as Reaper, Ardour, Carla, and Jalv.

**The plugins are all experimental, most work to some degree, none could be called complete...**

Right now they have to be built before installation, see instructions below. Standard Linux tooling and libs are used. So far, mostly only tested with Reaper on Ubuntu Studio.

They (should) all have my name in their metadata, so searching for the keyword "Ayers" should select them.

The code has been developed with much help from AI assistants (Claude Code & Codex). 

* [MIDI Generation/Processing](#midi)
* [Audio Synthesis](#audio-synthesis)
* [Audio Effects](#audio-effects)
* [Launchpad-specific](#launchpad-specific)
* [Misc](#misc)
* [Installation](#installation)

## MIDI

### euclid
Euclid is a multi-lane Euclidean MIDI rhythm generator mapped for drumkit voices. Each lane has beats, offset, length, and randomness with global timing controls.

*This is good for knocking together a quick rhythm section. Each instrument has an **R** button that will randomise parameters. The MIDI values are set to those of [drumkit](#drumkit) below, on MIDI channel 10, though do work after a fashion with other kits.*

### euclid-mono
EuclidMono is a two-pattern Euclidean generator for a single MIDI note. It combines Pattern A and B through logic operators with optional inversion per pattern.

*Same idea as `euclid` above but for a single instrument, with more versatile rhythm generation. The instrument may be selected, eg. MIDI note 36 is kick drum.*

### bassgen
BassGen is a monophonic MIDI bassline generator that builds phrases from root, scale, genre, density, and timing controls. It follows host transport, restarts bar-relative, offers a compact phrase preview in the UI, and persists the generated pattern exactly through LV2 state.

*This is aimed at the gap between a utility sequencer and a phrase generator. The current implementation now includes broader harmonic support and more characterful style presets, including `Funk` and `Sabbath` biases for distinctly different bass behavior.*

### midi-flip
MIDI Flip mirrors note events around a user-defined pivot note. It is a compact utility for inversion-like transformations in melodic material.

*This is a lot of fun, especially when used with `quantico` below. If you download an existing `.mid` song file, then send the MIDI through these, you get a whole new (possibly very discordant) tune.*

### quantico
Quantico is a MIDI scale quantizer that snaps notes to a selected key/scale. It is useful for harmonically constraining improvised or generated MIDI input.

*Handy as a utility on homemade or preeisting sequences.*

### ants
Ants is a cellular-automata MIDI generator driven by a swarm moving on a 16x16 scale grid. It uses pheromone-style behavior and randomness to create evolving melodic and rhythmic output.

*It kind-of works, but the controls need revisiting, it's far from intuitive.*

### slimmer
Slimmer is a monophonic MIDI filter that reduces polyphonic input to one selected note. It includes multiple selection strategies and timing controls for stable mono lines.

*Handy utility, eg. for pulling a bassline out of MIDI chords.*

### chatgen
ChatGen converts typed English text into MIDI note and CC streams for speech articulation control. It is designed primarily to drive Chatterbox for text-to-speech style workflows.

*Doesn't work as intended. Needs revisiting.*

### flues-control
Flues Control is a MIDI control utility tailored for controlling `flues-synth`. It provides program/CC-oriented control mapping with a dedicated panel workflow. Not much use on its own.

## Audio Synthesis

### drumkit
Drumkit is an industrial-inclined drum synthesizer with multiple synthesized voices and master FX. It targets aggressive electronic percussion without relying on samples.

*Fun sounds, some like 1970s organ drum machines.*

### bubbles
Bubbles is a water-inspired LV2 synth focused on flowing, bubbling, and dripping textures. It is useful for ambient sound design and organic percussive layers.

*It works, but is a little limited/disappointing.*

### floozy-poly
Floozy Poly extends the Floozy architecture to polyphonic voice handling. It is aimed at chordal and layered hybrid textures in performance contexts.

*Kind-of usable, best of this bunch. Unusual sounds.*

### floozy
Floozy is a hybrid instrument combining Disyn-style source content with PM-Synth style resonant processing. It delivers aggressive tone generation inside a physical-modeling signal path.

### floozy-dev
Floozy Dev is a development variant of the Floozy hybrid concept. It is used for iterating on the hybrid DSP path and feature behavior.

### pm-synth
PM-Synth is the LV2 port of the Stove physical-modeling engine. It brings interface strategies, delays, filtering, modulation, and reverb into a monophonic instrument.

### disyn
Disyn is a distortion-synthesis instrument based on multiple algorithmic oscillator models. It emphasizes rich harmonic and inharmonic spectra with compact parameter sets.

### chatterbox
Chatterbox is a formant-based speech synthesizer with larynx/noise sources and F1-F4 shaping. It includes vocal modes and dynamics controls for expressive synthetic voice design.

*Hopeless user interface, another to revisit.*

## Audio Effects

### p-mix
P-Mix is a probabilistic mixer that toggles channels on/off at bar boundaries. It is designed for dropout, cycling, and stochastic arrangement effects.

*The controls aren't very intuitive, but this basically works. I find it very useful for automating mixdowns.*

### e-mix
E-Mix is a Euclidean audio mixer effect that alternates active and silent blocks over a bar cycle. It supports fades for less abrupt transitions in rhythmic gating patterns.

*Very like `p-mix`, I use the two together.*

### eudelay
EuDelay is a delay-focused LV2 effect in the Flues plugin set. It is intended for rhythmic and spatial timing treatments inside the same host workflows as the other Flues effects.

*A little disappointing.*

### shifty
Shifty is a transport-synchronised pitch-shift pattern effect. It divides a repeating bar block into editable semitone slots, highlights the active division in the UI, and applies a first-pass real-time pitch shift to the audio according to host position.

*The current implementation is deliberately pragmatic rather than hi-fi. The transport model and UI are in place, and the next useful step is improving pitch quality and boundary crossfades.*

### chordant
Chordant is a transport-synced Euclidean capture/mix effect. It records and re-triggers captured segments with timing, fade, and behavior controls.

*I can't remember what this was intended to do...*

### euclidean-gate
Euclidean Gate applies Euclidean rhythm gating to incoming audio. It supports gate and mute modes with envelope shaping for tempo-locked chopping.

*Handy for messing things up.*

### speculate
Speculate is a spectral modulation effect based on short-time FFT processing. It provides shift, blur, freeze, and dry/wet control for evolving timbral transformations.

*Can't remember if this worked or not...*

### memone
Memone is a predictive audio effect that applies lightweight online sequence modeling. It learns from incoming signal over a warmup period and outputs predicted audio behavior.

*Useless.*

## Launchpad-specific

These are designed for use with an Akai Launchpad Mini Mk3.

### arpiso
ArpIso is a Euclidean gravity arpeggiator built around the Launchpad Mini MK3. Held pads act as wells that drive playheads and generate quantized note output synced to host transport.

*This is great fun.*

### padseq
PadSeq is a 64-step drum sequencer optimized for Launchpad Mini MK3 workflows. It combines step programming, Euclidean tools, and pattern handling in one instrument.

*Rather basic, but works a treat.*

### grid-seq
grid-seq is a grid-based MIDI step sequencer with Launchpad Mini MK3 integration. It supports full-range MIDI sequencing with visual interaction on hardware and UI.

*Kind-of works, really needs revisiting.*

### quadrangle
Quadrangle is a Launchpad performance instrument with multiple functional zones. It combines sequencing and live play interactions for electronic performance setups.

*Early experiment.*

## Misc

### metalv
MetaLV is an LV2 host plugin that can load and route other LV2 plugins in multiple slots. It also exposes MCP control interfaces for external agent-driven orchestration.

*Needs a lot more work...*

## Installation

### Prerequisites
1. Update your package lists and install the base toolchain: `sudo apt update && sudo apt install build-essential cmake meson ninja-build pkg-config git`. These provide the compiler, build systems, and helper utilities used by every plugin.
2. Clone the repository so you have the source you’ll build: `git clone https://github.com/danja/flues.git` and then `cd flues`. You only need to do this once unless you want a fresh copy.
3. Install the LV2 and audio libraries: `sudo apt install lv2-dev libx11-dev libcairo2-dev libasound2-dev`. Add `libpulse-dev libgtk-4-dev` if you plan to build the GTK-based front-ends or host tools described elsewhere in the repo.
4. If you are on a non-Debian/Ubuntu system, use the equivalent packages for your distro (for example `dnf install` or `pacman -S` with the same package names). Confirm each command succeeds before continuing.

### Building the plugins
1. From the repo root (`/home/danny/github/flues`), pick the LV2 directory you want to build (for example `lv2/disyn`).
2. Run CMake to configure, build, and install the plugin: `cmake -S lv2/<plugin> -B lv2/<plugin>/build`, `cmake --build lv2/<plugin>/build`, `cmake --install lv2/<plugin>/build --prefix "$HOME/.lv2"`. Each plugin installs into `~/.lv2`, which is scanned by hosts such as Ardour, Reaper, and Carla.
3. After the install step, verify the plugin is registered by running `lv2ls | grep flues`.

### Helper scripts
`./install-plugins.sh` (repo root) builds and installs the core LV2 instruments: `disyn`, `floozy`, `chatterbox`, `chatgen`, `drumkit`, `euclid`, `pm-synth`, and `flues-control`. Run it after installing the dependencies above to build everything in one go.

Numerous `install-*.sh` helpers exist that invoke the build pipeline for their target plugin and install into `~/.lv2`. Examples include `install-arpiso.sh`, `install-bassgen.sh`, `install-bubbles.sh`, `install-chordant.sh`, `install-disyn.sh`, `install-e-mix.sh`, `install-euclid.sh`, `install-euclid-mono.sh`, `install-euclidean-gate.sh`, `install-eudelay.sh`, `install-grid-seq.sh`, `install-memone.sh`, `install-padseq.sh`, `install-p-mix.sh`, `install-q.sh`, `install-shifty.sh`, and `install-speculate.sh`. Run the one that matches the plugin you want to refresh.

For Meson-based builds such as `grid-seq`, the scripts wrap `meson setup`, `meson compile`, and `meson install`, so those commands can also be run manually if you prefer. If you want fine-grained control, open `lv2/<plugin>/README.md` for plugin-specific notes and repeat the manual CMake/Meson commands outlined in this section.

### Final checks
Once the system-wide dependencies are installed and the chosen plugin(s) have been built, host applications will find them automatically. If you run into linker errors, double-check that the `pkg-config` packages listed above are installed, then re-run the CMake/Meson configuration so the build system picks them up.
