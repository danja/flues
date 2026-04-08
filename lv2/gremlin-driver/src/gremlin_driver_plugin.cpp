#include "GremlinDriverEngine.hpp"

#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#define GREMLIN_DRIVER_URI "https://danja.github.io/flues/plugins/gremlin-driver"

namespace {

using flues::gremlindriver::ClockMode;
using flues::gremlindriver::GremlinDriverEngine;
using flues::gremlindriver::LaneConfig;
using flues::gremlindriver::TriggerConfig;

enum PortIndex : uint32_t {
    PORT_CONTROL_IN = 0,
    PORT_MIDI_OUT = 1,
    PORT_CLOCK_MODE = 2,
    PORT_BPM = 3,
    PORT_LANE1_TARGET = 4,
    PORT_LANE1_SHAPE = 5,
    PORT_LANE1_RATE = 6,
    PORT_LANE1_DEPTH = 7,
    PORT_LANE1_CENTER = 8,
    PORT_LANE2_TARGET = 9,
    PORT_LANE2_SHAPE = 10,
    PORT_LANE2_RATE = 11,
    PORT_LANE2_DEPTH = 12,
    PORT_LANE2_CENTER = 13,
    PORT_LANE3_TARGET = 14,
    PORT_LANE3_SHAPE = 15,
    PORT_LANE3_RATE = 16,
    PORT_LANE3_DEPTH = 17,
    PORT_LANE3_CENTER = 18,
    PORT_LANE4_TARGET = 19,
    PORT_LANE4_SHAPE = 20,
    PORT_LANE4_RATE = 21,
    PORT_LANE4_DEPTH = 22,
    PORT_LANE4_CENTER = 23,
    PORT_TRIGGER1_ACTION = 24,
    PORT_TRIGGER1_RATE = 25,
    PORT_TRIGGER1_CHANCE = 26,
    PORT_TRIGGER2_ACTION = 27,
    PORT_TRIGGER2_RATE = 28,
    PORT_TRIGGER2_CHANCE = 29,
    PORT_STATUS_LANE1 = 30,
    PORT_STATUS_LANE2 = 31,
    PORT_STATUS_LANE3 = 32,
    PORT_STATUS_LANE4 = 33,
    PORT_STATUS_TRIGGER1 = 34,
    PORT_STATUS_TRIGGER2 = 35,
    PORT_STATUS_TRANSPORT = 36,
    PORT_STATUS_BPM = 37,
    PORT_RANDOMIZE = 38
};

struct URIDs {
    LV2_URID atomSequence = 0;
    LV2_URID midiEvent = 0;
    LV2_URID timePosition = 0;
    LV2_URID timeBeatsPerMinute = 0;
    LV2_URID timeSpeed = 0;
    LV2_URID atomFloat = 0;
};

struct MidiMessage {
    uint32_t frame = 0;
    uint32_t order = 0;
    std::vector<uint8_t> bytes;
};

struct GremlinDriverLV2 {
    const LV2_Atom_Sequence* controlIn = nullptr;
    LV2_Atom_Sequence* midiOut = nullptr;
    const float* clockMode = nullptr;
    const float* bpm = nullptr;
    const float* randomize = nullptr;
    std::array<const float*, 4> laneTarget{};
    std::array<const float*, 4> laneShape{};
    std::array<const float*, 4> laneRate{};
    std::array<const float*, 4> laneDepth{};
    std::array<const float*, 4> laneCenter{};
    std::array<const float*, 2> triggerAction{};
    std::array<const float*, 2> triggerRate{};
    std::array<const float*, 2> triggerChance{};
    std::array<float*, 4> statusLane{};
    std::array<float*, 2> statusTrigger{};
    float* statusTransport = nullptr;
    float* statusBpm = nullptr;

    LV2_URID_Map* map = nullptr;
    URIDs urids{};

    GremlinDriverEngine engine;
    double sampleRate = 44100.0;
    uint32_t bufferCapacity = 16384;
    std::array<int, 10> lastSentCc{};
    uint32_t refreshSamples = 0;
    uint32_t randomState = 0x3c6ef372u;
    bool lastRandomizePressed = false;
};

static constexpr std::array<uint8_t, 16> kPrimaryKnobCCs = {
    16, 20, 24, 28, 46, 50, 54, 58,
    17, 21, 25, 29, 47, 51, 55, 59
};

static constexpr std::array<uint8_t, 8> kHiddenKnobCCs = {
    18, 22, 26, 30, 48, 52, 56, 60
};

static constexpr std::array<uint8_t, 8> kMacroFaderCCs = {
    19, 23, 27, 31, 49, 53, 57, 61
};

static constexpr uint8_t kMasterFaderCC = 62;

static constexpr std::array<uint8_t, 11> kActionNotes = {
    0,   // Off
    2,   // Reseed
    5,   // Burst
    8,   // Random source
    11,  // Random delay
    14,  // Random all
    17,  // Scene down
    20,  // Scene up
    23,  // Panic
    25,  // Mode down
    26   // Mode up
};

static float port_or_default(const float* port, float fallback) {
    return port ? *port : fallback;
}

static int enum_from_port(const float* port, int fallback) {
    if (!port) {
        return fallback;
    }
    return static_cast<int>(std::lround(*port));
}

static int clamp_cc(float normalized) {
    const float clamped = std::clamp(normalized, 0.0f, 1.0f);
    return static_cast<int>(std::lround(clamped * 127.0f));
}

static uint32_t next_random_u32(GremlinDriverLV2* self) {
    self->randomState = self->randomState * 1664525u + 1013904223u;
    return self->randomState;
}

static uint8_t random_cc(GremlinDriverLV2* self, uint8_t minValue = 0, uint8_t maxValue = 127) {
    if (maxValue < minValue) {
        std::swap(maxValue, minValue);
    }
    const uint32_t span = static_cast<uint32_t>(maxValue - minValue) + 1u;
    return static_cast<uint8_t>(minValue + (next_random_u32(self) % span));
}

static uint8_t random_primary_knob_value(GremlinDriverLV2* self, uint8_t cc) {
    switch (cc) {
        case 16: return random_cc(self, 20, 82);  // Damage
        case 20: return random_cc(self, 18, 78);  // Chaos
        case 24: return random_cc(self, 0, 40);   // Noise
        case 28: return random_cc(self, 8, 58);   // Drift
        case 46: return random_cc(self, 0, 52);   // Crunch
        case 50: return random_cc(self, 10, 55);  // Fold
        case 54: return random_cc(self, 0, 20);   // Attack
        case 58: return random_cc(self, 6, 40);   // Release
        case 17: return random_cc(self, 10, 70);  // Delay Time
        case 21: return random_cc(self, 18, 78);  // Feedback
        case 25: return random_cc(self, 5, 60);   // Warp
        case 29: return random_cc(self, 8, 62);   // Stutter
        case 47: return random_cc(self, 60, 118); // Tone
        case 51: return random_cc(self, 42, 98);  // Damping
        case 55: return random_cc(self, 16, 82);  // Space
        case 59: return random_cc(self, 30, 76);  // Output
        default: return random_cc(self);
    }
}

static uint8_t random_hidden_knob_value(GremlinDriverLV2* self, uint8_t cc) {
    switch (cc) {
        case 18: return random_cc(self, 58, 104); // Source Gain
        case 22: return random_cc(self, 70, 127); // Burst
        case 26: return random_cc(self, 10, 68);  // Pitch Spread
        case 30: return random_cc(self, 20, 72);  // Delay Mix
        case 48: return random_cc(self, 12, 68);  // Cross Feedback
        case 52: return random_cc(self, 10, 54);  // Glitch Length
        case 56: return random_cc(self, 18, 76);  // Chaos Rate
        case 60: return random_cc(self, 28, 86);  // Duck
        default: return random_cc(self);
    }
}

static void append_midi(LV2_Atom_Sequence* seq,
                        uint32_t capacity,
                        LV2_URID midiEventUrid,
                        uint32_t frame,
                        const uint8_t* msg,
                        uint32_t size) {
    LV2_Atom_Event ev;
    ev.time.frames = frame;
    ev.body.type = midiEventUrid;
    ev.body.size = size;

    LV2_Atom_Event* appended = lv2_atom_sequence_append_event(seq, capacity, &ev);
    if (!appended) {
        return;
    }

    uint8_t* body = reinterpret_cast<uint8_t*>(appended + 1);
    std::memcpy(body, msg, size);
}

static uint8_t target_to_cc(int target) {
    if (target >= 1 && target <= 8) {
        return kMacroFaderCCs[static_cast<size_t>(target - 1)];
    }
    return (target == 9) ? kMasterFaderCC : 0;
}

static uint8_t action_to_note(int action) {
    if (action < 0 || action >= static_cast<int>(kActionNotes.size())) {
        return 0;
    }
    return kActionNotes[static_cast<size_t>(action)];
}

static LV2_Handle instantiate(const LV2_Descriptor*,
                              double rate,
                              const char*,
                              const LV2_Feature* const* features) {
    auto* self = new GremlinDriverLV2();
    if (!self) {
        return nullptr;
    }

    self->sampleRate = rate;
    self->lastSentCc.fill(-1);

    for (int i = 0; features[i]; ++i) {
        if (!std::strcmp(features[i]->URI, LV2_URID__map)) {
            self->map = static_cast<LV2_URID_Map*>(features[i]->data);
        }
    }

    if (!self->map) {
        delete self;
        return nullptr;
    }

    self->urids.atomSequence = self->map->map(self->map->handle, LV2_ATOM__Sequence);
    self->urids.midiEvent = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
    self->urids.timePosition = self->map->map(self->map->handle, "http://lv2plug.in/ns/ext/time#Position");
    self->urids.timeBeatsPerMinute = self->map->map(self->map->handle, "http://lv2plug.in/ns/ext/time#beatsPerMinute");
    self->urids.timeSpeed = self->map->map(self->map->handle, "http://lv2plug.in/ns/ext/time#speed");
    self->urids.atomFloat = self->map->map(self->map->handle, LV2_ATOM__Float);

    self->engine.init(rate);
    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    auto* self = static_cast<GremlinDriverLV2*>(instance);
    if (!self) {
        return;
    }

    switch (port) {
        case PORT_CONTROL_IN: self->controlIn = static_cast<const LV2_Atom_Sequence*>(data); break;
        case PORT_MIDI_OUT: self->midiOut = static_cast<LV2_Atom_Sequence*>(data); break;
        case PORT_CLOCK_MODE: self->clockMode = static_cast<const float*>(data); break;
        case PORT_BPM: self->bpm = static_cast<const float*>(data); break;
        case PORT_RANDOMIZE: self->randomize = static_cast<const float*>(data); break;
        case PORT_LANE1_TARGET: self->laneTarget[0] = static_cast<const float*>(data); break;
        case PORT_LANE1_SHAPE: self->laneShape[0] = static_cast<const float*>(data); break;
        case PORT_LANE1_RATE: self->laneRate[0] = static_cast<const float*>(data); break;
        case PORT_LANE1_DEPTH: self->laneDepth[0] = static_cast<const float*>(data); break;
        case PORT_LANE1_CENTER: self->laneCenter[0] = static_cast<const float*>(data); break;
        case PORT_LANE2_TARGET: self->laneTarget[1] = static_cast<const float*>(data); break;
        case PORT_LANE2_SHAPE: self->laneShape[1] = static_cast<const float*>(data); break;
        case PORT_LANE2_RATE: self->laneRate[1] = static_cast<const float*>(data); break;
        case PORT_LANE2_DEPTH: self->laneDepth[1] = static_cast<const float*>(data); break;
        case PORT_LANE2_CENTER: self->laneCenter[1] = static_cast<const float*>(data); break;
        case PORT_LANE3_TARGET: self->laneTarget[2] = static_cast<const float*>(data); break;
        case PORT_LANE3_SHAPE: self->laneShape[2] = static_cast<const float*>(data); break;
        case PORT_LANE3_RATE: self->laneRate[2] = static_cast<const float*>(data); break;
        case PORT_LANE3_DEPTH: self->laneDepth[2] = static_cast<const float*>(data); break;
        case PORT_LANE3_CENTER: self->laneCenter[2] = static_cast<const float*>(data); break;
        case PORT_LANE4_TARGET: self->laneTarget[3] = static_cast<const float*>(data); break;
        case PORT_LANE4_SHAPE: self->laneShape[3] = static_cast<const float*>(data); break;
        case PORT_LANE4_RATE: self->laneRate[3] = static_cast<const float*>(data); break;
        case PORT_LANE4_DEPTH: self->laneDepth[3] = static_cast<const float*>(data); break;
        case PORT_LANE4_CENTER: self->laneCenter[3] = static_cast<const float*>(data); break;
        case PORT_TRIGGER1_ACTION: self->triggerAction[0] = static_cast<const float*>(data); break;
        case PORT_TRIGGER1_RATE: self->triggerRate[0] = static_cast<const float*>(data); break;
        case PORT_TRIGGER1_CHANCE: self->triggerChance[0] = static_cast<const float*>(data); break;
        case PORT_TRIGGER2_ACTION: self->triggerAction[1] = static_cast<const float*>(data); break;
        case PORT_TRIGGER2_RATE: self->triggerRate[1] = static_cast<const float*>(data); break;
        case PORT_TRIGGER2_CHANCE: self->triggerChance[1] = static_cast<const float*>(data); break;
        case PORT_STATUS_LANE1: self->statusLane[0] = static_cast<float*>(data); break;
        case PORT_STATUS_LANE2: self->statusLane[1] = static_cast<float*>(data); break;
        case PORT_STATUS_LANE3: self->statusLane[2] = static_cast<float*>(data); break;
        case PORT_STATUS_LANE4: self->statusLane[3] = static_cast<float*>(data); break;
        case PORT_STATUS_TRIGGER1: self->statusTrigger[0] = static_cast<float*>(data); break;
        case PORT_STATUS_TRIGGER2: self->statusTrigger[1] = static_cast<float*>(data); break;
        case PORT_STATUS_TRANSPORT: self->statusTransport = static_cast<float*>(data); break;
        case PORT_STATUS_BPM: self->statusBpm = static_cast<float*>(data); break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    auto* self = static_cast<GremlinDriverLV2*>(instance);
    if (!self) {
        return;
    }
    self->lastSentCc.fill(-1);
    self->refreshSamples = 0;
    self->lastRandomizePressed = false;
    self->engine.reset();
}

static void run(LV2_Handle instance, uint32_t nframes) {
    auto* self = static_cast<GremlinDriverLV2*>(instance);
    if (!self || !self->midiOut) {
        return;
    }

    self->midiOut->atom.type = self->urids.atomSequence;
    self->midiOut->atom.size = sizeof(LV2_Atom_Sequence_Body);
    self->midiOut->body.unit = 0;
    self->midiOut->body.pad = 0;

    float hostBpm = 0.0f;
    float hostSpeed = 0.0f;
    bool sawTransport = false;
    std::vector<MidiMessage> events;
    events.reserve(64);

    if (self->controlIn && self->controlIn->atom.type == self->urids.atomSequence) {
        LV2_ATOM_SEQUENCE_FOREACH(self->controlIn, ev) {
            if (ev->body.type == self->urids.midiEvent) {
                MidiMessage message;
                message.frame = ev->time.frames >= 0 ? static_cast<uint32_t>(ev->time.frames) : 0u;
                message.order = 2;
                const uint8_t* data = reinterpret_cast<const uint8_t*>(ev + 1);
                message.bytes.assign(data, data + ev->body.size);
                events.push_back(std::move(message));
            } else if (ev->body.type == self->urids.timePosition) {
                sawTransport = true;
                const LV2_Atom_Object* obj = reinterpret_cast<const LV2_Atom_Object*>(&ev->body);

                const LV2_Atom_Float* bpmAtom = nullptr;
                lv2_atom_object_get(obj, self->urids.timeBeatsPerMinute, &bpmAtom, 0);
                if (bpmAtom && bpmAtom->atom.type == self->urids.atomFloat) {
                    hostBpm = bpmAtom->body;
                }

                const LV2_Atom_Float* speedAtom = nullptr;
                lv2_atom_object_get(obj, self->urids.timeSpeed, &speedAtom, 0);
                if (speedAtom && speedAtom->atom.type == self->urids.atomFloat) {
                    hostSpeed = speedAtom->body;
                }
            }
        }
    }

    const int clockMode = enum_from_port(self->clockMode, static_cast<int>(ClockMode::Transport));
    const float fallbackBpm = std::clamp(port_or_default(self->bpm, 120.0f), 40.0f, 220.0f);

    flues::gremlindriver::ClockState clock{};
    clock.transportDetected = sawTransport;
    clock.transportRunning = sawTransport && hostSpeed > 0.0f;
    clock.bpm = (hostBpm > 1.0f) ? hostBpm : fallbackBpm;

    if (clockMode == static_cast<int>(ClockMode::Free)) {
        clock.bpm = fallbackBpm;
        clock.running = true;
    } else {
        clock.running = sawTransport ? (hostSpeed > 0.0f) : true;
    }

    std::array<LaneConfig, 4> lanes{};
    for (size_t i = 0; i < lanes.size(); ++i) {
        lanes[i].target = enum_from_port(self->laneTarget[i], 0);
        lanes[i].shape = enum_from_port(self->laneShape[i], 0);
        lanes[i].rate = std::clamp(port_or_default(self->laneRate[i], 0.5f), 0.0f, 1.0f);
        lanes[i].depth = std::clamp(port_or_default(self->laneDepth[i], 0.5f), 0.0f, 1.0f);
        lanes[i].center = std::clamp(port_or_default(self->laneCenter[i], 0.5f), 0.0f, 1.0f);
    }

    std::array<TriggerConfig, 2> triggers{};
    for (size_t i = 0; i < triggers.size(); ++i) {
        triggers[i].action = enum_from_port(self->triggerAction[i], 0);
        triggers[i].rate = std::clamp(port_or_default(self->triggerRate[i], 0.45f), 0.0f, 1.0f);
        triggers[i].chance = std::clamp(port_or_default(self->triggerChance[i], 0.35f), 0.0f, 1.0f);
    }

    const auto result = self->engine.process(nframes, clock, lanes, triggers);

    const bool randomizePressed = port_or_default(self->randomize, 0.0f) >= 0.5f;
    if (randomizePressed && !self->lastRandomizePressed) {
        for (const uint8_t cc : kPrimaryKnobCCs) {
            MidiMessage message;
            message.frame = 0;
            message.order = 0;
            const uint8_t value = random_primary_knob_value(self, cc);
            message.bytes = {
                static_cast<uint8_t>(0xB0),
                cc,
                value
            };
            events.push_back(std::move(message));
        }

        for (const uint8_t cc : kHiddenKnobCCs) {
            MidiMessage message;
            message.frame = 0;
            message.order = 0;
            const uint8_t value = random_hidden_knob_value(self, cc);
            message.bytes = {
                static_cast<uint8_t>(0xB0),
                cc,
                value
            };
            events.push_back(std::move(message));
        }
    }
    self->lastRandomizePressed = randomizePressed;

    self->refreshSamples += nframes;
    const bool forceRefresh = self->refreshSamples >= static_cast<uint32_t>(self->sampleRate * 0.25);
    if (forceRefresh) {
        self->refreshSamples = 0;
    }

    for (int target = 1; target <= 9; ++target) {
        if (!result.activeTargets[static_cast<size_t>(target)]) {
            continue;
        }
        const int ccValue = clamp_cc(result.targetValues[static_cast<size_t>(target)]);
        if (!forceRefresh && self->lastSentCc[static_cast<size_t>(target)] == ccValue) {
            continue;
        }

        const uint8_t cc = target_to_cc(target);
        if (cc == 0) {
            continue;
        }

        MidiMessage message;
        message.frame = 0;
        message.order = 0;
        message.bytes = {
            static_cast<uint8_t>(0xB0),
            cc,
            static_cast<uint8_t>(ccValue)
        };
        events.push_back(std::move(message));
        self->lastSentCc[static_cast<size_t>(target)] = ccValue;
    }

    for (size_t i = 0; i < triggers.size(); ++i) {
        if (!result.triggerFired[i]) {
            continue;
        }
        const uint8_t note = action_to_note(triggers[i].action);
        if (note == 0) {
            continue;
        }

        MidiMessage message;
        message.frame = result.triggerFrames[i];
        message.order = 1;
        message.bytes = {
            static_cast<uint8_t>(0x90),
            note,
            static_cast<uint8_t>(127)
        };
        events.push_back(std::move(message));
    }

    std::stable_sort(events.begin(), events.end(), [](const MidiMessage& a, const MidiMessage& b) {
        if (a.frame != b.frame) {
            return a.frame < b.frame;
        }
        return a.order < b.order;
    });

    for (const MidiMessage& event : events) {
        if (!event.bytes.empty()) {
            append_midi(self->midiOut,
                        self->bufferCapacity,
                        self->urids.midiEvent,
                        std::min(event.frame, nframes > 0 ? nframes - 1 : 0u),
                        event.bytes.data(),
                        static_cast<uint32_t>(event.bytes.size()));
        }
    }

    for (size_t i = 0; i < self->statusLane.size(); ++i) {
        if (self->statusLane[i]) {
            *self->statusLane[i] = result.laneValues[i];
        }
    }
    for (size_t i = 0; i < self->statusTrigger.size(); ++i) {
        if (self->statusTrigger[i]) {
            *self->statusTrigger[i] = result.triggerFlashes[i];
        }
    }
    if (self->statusTransport) {
        *self->statusTransport = result.transportIndicator;
    }
    if (self->statusBpm) {
        *self->statusBpm = result.effectiveBpm;
    }
}

static void deactivate(LV2_Handle) {
}

static void cleanup(LV2_Handle instance) {
    delete static_cast<GremlinDriverLV2*>(instance);
}

static const void* extension_data(const char*) {
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    GREMLIN_DRIVER_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

} // namespace

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return (index == 0) ? &descriptor : nullptr;
}
