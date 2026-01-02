// euclidean_gate_plugin.cpp - LV2 Euclidean gate effect

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#define EUCLIDEAN_GATE_URI "https://danja.github.io/flues/plugins/euclidean-gate"

static constexpr int kSteps = 16;
static constexpr float kTwoPi = 6.283185307179586f;

enum PortIndex {
    PORT_CONTROL_IN = 0,
    PORT_IN_L = 1,
    PORT_IN_R = 2,
    PORT_OUT_L = 3,
    PORT_OUT_R = 4,
    PORT_BEATS = 5,
    PORT_OFFSET = 6,
    PORT_ATTACK = 7,
    PORT_DECAY = 8,
    PORT_MODE = 9,
    PORT_RANDOM_ADD = 10,
    PORT_RANDOM_SUB = 11
};

enum GateMode {
    MODE_PASS = 0,
    MODE_MUTE = 1
};

struct GateUrids {
    LV2_URID atom_Sequence;
    LV2_URID atom_Object;
    LV2_URID atom_Blank;
    LV2_URID atom_Float;
    LV2_URID time_Position;
    LV2_URID time_beatsPerMinute;
    LV2_URID time_speed;
};

struct EuclideanGate {
    const LV2_Atom_Sequence* controlIn;
    const float* inL;
    const float* inR;
    float* outL;
    float* outR;
    const float* beats;
    const float* offset;
    const float* attackMs;
    const float* decayMs;
    const float* mode;
    const float* randomAdd;
    const float* randomSub;

    LV2_URID_Map* map;
    GateUrids urids;

    double sampleRate;
    float tempoBpm;
    bool playing;

    uint32_t stepFrames;
    uint32_t samplesInStep;
    int stepIndex;

    float envValue;
    float envIncrement;
    bool envActive;
    bool envAttackPhase;

    uint32_t rngState;
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

static inline float rand_float(uint32_t* state) {
    *state = (*state * 1664525u) + 1013904223u;
    return static_cast<float>(*state & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

static int euclid_hit(int step, int pulses, int offset, int length) {
    if (length <= 0 || pulses <= 0) return 0;
    if (pulses >= length) return 1;
    const int base = (step + length - (offset % length)) % length;
    return ((base * pulses) % length) < pulses;
}

static void start_envelope(EuclideanGate* self, uint32_t attackFrames, uint32_t decayFrames) {
    if (attackFrames == 0) {
        self->envValue = 1.0f;
        self->envAttackPhase = false;
        self->envIncrement = (decayFrames > 0) ? -1.0f / static_cast<float>(decayFrames) : -1.0f;
    } else {
        self->envValue = 0.0f;
        self->envAttackPhase = true;
        self->envIncrement = 1.0f / static_cast<float>(attackFrames);
    }
    self->envActive = true;
}

static LV2_Handle instantiate(const LV2_Descriptor* descriptor,
                              double sampleRate,
                              const char* bundlePath,
                              const LV2_Feature* const* features) {
    (void)descriptor;
    (void)bundlePath;

    EuclideanGate* self = new EuclideanGate();
    std::memset(self, 0, sizeof(EuclideanGate));
    self->sampleRate = sampleRate;
    self->tempoBpm = 120.0f;
    self->playing = true;

    if (features) {
        for (const LV2_Feature* const* f = features; *f; ++f) {
            if (std::strcmp((*f)->URI, LV2_URID__map) == 0) {
                self->map = static_cast<LV2_URID_Map*>((*f)->data);
                break;
            }
        }
    }

    if (self->map) {
        self->urids.atom_Sequence = self->map->map(self->map->handle, LV2_ATOM__Sequence);
        self->urids.atom_Object = self->map->map(self->map->handle, LV2_ATOM__Object);
        self->urids.atom_Blank = self->map->map(self->map->handle, LV2_ATOM__Blank);
        self->urids.atom_Float = self->map->map(self->map->handle, LV2_ATOM__Float);
        self->urids.time_Position = self->map->map(self->map->handle, LV2_TIME__Position);
        self->urids.time_beatsPerMinute = self->map->map(self->map->handle, LV2_TIME__beatsPerMinute);
        self->urids.time_speed = self->map->map(self->map->handle, LV2_TIME__speed);
    }

    return static_cast<LV2_Handle>(self);
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    EuclideanGate* self = static_cast<EuclideanGate*>(instance);
    switch (port) {
        case PORT_CONTROL_IN:
            self->controlIn = static_cast<const LV2_Atom_Sequence*>(data);
            break;
        case PORT_IN_L:
            self->inL = static_cast<const float*>(data);
            break;
        case PORT_IN_R:
            self->inR = static_cast<const float*>(data);
            break;
        case PORT_OUT_L:
            self->outL = static_cast<float*>(data);
            break;
        case PORT_OUT_R:
            self->outR = static_cast<float*>(data);
            break;
        case PORT_BEATS:
            self->beats = static_cast<const float*>(data);
            break;
        case PORT_OFFSET:
            self->offset = static_cast<const float*>(data);
            break;
        case PORT_ATTACK:
            self->attackMs = static_cast<const float*>(data);
            break;
        case PORT_DECAY:
            self->decayMs = static_cast<const float*>(data);
            break;
        case PORT_MODE:
            self->mode = static_cast<const float*>(data);
            break;
        case PORT_RANDOM_ADD:
            self->randomAdd = static_cast<const float*>(data);
            break;
        case PORT_RANDOM_SUB:
            self->randomSub = static_cast<const float*>(data);
            break;
        default:
            break;
    }
}

static void activate(LV2_Handle instance) {
    EuclideanGate* self = static_cast<EuclideanGate*>(instance);
    self->tempoBpm = 120.0f;
    self->playing = true;
    self->stepIndex = 0;
    self->samplesInStep = 0;
    self->envValue = 0.0f;
    self->envIncrement = 0.0f;
    self->envActive = false;
    self->envAttackPhase = false;
    self->rngState = 0x12345678u;
}

static void run(LV2_Handle instance, uint32_t nSamples) {
    EuclideanGate* self = static_cast<EuclideanGate*>(instance);

    if (self->controlIn && self->map) {
        LV2_ATOM_SEQUENCE_FOREACH(self->controlIn, ev) {
            const LV2_Atom_Object* obj = nullptr;
            if (ev->body.type == self->urids.time_Position) {
                obj = reinterpret_cast<const LV2_Atom_Object*>(&ev->body);
            } else if (ev->body.type == self->urids.atom_Object ||
                       ev->body.type == self->urids.atom_Blank) {
                const LV2_Atom_Object* cand = reinterpret_cast<const LV2_Atom_Object*>(&ev->body);
                if (cand->body.otype == self->urids.time_Position) {
                    obj = cand;
                }
            }
            if (obj) {
                LV2_Atom* bpm = nullptr;
                lv2_atom_object_get(obj, self->urids.time_beatsPerMinute, &bpm, 0);
                if (bpm && bpm->type == self->urids.atom_Float) {
                    const LV2_Atom_Float* bpmValue = reinterpret_cast<const LV2_Atom_Float*>(bpm);
                    self->tempoBpm = bpmValue->body;
                }
                LV2_Atom* speed = nullptr;
                lv2_atom_object_get(obj, self->urids.time_speed, &speed, 0);
                if (speed && speed->type == self->urids.atom_Float) {
                    const LV2_Atom_Float* speedValue = reinterpret_cast<const LV2_Atom_Float*>(speed);
                    self->playing = speedValue->body > 0.0f;
                }
            }
        }
    }

    const float bpm = clampf(self->tempoBpm, 30.0f, 300.0f);
    const float samplesPerBeat = static_cast<float>(self->sampleRate) * 60.0f / bpm;
    uint32_t stepFrames = static_cast<uint32_t>(samplesPerBeat / 4.0f);
    if (stepFrames == 0) stepFrames = 1;
    self->stepFrames = stepFrames;

    const int pulses = clampi(self->beats ? static_cast<int>(std::lround(*self->beats)) : 0, 0, kSteps);
    const int offset = clampi(self->offset ? static_cast<int>(std::lround(*self->offset)) : 0, 0, kSteps - 1);
    const float attackMs = clampf(self->attackMs ? *self->attackMs : 5.0f, 0.1f, 500.0f);
    const float decayMs = clampf(self->decayMs ? *self->decayMs : 120.0f, 1.0f, 2000.0f);
    const float randomAdd = clampf(self->randomAdd ? *self->randomAdd : 0.0f, 0.0f, 1.0f);
    const float randomSub = clampf(self->randomSub ? *self->randomSub : 0.0f, 0.0f, 1.0f);

    uint32_t attackFrames = static_cast<uint32_t>(attackMs * 0.001f * self->sampleRate);
    uint32_t decayFrames = static_cast<uint32_t>(decayMs * 0.001f * self->sampleRate);
    if (decayFrames == 0) decayFrames = 1;

    const int mode = self->mode ? (static_cast<int>(std::lround(*self->mode)) != 0) : 0;

    for (uint32_t i = 0; i < nSamples; ++i) {
        if (self->playing) {
            if (self->samplesInStep == 0) {
                bool hit = euclid_hit(self->stepIndex, pulses, offset, kSteps) != 0;
                if (hit && randomSub > 0.0001f) {
                    if (rand_float(&self->rngState) < randomSub) {
                        hit = false;
                    }
                }
                if (!hit && randomAdd > 0.0001f) {
                    if (rand_float(&self->rngState) < randomAdd) {
                        hit = true;
                    }
                }
                if (hit) {
                    start_envelope(self, attackFrames, decayFrames);
                }
            }
        }

        if (self->envActive) {
            self->envValue += self->envIncrement;
            if (self->envAttackPhase) {
                if (self->envValue >= 1.0f) {
                    self->envValue = 1.0f;
                    self->envAttackPhase = false;
                    self->envIncrement = -1.0f / static_cast<float>(decayFrames);
                }
            } else {
                if (self->envValue <= 0.0f) {
                    self->envValue = 0.0f;
                    self->envActive = false;
                }
            }
        }

        const float inL = self->inL ? self->inL[i] : 0.0f;
        const float inR = self->inR ? self->inR[i] : 0.0f;
        const float env = self->envValue;
        const float gain = (mode == MODE_MUTE) ? (1.0f - env) : env;
        if (self->outL) {
            self->outL[i] = inL * gain;
        }
        if (self->outR) {
            self->outR[i] = inR * gain;
        }

        if (self->playing) {
            self->samplesInStep++;
            if (self->samplesInStep >= self->stepFrames) {
                self->samplesInStep = 0;
                self->stepIndex = (self->stepIndex + 1) % kSteps;
            }
        }
    }
}

static void deactivate(LV2_Handle instance) {
    (void)instance;
}

static void cleanup(LV2_Handle instance) {
    delete static_cast<EuclideanGate*>(instance);
}

static const void* extension_data(const char* uri) {
    (void)uri;
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    EUCLIDEAN_GATE_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    if (index == 0) {
        return &descriptor;
    }
    return nullptr;
}
