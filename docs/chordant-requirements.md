# Chordant Requirements

lv2 plugin following the same pattern as the others, controls based on lv2/e-mix which is :

* tied to host DAW beat timing
* continuously buffers audio input on 2 channels
* mapped to Euclidean patterns to do the following -
 * during spaces, "beat low" between Euclidean "beat high", the audio is preserved until the next beat high
 * at beat high the audio is mixed normalised according to the number of beat low segments recorded, output to 2 channels

The idea is for something that, for example, may be used to take a melody line and generate a rhythmic chord periodically

