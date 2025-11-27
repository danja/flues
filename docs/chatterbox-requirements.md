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