#pragma once

#include <lv2/urid/urid.h>

struct CadenceURIDs {
    LV2_URID atom_Object = 0;
    LV2_URID atom_Blank = 0;
    LV2_URID atom_Float = 0;
    LV2_URID atom_Double = 0;
    LV2_URID atom_Int = 0;
    LV2_URID atom_Long = 0;
    LV2_URID atom_Chunk = 0;
    LV2_URID atom_Sequence = 0;
    LV2_URID midi_Event = 0;
    LV2_URID time_Position = 0;
    LV2_URID time_speed = 0;
    LV2_URID time_bar = 0;
    LV2_URID time_barBeat = 0;
    LV2_URID time_beatsPerBar = 0;
    LV2_URID time_beatsPerMinute = 0;
    LV2_URID state_progression = 0;
    LV2_URID state_variation = 0;
};
