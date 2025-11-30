# Chatterbox

Speech-like sound generator

first implementation is as a progressive web app

## Overview

Chatterbox is an audio synthesizer based on a simulation of human speech.

It follows the shape of the IPA vowel quadrilateral.

## Engine

The app core is based around the following DSP components :

* larynx : generates a modified sawtooth wave to simulate the human larynx
* aspirator : a noise generator 
* F1, F2, F3, F4 : formant filters

## User Interface

### Joystick

The center of the screen features a canvas area that operates like a joystick. Clicking in the area initiates an event. The coordinates are mapped to the frequency and Q of F1 and F2

### Sliders

* pitch
* stress (amplitude, distorted at higher levels)
* attack
* decay
* F3 frequency
* F4 frequency

### Checkboxes

* voiced (checked by default) - larynx is generating
* aspirated (unchecked by default) 
* nasal
* sing
* shout
* fry


## MIDI Control

The app responds to note on, note off, pitch and velocity for voicing with addition control channels used for other parameters.

## LV2 Plugin Implementation

Following successful completion of the web application, Chatterbox was ported to a native LV2 plugin (`lv2/chatterbox/`) featuring:
- C++ implementation with all DSP modules preserved
- Native X11/Cairo UI with IPA vowel quadrilateral joystick
- Comprehensive MIDI control (note on/off, velocity, 15 CC mappings)
- All vocal modes and effects from the web version
- See `lv2/chatterbox/README.md` for complete documentation