# P-Mix 

![screenshot](../../docs/images/p-mix.png)

**Status 2026-02-02 :** probably buggy, but already used in production.

P-Mix is a probabilistic mixer lv2 plugin. When applied to track in a DAW it will automatically cut/fade in/out the audio of that track according to certain parameters.

A typical use is for electronic dance music where there are many parallel tracks and only some of them should be playing at a given time. In this scenario the plugin acts as a virtual DJ.

## Parameters

1. **Granularity**

Granularity refers to the number of bars between potential transitions for the current track.

2. **Maintain**

The probability of the current status of the track (in/out) being maintained after the transition. 

3. **Fade**

The probability of the transition being a fade in/out.

4. **Cut**

The probability of the transition being a cut in/out.

5. **Fade Max Length**

The proportion of the granularity value over which to perform fades.

6. **Bias**

The target proportion of the overall playing time that this track should be audible.

## Build & Install

Clone this repo. In the root of the repo run :

```sh
./install-p-mix.sh 
```

Manual build instructions?

Dependencies?