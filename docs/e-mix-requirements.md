# E-Mix Euclidean Mixer

An lv2 plugin which reuses much of the code of lv2/p-mix

It will play or silence audio in blocks according to Euclidean patterns. 

It will be sync'd to the transport as in p-mix

Total Bars will be the number of bars over which the pattern will be applied. The pattern will be divided according to Division. Steps will be the number of blocks within the pattern which are played. Fade will detrmine over how many bars the block will be faded in and out. Fade bars will be considered part of the playing block, so a fade in will start at the beginning of a block and continue for n bars, a fade out will start n bars from the end of a block. 

Controls will be 5 text edit boxes. Values will be persisted.

## Controls

* Total Bars : default 128
* Division : default 16
* Steps : default 8 
* Offset : default 0
* Fade : default 0


