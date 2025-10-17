# Physical Modelling Synth

Stove is an extended, general-purpose version of clarinet synth. It is built as a responsive PWA using Vite. Its user interface has a set of knobs in blocks corresponding to internal modules and a one-octave piano keyboard with computer keypresses mapped to GUI keys.

Code will be under experiments/pm-synth/src with a live app version built then copied into the www directory using Github actions.

The architecture is modular with blocks of the user interface mirroring code modules. Functionality is achieved in small classes, no large code files. All components have corresponding unit and integration tests using Vitest and Playwright.  

Signals are triggered by a gate and CV from the keyboard. The flow is from Sources through Envelope through Delay Lines. The Feedback module follows the Delay Lines passing signal back. Finally the Filter module gives tone control of the output.

## Modules

### Sources

Three sources are provided, each with a level control. They are DC - which corresponds to a constant pressure such as windspeed in a blown instrument; white noise to simulate turbulence; tone, which is a sawtooth wave with its frequency controlled by the CV.

### Envelope

The envelope section has two controls : Attack and Release controlling the initial rise in amplitude of the sources and the final decline. When a key is held done the signal is held constant.

### Interfaces

The interface module has a rotary switch for the type of stimulation and a knob for the intensity of the interface effect (such as reed stiffness).

* Pluck - simulating a guitar string pluck
* Hit - simulating a piano mallet or drum beater
* Reed - simulating a clarinet reed
* Flute - simulating a flute mouthpiece 
* Brass - simulating a trumpet mouthpiece

### Delay Lines

There are two delay lines with their length determined by the CV level, ie. pitch. When equal lengths these model the tube in a wind instrument or the string in a string instrument. This module has two controls, one for tuning (affecting the delay length) and one to adjust the ratio of the delay line lengths to simulate drum and gong sounds.

### Filter

The filter has three controls : Frequency, Q and Shape. The Shape controls the response from lowpass through bandpass to highpass.

### Modulation

This section has two knobs, one for LFO frequency and the other for type/level. The LFO is a sine wave. The Level knob applies zero modulation in the 12 o'clock position, maximum AM in the fully CCW position and maximum FM in the fully CW position.

### Feedback

This has three knobs controlling the level of feedback from the first and second delay line as well as post-Filter.