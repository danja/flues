#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <algorithm>
#include <cstdio>
#include <ctime>

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>

#include "PMSynthEngine.hpp"

#define PMSYNTH_URI "https://danja.github.io/flues/plugins/pm-synth"
#define PLUGIN_VERSION "v1.0.2-debug-2024-10-20"
#define LOG_PREFIX "[PM-Synth Plugin] "

// Constructor runs when library is loaded
__attribute__((constructor))
static void on_plugin_library_load() {
    time_t now = time(nullptr);
    std::fprintf(stderr, "\n");
    std::fprintf(stderr, "========================================\n");
    std::fprintf(stderr, LOG_PREFIX "DSP PLUGIN LOADED! %s\n", PLUGIN_VERSION);
    std::fprintf(stderr, LOG_PREFIX "Time: %s", ctime(&now));
    std::fprintf(stderr, LOG_PREFIX "Binary: pm_synth.so\n");
    std::fprintf(stderr, "========================================\n");
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
}

namespace flues::pm {

enum PortIndex : uint32_t {
    PORT_AUDIO_OUT = 0,
    PORT_MIDI_IN,
    PORT_DC_LEVEL,
    PORT_NOISE_LEVEL,
    PORT_TONE_LEVEL,
    PORT_ATTACK,
    PORT_RELEASE,
    PORT_INTERFACE_TYPE,
    PORT_INTERFACE_INTENSITY,
    PORT_TUNING,
    PORT_RATIO,
    PORT_DELAY1_FEEDBACK,
    PORT_DELAY2_FEEDBACK,
    PORT_FILTER_FEEDBACK,
    PORT_FILTER_FREQUENCY,
    PORT_FILTER_Q,
    PORT_FILTER_SHAPE,
    PORT_LFO_FREQUENCY,
    PORT_MOD_TYPE_LEVEL,
    PORT_REVERB_SIZE,
    PORT_REVERB_LEVEL,
    PORT_TOTAL_COUNT
};

struct PMSynthLV2 {
    std::unique_ptr<PMSynthEngine> engine;
    float sampleRate;

    const LV2_Atom_Sequence* midiIn;
    float* audioOut;

    const float* dcLevel;
    const float* noiseLevel;
    const float* toneLevel;
    const float* attack;
    const float* release;
    const float* interfaceType;
    const float* interfaceIntensity;
    const float* tuning;
    const float* ratio;
    const float* delay1Feedback;
    const float* delay2Feedback;
    const float* filterFeedback;
    const float* filterFrequency;
    const float* filterQ;
    const float* filterShape;
    const float* lfoFrequency;
    const float* modulationTypeLevel;
    const float* reverbSize;
    const float* reverbLevel;

    LV2_URID_Map* map;
    LV2_URID midiEventUrid;
    LV2_URID atomSequenceUrid;

    int currentNote;
};

static void apply_parameters(PMSynthLV2* self) {
    if (!self->engine) {
        return;
    }

    auto apply = [&](const float* port, auto setter) {
        if (port) {
            (self->engine.get()->*setter)(*port);
        }
    };

    apply(self->dcLevel, &PMSynthEngine::setDCLevel);
    apply(self->noiseLevel, &PMSynthEngine::setNoiseLevel);
    apply(self->toneLevel, &PMSynthEngine::setToneLevel);
    apply(self->attack, &PMSynthEngine::setAttack);
    apply(self->release, &PMSynthEngine::setRelease);

    if (self->interfaceType) {
        self->engine->setInterfaceType(*self->interfaceType);
    }
    apply(self->interfaceIntensity, &PMSynthEngine::setInterfaceIntensity);
    apply(self->tuning, &PMSynthEngine::setTuning);
    apply(self->ratio, &PMSynthEngine::setRatio);
    apply(self->delay1Feedback, &PMSynthEngine::setDelay1Feedback);
    apply(self->delay2Feedback, &PMSynthEngine::setDelay2Feedback);
    apply(self->filterFeedback, &PMSynthEngine::setFilterFeedback);
    apply(self->filterFrequency, &PMSynthEngine::setFilterFrequency);
    apply(self->filterQ, &PMSynthEngine::setFilterQ);
    apply(self->filterShape, &PMSynthEngine::setFilterShape);
    apply(self->lfoFrequency, &PMSynthEngine::setLFOFrequency);
    apply(self->modulationTypeLevel, &PMSynthEngine::setModulationTypeLevel);
    apply(self->reverbSize, &PMSynthEngine::setReverbSize);
    apply(self->reverbLevel, &PMSynthEngine::setReverbLevel);
}

static void handle_midi(PMSynthLV2* self, const uint8_t* msg, uint32_t size) {
    if (size < 1 || !self->engine) {
        return;
    }

    const uint8_t status = msg[0] & 0xF0U;
    const uint8_t data1 = size > 1 ? msg[1] : 0;
    const uint8_t data2 = size > 2 ? msg[2] : 0;

    switch (status) {
        case LV2_MIDI_MSG_NOTE_ON: {
            if (data2 == 0) {
                if (self->currentNote == data1) {
                    self->engine->noteOff();
                    self->currentNote = -1;
                }
                break;
            }
            const float freq = 440.0f * std::pow(2.0f, (static_cast<int>(data1) - 69) / 12.0f);
            self->engine->noteOn(freq);
            self->currentNote = data1;
            break;
        }
        case LV2_MIDI_MSG_NOTE_OFF: {
            if (self->currentNote == data1) {
                self->engine->noteOff();
                self->currentNote = -1;
            }
            break;
        }
        case LV2_MIDI_MSG_CONTROLLER: {
            if (data1 == LV2_MIDI_CTL_ALL_SOUNDS_OFF || data1 == LV2_MIDI_CTL_ALL_NOTES_OFF) {
                self->engine->noteOff();
                self->currentNote = -1;
            }
            break;
        }
        default:
            break;
    }
}

} // namespace flues::pm

extern "C" {

static LV2_Handle instantiate(const LV2_Descriptor*, double rate,
                              const char* bundle_path, const LV2_Feature* const* features) {
    using namespace flues::pm;

    std::fprintf(stderr, LOG_PREFIX "instantiate() called\n");
    std::fprintf(stderr, LOG_PREFIX "  Sample rate: %.1f Hz\n", rate);
    std::fprintf(stderr, LOG_PREFIX "  Bundle path: %s\n", bundle_path);

    auto* self = new PMSynthLV2();
    self->sampleRate = static_cast<float>(rate);
    self->engine = std::make_unique<PMSynthEngine>(self->sampleRate);

    std::fprintf(stderr, LOG_PREFIX "  Engine created successfully\n");
    self->midiIn = nullptr;
    self->audioOut = nullptr;

    self->dcLevel = nullptr;
    self->noiseLevel = nullptr;
    self->toneLevel = nullptr;
    self->attack = nullptr;
    self->release = nullptr;
    self->interfaceType = nullptr;
    self->interfaceIntensity = nullptr;
    self->tuning = nullptr;
    self->ratio = nullptr;
    self->delay1Feedback = nullptr;
    self->delay2Feedback = nullptr;
    self->filterFeedback = nullptr;
    self->filterFrequency = nullptr;
    self->filterQ = nullptr;
    self->filterShape = nullptr;
    self->lfoFrequency = nullptr;
    self->modulationTypeLevel = nullptr;
    self->reverbSize = nullptr;
    self->reverbLevel = nullptr;

    self->map = nullptr;
    self->midiEventUrid = 0;
    self->currentNote = -1;

    for (const LV2_Feature* const* f = features; f && *f; ++f) {
        if (!strcmp((*f)->URI, LV2_URID__map)) {
            self->map = static_cast<LV2_URID_Map*>((*f)->data);
        }
    }

    if (!self->map) {
        delete self;
        return nullptr;
    }

    self->midiEventUrid = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
    self->atomSequenceUrid = self->map->map(self->map->handle, LV2_ATOM__Sequence);

    std::fprintf(stderr, LOG_PREFIX "instantiate() complete! Instance: %p\n", (void*)self);
    std::fflush(stderr);

    return self;
}

static void cleanup(LV2_Handle instance) {
    std::fprintf(stderr, LOG_PREFIX "cleanup() called\n");
    std::fflush(stderr);
    auto* self = static_cast<flues::pm::PMSynthLV2*>(instance);
    delete self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    using namespace flues::pm;
    auto* self = static_cast<PMSynthLV2*>(instance);

    switch (port) {
        case PORT_AUDIO_OUT: self->audioOut = static_cast<float*>(data); break;
        case PORT_MIDI_IN: self->midiIn = static_cast<const LV2_Atom_Sequence*>(data); break;
        case PORT_DC_LEVEL: self->dcLevel = static_cast<const float*>(data); break;
        case PORT_NOISE_LEVEL: self->noiseLevel = static_cast<const float*>(data); break;
        case PORT_TONE_LEVEL: self->toneLevel = static_cast<const float*>(data); break;
        case PORT_ATTACK: self->attack = static_cast<const float*>(data); break;
        case PORT_RELEASE: self->release = static_cast<const float*>(data); break;
        case PORT_INTERFACE_TYPE: self->interfaceType = static_cast<const float*>(data); break;
        case PORT_INTERFACE_INTENSITY: self->interfaceIntensity = static_cast<const float*>(data); break;
        case PORT_TUNING: self->tuning = static_cast<const float*>(data); break;
        case PORT_RATIO: self->ratio = static_cast<const float*>(data); break;
        case PORT_DELAY1_FEEDBACK: self->delay1Feedback = static_cast<const float*>(data); break;
        case PORT_DELAY2_FEEDBACK: self->delay2Feedback = static_cast<const float*>(data); break;
        case PORT_FILTER_FEEDBACK: self->filterFeedback = static_cast<const float*>(data); break;
        case PORT_FILTER_FREQUENCY: self->filterFrequency = static_cast<const float*>(data); break;
        case PORT_FILTER_Q: self->filterQ = static_cast<const float*>(data); break;
        case PORT_FILTER_SHAPE: self->filterShape = static_cast<const float*>(data); break;
        case PORT_LFO_FREQUENCY: self->lfoFrequency = static_cast<const float*>(data); break;
        case PORT_MOD_TYPE_LEVEL: self->modulationTypeLevel = static_cast<const float*>(data); break;
        case PORT_REVERB_SIZE: self->reverbSize = static_cast<const float*>(data); break;
        case PORT_REVERB_LEVEL: self->reverbLevel = static_cast<const float*>(data); break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    auto* self = static_cast<flues::pm::PMSynthLV2*>(instance);
    if (!self) {
        return;
    }
    self->engine = std::make_unique<flues::pm::PMSynthEngine>(self->sampleRate);
    self->currentNote = -1;
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    using namespace flues::pm;
    auto* self = static_cast<PMSynthLV2*>(instance);
    if (!self || !self->audioOut) {
        return;
    }

    apply_parameters(self);

    float* out = self->audioOut;
    std::memset(out, 0, n_samples * sizeof(float));

    uint32_t frame = 0;

    if (self->midiIn && self->midiIn->atom.type == self->atomSequenceUrid) {
        LV2_ATOM_SEQUENCE_FOREACH(self->midiIn, ev) {
            const uint32_t eventFrame = ev->time.frames >= 0
                ? static_cast<uint32_t>(ev->time.frames)
                : 0u;

            if (frame < eventFrame) {
                const uint32_t limit = std::min(eventFrame, n_samples);
                for (; frame < limit; ++frame) {
                    out[frame] = self->engine->process();
                }
            }

            if (ev->body.type == self->midiEventUrid) {
                const uint8_t* msg = reinterpret_cast<const uint8_t*>(ev + 1);
                handle_midi(self, msg, ev->body.size);
            }
        }
    }

    for (; frame < n_samples; ++frame) {
        out[frame] = self->engine->process();
    }
}

static void deactivate(LV2_Handle) {}

static const void* extension_data(const char*) {
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    PMSYNTH_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : nullptr;
}

} // extern "C"
