# EuDelay

EuDelay will be a delay based processor lv2 plugin, built on Euclidean rhythm - the concept of spacing a number of events as evenly as possible over time.

It will follow the same patterns as p-mix, having a UI with a series of rotary knobs.

It will take one or two channels of audio as input, have one or two channels output.

## Controls

* Scale : ratio of delay time to BPM
* Steps (NSteps) : overall number of steps in the rhythm. Values 2-24, default 16
* Taps : number of delay line taps. Values 1-NSteps
* Offset : number of steps between the start and the first tap. Values 0-NSteps
* Feedback : from sum of tapped values, 0-100%
* Mix : wet/dry
