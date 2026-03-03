#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/midi/midi.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#define EUCLID_MONO_URI "https://danja.github.io/flues/plugins/euclid-mono"

static constexpr int kMinStepsPerBar = 8;
static constexpr int kMaxStepsPerBar = 24;
static constexpr int kDefaultStepsPerBar = 16;
static constexpr uint8_t kMidiChannel10 = 0x99; // Note On, channel 10

enum LogicOp {
    LOGIC_AND = 0,
    LOGIC_OR = 1,
    LOGIC_XOR = 2,
    LOGIC_NAND = 3,
    LOGIC_NOR = 4,
    LOGIC_XNOR = 5,
    LOGIC_OP_COUNT = 6
};

enum PortIndex {
    PORT_CONTROL = 0,
    PORT_MIDI_OUT,
    PORT_STEPS_PER_BAR,
    PORT_SWING,
    PORT_SEED,
    PORT_MIDI_NOTE,
    PORT_A_BEATS,
    PORT_A_OFFSET,
    PORT_A_LENGTH,
    PORT_A_RANDOM,
    PORT_B_BEATS,
    PORT_B_OFFSET,
    PORT_B_LENGTH,
    PORT_B_RANDOM,
    PORT_INVERT_A,
    PORT_INVERT_B,
    PORT_LOGIC_OP,
    PORT_TOTAL_COUNT
};

struct PatternPorts {
    const float* beats;
    const float* offset;
    const float* length;
    const float* random;
};

struct PatternParams {
    int beats;
    int offset;
    int length;
    float random;
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

struct EuclidMonoLV2 {
    const LV2_Atom_Sequence* control;
    LV2_Atom_Sequence* midiOut;

    const float* stepsPerBarPort;
    const float* swingPort;
    const float* seedPort;
    const float* midiNotePort;

    PatternPorts patternPorts[2];
    const float* invertAPort;
    const float* invertBPort;
    const float* logicOpPort;

    LV2_URID_Map* map;
    LV2_URID midiEventUrid;
    LV2_URID atomSequenceUrid;
    LV2_URID timePositionUrid;
    LV2_URID timeBeatsPerMinuteUrid;
    LV2_URID timeSpeedUrid;
    LV2_URID timeBeatsPerBarUrid;
    LV2_URID timeBeatUnitUrid;
    LV2_URID atomObjectUrid;
    LV2_URID atomFloatUrid;

    double sampleRate;
    float hostBPM;
    bool transportPlaying;

    float beatsPerBar;
    float beatUnit;

    int stepsPerBar;
    float swing;
    uint32_t seed;
    uint32_t lastSeed;
    int midiNote;

    PatternParams patterns[2];
    bool invertA;
    bool invertB;
    int logicOp;

    double baseFramesPerStep;
    double stepFrameCounter;
    uint32_t stepIndex;

    double midiClockPhase;
    bool midiClockRunning;

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

static bool applyLogic(int logicOp, bool a, bool b) {
    switch (logicOp) {
        case LOGIC_AND:  return a && b;
        case LOGIC_OR:   return a || b;
        case LOGIC_XOR:  return (a != b);
        case LOGIC_NAND: return !(a && b);
        case LOGIC_NOR:  return !(a || b);
        case LOGIC_XNOR: return (a == b);
        default:         return a && b;
    }
}

static void emitNote(EuclidMonoLV2* self, uint32_t frameOffset, uint8_t note, uint8_t velocity) {
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

static bool patternHit(EuclidMonoLV2* self, const PatternParams& params, float* outRandomness) {
    const int length = clampi(params.length, 1, kMaxStepsPerBar);
    const int pulses = clampi(params.beats, 0, length);
    const int offset = clampi(params.offset, 0, length - 1);
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

    *outRandomness = randomness;
    return hit;
}

static void emitStep(EuclidMonoLV2* self, uint32_t frameOffset) {
    float randA = 0.0f;
    float randB = 0.0f;

    bool hitA = patternHit(self, self->patterns[0], &randA);
    bool hitB = patternHit(self, self->patterns[1], &randB);

    if (self->invertA) {
        hitA = !hitA;
    }
    if (self->invertB) {
        hitB = !hitB;
    }

    if (!applyLogic(self->logicOp, hitA, hitB)) {
        return;
    }

    const float randomness = randA > randB ? randA : randB;
    const int baseVelocity = 100;
    const int spread = static_cast<int>(randomness * 40.0f);
    int velocity = baseVelocity;
    if (spread > 0) {
        velocity += self->rng.nextInt(-spread, spread);
    }
    velocity = clampi(velocity, 1, 127);

    emitNote(self, frameOffset, static_cast<uint8_t>(clampi(self->midiNote, 0, 127)), static_cast<uint8_t>(velocity));
}

static void applyPorts(EuclidMonoLV2* self) {
    self->stepsPerBar = clampi(static_cast<int>(std::lround(portValue(self->stepsPerBarPort, static_cast<float>(kDefaultStepsPerBar)))),
                               kMinStepsPerBar, kMaxStepsPerBar);
    self->swing = clampf(portValue(self->swingPort, 0.0f), 0.0f, 1.0f);
    self->seed = static_cast<uint32_t>(clampf(portValue(self->seedPort, 1.0f), 0.0f, 65535.0f));
    self->midiNote = clampi(static_cast<int>(std::lround(portValue(self->midiNotePort, 60.0f))), 0, 127);

    for (int i = 0; i < 2; ++i) {
        const PatternPorts& ports = self->patternPorts[i];
        PatternParams values;
        values.beats = clampi(static_cast<int>(std::lround(portValue(ports.beats, i == 0 ? 4.0f : 3.0f))), 0, kMaxStepsPerBar);
        values.offset = clampi(static_cast<int>(std::lround(portValue(ports.offset, 0.0f))), 0, kMaxStepsPerBar - 1);
        values.length = clampi(static_cast<int>(std::lround(portValue(ports.length, static_cast<float>(kDefaultStepsPerBar)))), 1, kMaxStepsPerBar);
        values.random = clampf(portValue(ports.random, 0.0f), 0.0f, 1.0f);
        self->patterns[i] = values;
    }

    self->invertA = portValue(self->invertAPort, 0.0f) >= 0.5f;
    self->invertB = portValue(self->invertBPort, 0.0f) >= 0.5f;
    self->logicOp = clampi(static_cast<int>(std::lround(portValue(self->logicOpPort, 0.0f))), 0, LOGIC_OP_COUNT - 1);
}

static LV2_Handle instantiate(const LV2_Descriptor*, double rate, const char*, const LV2_Feature* const* features) {
    EuclidMonoLV2* self = new EuclidMonoLV2();
    if (!self) {
        return nullptr;
    }

    self->control = nullptr;
    self->midiOut = nullptr;
    self->stepsPerBarPort = nullptr;
    self->swingPort = nullptr;
    self->seedPort = nullptr;
    self->midiNotePort = nullptr;
    self->patternPorts[0] = {nullptr, nullptr, nullptr, nullptr};
    self->patternPorts[1] = {nullptr, nullptr, nullptr, nullptr};
    self->invertAPort = nullptr;
    self->invertBPort = nullptr;
    self->logicOpPort = nullptr;

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
    self->timeBeatsPerBarUrid = self->map->map(self->map->handle, "http://lv2plug.in/ns/ext/time#beatsPerBar");
    self->timeBeatUnitUrid = self->map->map(self->map->handle, "http://lv2plug.in/ns/ext/time#beatUnit");
    self->atomObjectUrid = self->map->map(self->map->handle, LV2_ATOM__Object);
    self->atomFloatUrid = self->map->map(self->map->handle, LV2_ATOM__Float);

    self->sampleRate = rate;
    self->hostBPM = 120.0f;
    self->transportPlaying = false;
    self->beatsPerBar = 4.0f;
    self->beatUnit = 4.0f;
    self->stepsPerBar = kDefaultStepsPerBar;
    self->swing = 0.0f;
    self->seed = 1;
    self->lastSeed = 0;
    self->midiNote = 60;
    self->patterns[0] = {4, 0, kDefaultStepsPerBar, 0.0f};
    self->patterns[1] = {3, 0, kDefaultStepsPerBar, 0.0f};
    self->invertA = false;
    self->invertB = false;
    self->logicOp = LOGIC_AND;

    self->baseFramesPerStep = self->sampleRate * 60.0 * 4.0 / (self->hostBPM * self->stepsPerBar);
    self->stepFrameCounter = 0.0;
    self->stepIndex = 0;
    self->midiClockPhase = 0.0;
    self->midiClockRunning = false;

    self->rng.seed(self->seed);

    return static_cast<LV2_Handle>(self);
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    EuclidMonoLV2* self = static_cast<EuclidMonoLV2*>(instance);

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
        case PORT_MIDI_NOTE:
            self->midiNotePort = static_cast<const float*>(data);
            break;
        case PORT_A_BEATS:
            self->patternPorts[0].beats = static_cast<const float*>(data);
            break;
        case PORT_A_OFFSET:
            self->patternPorts[0].offset = static_cast<const float*>(data);
            break;
        case PORT_A_LENGTH:
            self->patternPorts[0].length = static_cast<const float*>(data);
            break;
        case PORT_A_RANDOM:
            self->patternPorts[0].random = static_cast<const float*>(data);
            break;
        case PORT_B_BEATS:
            self->patternPorts[1].beats = static_cast<const float*>(data);
            break;
        case PORT_B_OFFSET:
            self->patternPorts[1].offset = static_cast<const float*>(data);
            break;
        case PORT_B_LENGTH:
            self->patternPorts[1].length = static_cast<const float*>(data);
            break;
        case PORT_B_RANDOM:
            self->patternPorts[1].random = static_cast<const float*>(data);
            break;
        case PORT_INVERT_A:
            self->invertAPort = static_cast<const float*>(data);
            break;
        case PORT_INVERT_B:
            self->invertBPort = static_cast<const float*>(data);
            break;
        case PORT_LOGIC_OP:
            self->logicOpPort = static_cast<const float*>(data);
            break;
        default:
            break;
    }
}

static void activate(LV2_Handle instance) {
    EuclidMonoLV2* self = static_cast<EuclidMonoLV2*>(instance);
    self->stepFrameCounter = 0.0;
    self->stepIndex = 0;
    self->transportPlaying = false;
    self->midiClockPhase = 0.0;
    self->midiClockRunning = false;
    self->rng.seed(self->seed);
}

static double step_frames(double baseFrames, float swing, uint32_t stepIndex) {
    const double swingFactor = (stepIndex % 2 == 1)
        ? (1.0 + swing * 0.5)
        : (1.0 - swing * 0.5);
    const double clamped = swingFactor < 0.2 ? 0.2 : swingFactor;
    return baseFrames * clamped;
}

static double step_clocks(double baseClocks, float swing, uint32_t stepIndex) {
    const double swingFactor = (stepIndex % 2 == 1)
        ? (1.0 + swing * 0.5)
        : (1.0 - swing * 0.5);
    const double clamped = swingFactor < 0.2 ? 0.2 : swingFactor;
    return baseClocks * clamped;
}

static void run(LV2_Handle instance, uint32_t nframes) {
    EuclidMonoLV2* self = static_cast<EuclidMonoLV2*>(instance);
    if (!self->midiOut) {
        return;
    }

    self->midiOut->atom.type = self->atomSequenceUrid;
    self->midiOut->atom.size = sizeof(LV2_Atom_Sequence_Body);
    self->midiOut->body.unit = 0;
    self->midiOut->body.pad = 0;

    float bpm = self->hostBPM > 0.0f ? self->hostBPM : 120.0f;
    bool transportPlaying = false;

    bool midiClockSeen = false;
    bool timePositionSeen = false;
    if (self->control && self->control->atom.type == self->atomSequenceUrid) {
        LV2_ATOM_SEQUENCE_FOREACH(self->control, ev) {
            const LV2_Atom_Object* obj = nullptr;
            if (ev->body.type == self->timePositionUrid) {
                obj = reinterpret_cast<const LV2_Atom_Object*>(&ev->body);
            } else if (ev->body.type == self->atomObjectUrid) {
                const LV2_Atom_Object* cand = reinterpret_cast<const LV2_Atom_Object*>(&ev->body);
                if (cand->body.otype == self->timePositionUrid) {
                    obj = cand;
                }
            }
            if (obj) {
                timePositionSeen = true;
                const LV2_Atom_Float* bpmAtom = nullptr;
                lv2_atom_object_get(obj, self->timeBeatsPerMinuteUrid, &bpmAtom, 0);
                if (bpmAtom && bpmAtom->atom.type == self->atomFloatUrid) {
                    bpm = bpmAtom->body;
                }

                const LV2_Atom_Float* beatsPerBarAtom = nullptr;
                lv2_atom_object_get(obj, self->timeBeatsPerBarUrid, &beatsPerBarAtom, 0);
                if (beatsPerBarAtom && beatsPerBarAtom->atom.type == self->atomFloatUrid) {
                    self->beatsPerBar = beatsPerBarAtom->body;
                }

                const LV2_Atom_Float* beatUnitAtom = nullptr;
                lv2_atom_object_get(obj, self->timeBeatUnitUrid, &beatUnitAtom, 0);
                if (beatUnitAtom && beatUnitAtom->atom.type == self->atomFloatUrid) {
                    self->beatUnit = beatUnitAtom->body;
                }

                const LV2_Atom_Float* speedAtom = nullptr;
                lv2_atom_object_get(obj, self->timeSpeedUrid, &speedAtom, 0);
                if (speedAtom && speedAtom->atom.type == self->atomFloatUrid) {
                    transportPlaying = speedAtom->body > 0.0f;
                }
            } else if (ev->body.type == self->midiEventUrid && ev->body.size >= 1) {
                const uint8_t* midiMsg = reinterpret_cast<const uint8_t*>(ev + 1);
                const uint8_t statusByte = midiMsg[0];
                if (statusByte == 0xF8 || statusByte == 0xFA || statusByte == 0xFB || statusByte == 0xFC) {
                    midiClockSeen = true;
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

    applyPorts(self);

    const int stepsPerBar = clampi(self->stepsPerBar, kMinStepsPerBar, kMaxStepsPerBar);
    const float swing = clampf(self->swing, 0.0f, 1.0f);
    const float beatsPerBar = self->beatsPerBar > 0.0f ? self->beatsPerBar : 4.0f;
    const float beatUnit = self->beatUnit > 0.0f ? self->beatUnit : 4.0f;

    self->baseFramesPerStep = self->sampleRate * 60.0 / bpm * (beatsPerBar / static_cast<float>(stepsPerBar));

    bool shouldPlay = false;
    if (timePositionSeen) {
        shouldPlay = transportPlaying;
    } else if (midiClockSeen) {
        shouldPlay = self->midiClockRunning;
    }

    if (!shouldPlay) {
        self->stepFrameCounter = 0.0;
        self->stepIndex = 0;
        self->midiClockPhase = 0.0;
        self->midiClockRunning = false;
        return;
    }

    if (!wasPlaying && transportPlaying && !midiClockSeen) {
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

    const bool midiClockEnabled = !timePositionSeen;
    if (midiClockSeen && midiClockEnabled && self->control && self->control->atom.type == self->atomSequenceUrid) {
        const float quartersPerBar = beatsPerBar / (beatUnit / 4.0f);
        const double clocksPerBar = 24.0 * quartersPerBar;
        const double baseClocksPerStep = clocksPerBar / static_cast<double>(stepsPerBar);

        LV2_ATOM_SEQUENCE_FOREACH(self->control, ev) {
            if (ev->body.type != self->midiEventUrid) {
                continue;
            }
            const uint8_t* midiMsg = reinterpret_cast<const uint8_t*>(ev + 1);
            if (ev->body.size < 1) {
                continue;
            }
            const uint8_t statusByte = midiMsg[0];
            if (statusByte == 0xF2 && ev->body.size >= 3) { // Song Position Pointer
                const uint16_t spp = static_cast<uint16_t>(midiMsg[1] | (midiMsg[2] << 7));
                const double sppClocks = static_cast<double>(spp) * 6.0;
                const double barClocks = clocksPerBar > 0.0 ? clocksPerBar : 96.0;
                const double stepClocks = baseClocksPerStep > 0.0 ? baseClocksPerStep : 6.0;

                const double barPhase = std::fmod(sppClocks, barClocks);
                uint32_t step = static_cast<uint32_t>(std::floor(barPhase / stepClocks));
                if (step >= static_cast<uint32_t>(stepsPerBar)) {
                    step = 0;
                }
                self->stepIndex = step;
                self->midiClockPhase = std::fmod(barPhase, stepClocks);
                continue;
            }
            if (statusByte == 0xFA) { // Start
                self->midiClockRunning = true;
                self->midiClockPhase = 0.0;
                self->stepIndex = 0;
                self->rng.seed(self->seed);
                continue;
            }
            if (statusByte == 0xFB) { // Continue
                self->midiClockRunning = true;
                continue;
            }
            if (statusByte == 0xFC) { // Stop
                self->midiClockRunning = false;
                self->midiClockPhase = 0.0;
                self->stepIndex = 0;
                continue;
            }
            if (statusByte != 0xF8) {
                continue;
            }
            if (!self->midiClockRunning) {
                continue;
            }

            self->midiClockPhase += 1.0;
            double targetClocks = step_clocks(baseClocksPerStep, swing, self->stepIndex);
            while (self->midiClockPhase >= targetClocks) {
                self->midiClockPhase -= targetClocks;
                emitStep(self, ev->time.frames);
                self->stepIndex = (self->stepIndex + 1) % static_cast<uint32_t>(stepsPerBar);
                targetClocks = step_clocks(baseClocksPerStep, swing, self->stepIndex);
            }
        }
    } else {
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
}

static void deactivate(LV2_Handle) {}

static void cleanup(LV2_Handle instance) {
    EuclidMonoLV2* self = static_cast<EuclidMonoLV2*>(instance);
    delete self;
}

static const void* extension_data(const char* uri) {
    (void)uri;
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    EUCLID_MONO_URI,
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
