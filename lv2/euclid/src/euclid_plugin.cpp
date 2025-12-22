// euclid_plugin.cpp - LV2 Euclidean rhythm generator for Drumkit

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/midi/midi.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#define EUCLID_URI "https://danja.github.io/flues/plugins/euclid"

static constexpr int kInstrumentCount = 9;
static constexpr int kMinStepsPerBar = 8;
static constexpr int kMaxStepsPerBar = 24;
static constexpr int kDefaultStepsPerBar = 16;
static constexpr uint8_t kMidiChannel10 = 0x99; // Note On, channel 10

static constexpr uint8_t kBeatsCCBase = 20;   // 20-28
static constexpr uint8_t kOffsetCCBase = 30;  // 30-38
static constexpr uint8_t kRandomCCBase = 40;  // 40-48
static constexpr uint8_t kLengthCCBase = 50;  // 50-58
static constexpr uint8_t kStepsCC = 70;
static constexpr uint8_t kSwingCC = 71;
static constexpr uint8_t kSeedCC = 72;

static const uint8_t kInstrumentNotes[kInstrumentCount] = {
    36, // Kick
    40, // Snare
    39, // Clap
    42, // Closed HH
    46, // Open HH
    45, // Lo Tom
    50, // Hi Tom
    41, // Crash
    51  // Bash
};

enum PortIndex {
    PORT_CONTROL = 0,
    PORT_MIDI_OUT = 1,
    PORT_STEPS_PER_BAR = 2,
    PORT_SWING = 3,
    PORT_SEED = 4,
    PORT_KICK_BEATS = 5,
    PORT_KICK_OFFSET = 6,
    PORT_KICK_LENGTH = 7,
    PORT_KICK_RANDOM = 8,
    PORT_SNARE_BEATS = 9,
    PORT_SNARE_OFFSET = 10,
    PORT_SNARE_LENGTH = 11,
    PORT_SNARE_RANDOM = 12,
    PORT_CLAP_BEATS = 13,
    PORT_CLAP_OFFSET = 14,
    PORT_CLAP_LENGTH = 15,
    PORT_CLAP_RANDOM = 16,
    PORT_HH_CLOSED_BEATS = 17,
    PORT_HH_CLOSED_OFFSET = 18,
    PORT_HH_CLOSED_LENGTH = 19,
    PORT_HH_CLOSED_RANDOM = 20,
    PORT_HH_OPEN_BEATS = 21,
    PORT_HH_OPEN_OFFSET = 22,
    PORT_HH_OPEN_LENGTH = 23,
    PORT_HH_OPEN_RANDOM = 24,
    PORT_TOM_LO_BEATS = 25,
    PORT_TOM_LO_OFFSET = 26,
    PORT_TOM_LO_LENGTH = 27,
    PORT_TOM_LO_RANDOM = 28,
    PORT_TOM_HI_BEATS = 29,
    PORT_TOM_HI_OFFSET = 30,
    PORT_TOM_HI_LENGTH = 31,
    PORT_TOM_HI_RANDOM = 32,
    PORT_CRASH_BEATS = 33,
    PORT_CRASH_OFFSET = 34,
    PORT_CRASH_LENGTH = 35,
    PORT_CRASH_RANDOM = 36,
    PORT_BASH_BEATS = 37,
    PORT_BASH_OFFSET = 38,
    PORT_BASH_LENGTH = 39,
    PORT_BASH_RANDOM = 40,
    PORT_TOTAL_COUNT = 41
};

struct InstrumentPorts {
    const float* beats;
    const float* offset;
    const float* length;
    const float* random;
};

struct ParamValue {
    int beats;
    int offset;
    int length;
    float random;
};

struct ParamFlags {
    bool beats;
    bool offset;
    bool length;
    bool random;
};

struct Rng {
    uint32_t state = 0x12345678u;

    void seed(uint32_t seedValue) {
        state = seedValue ? seedValue : 0x12345678u;
    }

    float nextFloat() {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
    }

    int nextInt(int minValue, int maxValue) {
        if (maxValue <= minValue) {
            return minValue;
        }
        const float t = nextFloat();
        return minValue + static_cast<int>(t * static_cast<float>(maxValue - minValue + 1));
    }
};

struct EuclidLV2 {
    const LV2_Atom_Sequence* control;
    LV2_Atom_Sequence* midiOut;

    const float* stepsPerBarPort;
    const float* swingPort;
    const float* seedPort;
    InstrumentPorts instruments[kInstrumentCount];

    LV2_URID_Map* map;
    LV2_URID midiEventUrid;
    LV2_URID atomSequenceUrid;
    LV2_URID timePositionUrid;
    LV2_URID timeBeatsPerMinuteUrid;
    LV2_URID timeSpeedUrid;
    LV2_URID atomFloatUrid;

    double sampleRate;
    float hostBPM;
    bool transportPlaying;

    int stepsPerBar;
    float swing;
    uint32_t seed;
    uint32_t lastSeed;
    bool stepsOverride;
    bool swingOverride;
    bool seedOverride;

    ParamValue params[kInstrumentCount];
    ParamValue ccParams[kInstrumentCount];
    ParamFlags ccFlags[kInstrumentCount];

    double baseFramesPerStep;
    double stepFrameCounter;
    uint32_t stepIndex;

    Rng rng;
};

static inline float clampf(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static inline int clampi(int value, int minValue, int maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static inline float portValue(const float* port, float fallback) {
    return port ? *port : fallback;
}

static bool euclidHit(int step, int pulses, int offset, int length) {
    if (length <= 0) return false;
    if (pulses <= 0) return false;
    if (pulses >= length) return true;

    const int baseStep = (step - offset + length) % length;
    return ((baseStep * pulses) % length) < pulses;
}

static void emitNote(EuclidLV2* self, uint32_t frameOffset, uint8_t note, uint8_t velocity) {
    LV2_Atom_Event event;
    event.time.frames = frameOffset;
    event.body.type = self->midiEventUrid;
    event.body.size = 3;

    const uint32_t capacity = 8192;
    LV2_Atom_Event* outEvent = lv2_atom_sequence_append_event(self->midiOut, capacity, &event);
    if (!outEvent) {
        return;
    }

    uint8_t* out = reinterpret_cast<uint8_t*>(outEvent + 1);
    out[0] = kMidiChannel10;
    out[1] = note;
    out[2] = velocity;
}

static void emitStep(EuclidLV2* self, uint32_t frameOffset) {
    for (int i = 0; i < kInstrumentCount; ++i) {
        const ParamValue& params = self->params[i];
        int length = clampi(params.length, 1, kMaxStepsPerBar);
        int pulses = clampi(params.beats, 0, length);
        int offset = clampi(params.offset, 0, length - 1);
        const float randomness = clampf(params.random, 0.0f, 1.0f);

        bool hit = euclidHit(static_cast<int>(self->stepIndex), pulses, offset, length);
        if (randomness > 0.0f) {
            const float roll = self->rng.nextFloat();
            if (hit && roll < randomness * 0.4f) {
                hit = false;
            } else if (!hit && roll < randomness * 0.2f) {
                hit = true;
            }
        }

        if (!hit) {
            continue;
        }

        const int baseVelocity = 100;
        const int spread = static_cast<int>(randomness * 40.0f);
        int velocity = baseVelocity;
        if (spread > 0) {
            velocity += self->rng.nextInt(-spread, spread);
        }
        velocity = clampi(velocity, 1, 127);
        emitNote(self, frameOffset, kInstrumentNotes[i], static_cast<uint8_t>(velocity));
    }
}

static int ccToInt(uint8_t value, int minValue, int maxValue) {
    const float t = static_cast<float>(value) / 127.0f;
    const float scaled = minValue + (maxValue - minValue) * t;
    return static_cast<int>(std::lround(scaled));
}

static float ccToFloat(uint8_t value) {
    return static_cast<float>(value) / 127.0f;
}

static void handle_cc(EuclidLV2* self, uint8_t cc, uint8_t value) {
    if (cc >= kBeatsCCBase && cc < kBeatsCCBase + kInstrumentCount) {
        int index = cc - kBeatsCCBase;
        self->ccParams[index].beats = ccToInt(value, 0, kMaxStepsPerBar);
        self->ccFlags[index].beats = true;
        return;
    }

    if (cc >= kOffsetCCBase && cc < kOffsetCCBase + kInstrumentCount) {
        int index = cc - kOffsetCCBase;
        self->ccParams[index].offset = ccToInt(value, 0, kMaxStepsPerBar - 1);
        self->ccFlags[index].offset = true;
        return;
    }

    if (cc >= kRandomCCBase && cc < kRandomCCBase + kInstrumentCount) {
        int index = cc - kRandomCCBase;
        self->ccParams[index].random = ccToFloat(value);
        self->ccFlags[index].random = true;
        return;
    }

    if (cc >= kLengthCCBase && cc < kLengthCCBase + kInstrumentCount) {
        int index = cc - kLengthCCBase;
        self->ccParams[index].length = ccToInt(value, 1, kMaxStepsPerBar);
        self->ccFlags[index].length = true;
        return;
    }

    if (cc == kStepsCC) {
        self->stepsPerBar = ccToInt(value, kMinStepsPerBar, kMaxStepsPerBar);
        self->stepsOverride = true;
        return;
    }

    if (cc == kSwingCC) {
        self->swing = ccToFloat(value);
        self->swingOverride = true;
        return;
    }

    if (cc == kSeedCC) {
        self->seed = static_cast<uint32_t>(value) * 516u;
        self->seedOverride = true;
    }
}

static void apply_ports(EuclidLV2* self) {
    const int stepsFromPort = clampi(static_cast<int>(std::lround(portValue(self->stepsPerBarPort, static_cast<float>(kDefaultStepsPerBar)))),
                                     kMinStepsPerBar, kMaxStepsPerBar);
    if (!self->stepsOverride) {
        self->stepsPerBar = stepsFromPort;
    }

    const float swingFromPort = clampf(portValue(self->swingPort, 0.0f), 0.0f, 1.0f);
    if (!self->swingOverride) {
        self->swing = swingFromPort;
    }

    const float seedFromPort = portValue(self->seedPort, 1.0f);
    if (!self->seedOverride) {
        self->seed = static_cast<uint32_t>(clampf(seedFromPort, 0.0f, 65535.0f));
    }

    for (int i = 0; i < kInstrumentCount; ++i) {
        const InstrumentPorts& ports = self->instruments[i];
        ParamValue values;
        values.beats = clampi(static_cast<int>(std::lround(portValue(ports.beats, 0.0f))), 0, kMaxStepsPerBar);
        values.offset = clampi(static_cast<int>(std::lround(portValue(ports.offset, 0.0f))), 0, kMaxStepsPerBar - 1);
        values.length = clampi(static_cast<int>(std::lround(portValue(ports.length, static_cast<float>(kDefaultStepsPerBar)))), 1, kMaxStepsPerBar);
        values.random = clampf(portValue(ports.random, 0.0f), 0.0f, 1.0f);

        if (self->ccFlags[i].beats) {
            values.beats = self->ccParams[i].beats;
        }
        if (self->ccFlags[i].offset) {
            values.offset = self->ccParams[i].offset;
        }
        if (self->ccFlags[i].length) {
            values.length = self->ccParams[i].length;
        }
        if (self->ccFlags[i].random) {
            values.random = self->ccParams[i].random;
        }

        self->params[i] = values;
    }
}

static LV2_Handle instantiate(const LV2_Descriptor*, double rate, const char*, const LV2_Feature* const* features) {
    EuclidLV2* self = new EuclidLV2();
    if (!self) {
        return nullptr;
    }

    self->control = nullptr;
    self->midiOut = nullptr;
    self->stepsPerBarPort = nullptr;
    self->swingPort = nullptr;
    self->seedPort = nullptr;

    self->map = nullptr;
    for (int i = 0; features[i]; ++i) {
        if (std::strcmp(features[i]->URI, LV2_URID__map) == 0) {
            self->map = static_cast<LV2_URID_Map*>(features[i]->data);
        }
    }

    if (!self->map) {
        delete self;
        return nullptr;
    }

    self->midiEventUrid = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
    self->atomSequenceUrid = self->map->map(self->map->handle, LV2_ATOM__Sequence);
    self->timePositionUrid = self->map->map(self->map->handle, "http://lv2plug.in/ns/ext/time#Position");
    self->timeBeatsPerMinuteUrid = self->map->map(self->map->handle, "http://lv2plug.in/ns/ext/time#beatsPerMinute");
    self->timeSpeedUrid = self->map->map(self->map->handle, "http://lv2plug.in/ns/ext/time#speed");
    self->atomFloatUrid = self->map->map(self->map->handle, LV2_ATOM__Float);

    self->sampleRate = rate;
    self->hostBPM = 120.0f;
    self->transportPlaying = true;
    self->stepsPerBar = kDefaultStepsPerBar;
    self->swing = 0.0f;
    self->seed = 1;
    self->lastSeed = 0;
    self->stepsOverride = false;
    self->swingOverride = false;
    self->seedOverride = false;

    self->baseFramesPerStep = self->sampleRate * 60.0 * 4.0 / (self->hostBPM * self->stepsPerBar);
    self->stepFrameCounter = 0.0;
    self->stepIndex = 0;

    self->rng.seed(self->seed);

    for (int i = 0; i < kInstrumentCount; ++i) {
        self->instruments[i] = {nullptr, nullptr, nullptr, nullptr};
        self->params[i] = {0, 0, kDefaultStepsPerBar, 0.0f};
        self->ccParams[i] = {0, 0, kDefaultStepsPerBar, 0.0f};
        self->ccFlags[i] = {false, false, false, false};
    }

    return static_cast<LV2_Handle>(self);
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    EuclidLV2* self = static_cast<EuclidLV2*>(instance);

    switch (port) {
        case PORT_CONTROL:
            self->control = static_cast<const LV2_Atom_Sequence*>(data);
            break;
        case PORT_MIDI_OUT:
            self->midiOut = static_cast<LV2_Atom_Sequence*>(data);
            break;
        case PORT_STEPS_PER_BAR:
            self->stepsPerBarPort = static_cast<const float*>(data);
            break;
        case PORT_SWING:
            self->swingPort = static_cast<const float*>(data);
            break;
        case PORT_SEED:
            self->seedPort = static_cast<const float*>(data);
            break;
        case PORT_KICK_BEATS:
            self->instruments[0].beats = static_cast<const float*>(data);
            break;
        case PORT_KICK_OFFSET:
            self->instruments[0].offset = static_cast<const float*>(data);
            break;
        case PORT_KICK_LENGTH:
            self->instruments[0].length = static_cast<const float*>(data);
            break;
        case PORT_KICK_RANDOM:
            self->instruments[0].random = static_cast<const float*>(data);
            break;
        case PORT_SNARE_BEATS:
            self->instruments[1].beats = static_cast<const float*>(data);
            break;
        case PORT_SNARE_OFFSET:
            self->instruments[1].offset = static_cast<const float*>(data);
            break;
        case PORT_SNARE_LENGTH:
            self->instruments[1].length = static_cast<const float*>(data);
            break;
        case PORT_SNARE_RANDOM:
            self->instruments[1].random = static_cast<const float*>(data);
            break;
        case PORT_CLAP_BEATS:
            self->instruments[2].beats = static_cast<const float*>(data);
            break;
        case PORT_CLAP_OFFSET:
            self->instruments[2].offset = static_cast<const float*>(data);
            break;
        case PORT_CLAP_LENGTH:
            self->instruments[2].length = static_cast<const float*>(data);
            break;
        case PORT_CLAP_RANDOM:
            self->instruments[2].random = static_cast<const float*>(data);
            break;
        case PORT_HH_CLOSED_BEATS:
            self->instruments[3].beats = static_cast<const float*>(data);
            break;
        case PORT_HH_CLOSED_OFFSET:
            self->instruments[3].offset = static_cast<const float*>(data);
            break;
        case PORT_HH_CLOSED_LENGTH:
            self->instruments[3].length = static_cast<const float*>(data);
            break;
        case PORT_HH_CLOSED_RANDOM:
            self->instruments[3].random = static_cast<const float*>(data);
            break;
        case PORT_HH_OPEN_BEATS:
            self->instruments[4].beats = static_cast<const float*>(data);
            break;
        case PORT_HH_OPEN_OFFSET:
            self->instruments[4].offset = static_cast<const float*>(data);
            break;
        case PORT_HH_OPEN_LENGTH:
            self->instruments[4].length = static_cast<const float*>(data);
            break;
        case PORT_HH_OPEN_RANDOM:
            self->instruments[4].random = static_cast<const float*>(data);
            break;
        case PORT_TOM_LO_BEATS:
            self->instruments[5].beats = static_cast<const float*>(data);
            break;
        case PORT_TOM_LO_OFFSET:
            self->instruments[5].offset = static_cast<const float*>(data);
            break;
        case PORT_TOM_LO_LENGTH:
            self->instruments[5].length = static_cast<const float*>(data);
            break;
        case PORT_TOM_LO_RANDOM:
            self->instruments[5].random = static_cast<const float*>(data);
            break;
        case PORT_TOM_HI_BEATS:
            self->instruments[6].beats = static_cast<const float*>(data);
            break;
        case PORT_TOM_HI_OFFSET:
            self->instruments[6].offset = static_cast<const float*>(data);
            break;
        case PORT_TOM_HI_LENGTH:
            self->instruments[6].length = static_cast<const float*>(data);
            break;
        case PORT_TOM_HI_RANDOM:
            self->instruments[6].random = static_cast<const float*>(data);
            break;
        case PORT_CRASH_BEATS:
            self->instruments[7].beats = static_cast<const float*>(data);
            break;
        case PORT_CRASH_OFFSET:
            self->instruments[7].offset = static_cast<const float*>(data);
            break;
        case PORT_CRASH_LENGTH:
            self->instruments[7].length = static_cast<const float*>(data);
            break;
        case PORT_CRASH_RANDOM:
            self->instruments[7].random = static_cast<const float*>(data);
            break;
        case PORT_BASH_BEATS:
            self->instruments[8].beats = static_cast<const float*>(data);
            break;
        case PORT_BASH_OFFSET:
            self->instruments[8].offset = static_cast<const float*>(data);
            break;
        case PORT_BASH_LENGTH:
            self->instruments[8].length = static_cast<const float*>(data);
            break;
        case PORT_BASH_RANDOM:
            self->instruments[8].random = static_cast<const float*>(data);
            break;
        default:
            break;
    }
}

static void activate(LV2_Handle instance) {
    EuclidLV2* self = static_cast<EuclidLV2*>(instance);
    self->stepFrameCounter = 0.0;
    self->stepIndex = 0;
    self->transportPlaying = true;
    self->rng.seed(self->seed);
}

static double step_frames(double baseFrames, float swing, uint32_t stepIndex) {
    const double swingFactor = (stepIndex % 2 == 1)
        ? (1.0 + swing * 0.5)
        : (1.0 - swing * 0.5);
    const double clamped = swingFactor < 0.2 ? 0.2 : swingFactor;
    return baseFrames * clamped;
}

static void run(LV2_Handle instance, uint32_t nframes) {
    EuclidLV2* self = static_cast<EuclidLV2*>(instance);
    if (!self->midiOut) {
        return;
    }

    self->midiOut->atom.type = self->atomSequenceUrid;
    self->midiOut->atom.size = sizeof(LV2_Atom_Sequence_Body);
    self->midiOut->body.unit = 0;
    self->midiOut->body.pad = 0;

    float bpm = self->hostBPM > 0.0f ? self->hostBPM : 120.0f;
    bool transportPlaying = self->transportPlaying;

    if (self->control && self->control->atom.type == self->atomSequenceUrid) {
        LV2_ATOM_SEQUENCE_FOREACH(self->control, ev) {
            if (ev->body.type == self->timePositionUrid) {
                const LV2_Atom_Object* obj = reinterpret_cast<const LV2_Atom_Object*>(&ev->body);
                const LV2_Atom_Float* bpmAtom = nullptr;
                lv2_atom_object_get(obj, self->timeBeatsPerMinuteUrid, &bpmAtom, 0);
                if (bpmAtom && bpmAtom->atom.type == self->atomFloatUrid) {
                    bpm = bpmAtom->body;
                }

                const LV2_Atom_Float* speedAtom = nullptr;
                lv2_atom_object_get(obj, self->timeSpeedUrid, &speedAtom, 0);
                if (speedAtom && speedAtom->atom.type == self->atomFloatUrid) {
                    transportPlaying = speedAtom->body > 0.0f;
                }
            } else if (ev->body.type == self->midiEventUrid) {
                const uint8_t* midiMsg = reinterpret_cast<const uint8_t*>(ev + 1);
                if (ev->body.size >= 3) {
                    const uint8_t status = midiMsg[0] & 0xF0;
                    if (status == 0xB0) {
                        handle_cc(self, midiMsg[1], midiMsg[2]);
                    }
                }
            }
        }
    }

    if (bpm <= 0.0f) {
        bpm = 120.0f;
    }

    const bool wasPlaying = self->transportPlaying;
    self->transportPlaying = transportPlaying;
    self->hostBPM = bpm;

    apply_ports(self);

    const int stepsPerBar = clampi(self->stepsPerBar, kMinStepsPerBar, kMaxStepsPerBar);
    const float swing = clampf(self->swing, 0.0f, 1.0f);

    self->baseFramesPerStep = self->sampleRate * 60.0 * 4.0 / (bpm * stepsPerBar);

    if (!transportPlaying) {
        self->stepFrameCounter = 0.0;
        self->stepIndex = 0;
        return;
    }

    if (!wasPlaying && transportPlaying) {
        self->stepFrameCounter = 0.0;
        self->stepIndex = 0;
        self->rng.seed(self->seed);
    }

    if (self->seed != self->lastSeed) {
        self->rng.seed(self->seed);
        self->lastSeed = self->seed;
    }

    double framePosition = 0.0;
    double stepCounter = self->stepFrameCounter;

    if (self->stepIndex >= static_cast<uint32_t>(stepsPerBar)) {
        self->stepIndex = 0;
    }

    if (stepCounter <= 1e-9 && nframes > 0) {
        emitStep(self, 0);
        self->stepIndex = (self->stepIndex + 1) % static_cast<uint32_t>(stepsPerBar);
    }

    while (framePosition < static_cast<double>(nframes)) {
        double currentFrames = step_frames(self->baseFramesPerStep, swing, self->stepIndex);
        if (currentFrames < 1.0) {
            currentFrames = 1.0;
        }
        if (stepCounter >= currentFrames) {
            stepCounter = 0.0;
        }

        double framesLeftInStep = currentFrames - stepCounter;
        if (framesLeftInStep <= 0.0) {
            framesLeftInStep = currentFrames;
            stepCounter = 0.0;
        }

        const double framesLeft = static_cast<double>(nframes) - framePosition;
        if (framesLeftInStep > framesLeft) {
            stepCounter += framesLeft;
            framePosition = static_cast<double>(nframes);
            break;
        }

        framePosition += framesLeftInStep;
        uint32_t eventFrame = static_cast<uint32_t>(std::llround(framePosition));
        if (eventFrame < nframes) {
            emitStep(self, eventFrame);
        }
        self->stepIndex = (self->stepIndex + 1) % static_cast<uint32_t>(stepsPerBar);
        stepCounter = 0.0;
    }

    self->stepFrameCounter = stepCounter;
}

static void deactivate(LV2_Handle) {}

static void cleanup(LV2_Handle instance) {
    EuclidLV2* self = static_cast<EuclidLV2*>(instance);
    delete self;
}

static const void* extension_data(const char* uri) {
    (void)uri;
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    EUCLID_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

extern "C" {

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : nullptr;
}

} // extern "C"
