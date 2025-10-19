# Disyn Distortion Synthesizer

Disyn is a browser-based sound synthesizer based on experiments/pm-synth which has the following elements :

* Oscillator
* Attack Release envelope shaper with VCA
* Reverb
* Keyboard
* MIDI Input

The oscillator module uses the algorithms used in docs/reference/distortion-synthesis.html
It has 3 controls : Algorithm, Param 1, Param 2
Algorithm will switch between the available algorithms. Param 1 & 2 will be assigned to parameters dependent on the algorithm such as pitch ratio.
The fundamental pitch will be determined by keyboard or midi input following the pattern of pm-synth.

Envelope shaper and reverb will follow the pattern on pm-synth.

code should go in experiments/disyn