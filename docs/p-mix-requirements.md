# P-Mix Probabilistic Mixer LV2 Plugin

## Overview

P-Mix will act as an autonomous mixer for an audio track in a DAW intended to aid DJ-like performance. It will keep a count of the current bar in the song and at every transition (see Granularity below) either :

1. Maintain current audio volume
2. Fade the track in (if it was at zero volume)
3. Fade the track out (if it was at full volume)
4. Cut the track to full volume (if it was at zero volume)
5. Cut the track to zero volume (if it was at max volume)

There will be a numeric probability expressed as a percentage for each of the alternatives : Maintain, Fade and Cut.

Granularity is the number of bars that should be counted between (potential) transitions, ie. applications of the probabilistic algorithm. It will have a value between 1 and 32.

The values of the probabilities and the granularity will be persisted between sessions.

P-Mix will operate over however many audio channels there are in the current track.

The user interface will contain a simple user interface with four knobs, each with an associated edit box :

* Granularity 
* Maintain
* Fade
* Cut

It will follow the patterns of lv2/padseq
