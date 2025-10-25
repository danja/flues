#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>

#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>

#include "FloozyEngine.hpp"

#define FLOOZY_URI "https://danja.github.io/flues/plugins/floozy-poly"
#define LOG_PREFIX "[Floozy Poly Plugin] "

namespace flues::floozy_poly {

enum PortIndex : uint32_t {
    PORT_AUDIO_OUT = 0,
    PORT_MIDI_IN,
    PORT_SOURCE_ALGORITHM,
    PORT_SOURCE_PARAM1,
    PORT_SOURCE_PARAM2,
    PORT_SOURCE_LEVEL,
    PORT_SOURCE_NOISE,
    PORT_SOURCE_DC,
    PORT_ENVELOPE_ATTACK,
    PORT_ENVELOPE_RELEASE,
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
    PORT_MASTER_GAIN,
    PORT_TOTAL_COUNT
};

struct FloozyPolyLV2 {
    std::unique_ptr<FloozyPolyEngine> engine;
    float sampleRate;

    const LV2_Atom_Sequence* midiIn;
    float* audioOut;

    const float* sourceAlgorithm;
    const float* sourceParam1;
    const float* sourceParam2;
    const float* sourceLevel;
    const float* sourceNoise;
    const float* sourceDC;
    const float* envelopeAttack;
    const float* envelopeRelease;
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
    const float* masterGain;

    LV2_URID_Map* map;
    LV2_URID midiEventUrid;
    LV2_URID atomSequenceUrid;
};

static void apply_parameters(FloozyPolyLV2* self) {
    if (!self || !self->engine) {
        return;
    }

    auto apply = [&](const float* port, auto setter) {
        if (port) {
            (self->engine.get()->*setter)(*port);
        }
    };

    apply(self->sourceAlgorithm, &FloozyPolyEngine::setAlgorithm);
    apply(self->sourceParam1, &FloozyPolyEngine::setParam1);
    apply(self->sourceParam2, &FloozyPolyEngine::setParam2);
    apply(self->sourceLevel, &FloozyPolyEngine::setToneLevel);
    apply(self->sourceNoise, &FloozyPolyEngine::setNoiseLevel);
    apply(self->sourceDC, &FloozyPolyEngine::setDCLevel);
    apply(self->envelopeAttack, &FloozyPolyEngine::setAttack);
    apply(self->envelopeRelease, &FloozyPolyEngine::setRelease);

    if (self->interfaceType) {
        self->engine->setInterfaceType(*self->interfaceType);
    }

    apply(self->interfaceIntensity, &FloozyPolyEngine::setInterfaceIntensity);
    apply(self->tuning, &FloozyPolyEngine::setTuning);
    apply(self->ratio, &FloozyPolyEngine::setRatio);
    apply(self->delay1Feedback, &FloozyPolyEngine::setDelay1Feedback);
    apply(self->delay2Feedback, &FloozyPolyEngine::setDelay2Feedback);
    apply(self->filterFeedback, &FloozyPolyEngine::setFilterFeedback);
    apply(self->filterFrequency, &FloozyPolyEngine::setFilterFrequency);
    apply(self->filterQ, &FloozyPolyEngine::setFilterQ);
    apply(self->filterShape, &FloozyPolyEngine::setFilterShape);
    apply(self->lfoFrequency, &FloozyPolyEngine::setLFOFrequency);
    apply(self->modulationTypeLevel, &FloozyPolyEngine::setModulationTypeLevel);
    apply(self->reverbSize, &FloozyPolyEngine::setReverbSize);
    apply(self->reverbLevel, &FloozyPolyEngine::setReverbLevel);
    apply(self->masterGain, &FloozyPolyEngine::setMasterGain);
}

static void handle_midi(FloozyPolyLV2* self, const uint8_t* msg, uint32_t size) {
    if (size < 1 || !self || !self->engine) {
        return;
    }

    const uint8_t status = msg[0] & 0xF0U;
    const uint8_t data1 = size > 1 ? msg[1] : 0;
    const uint8_t data2 = size > 2 ? msg[2] : 0;

    switch (status) {
        case LV2_MIDI_MSG_NOTE_ON: {
            if (data2 == 0) {
                self->engine->noteOff(static_cast<int>(data1));
                break;
            }
            const float freq = 440.0f * std::pow(2.0f, (static_cast<int>(data1) - 69) / 12.0f);
            self->engine->noteOn(static_cast<int>(data1), freq);
            break;
        }
        case LV2_MIDI_MSG_NOTE_OFF: {
            self->engine->noteOff(static_cast<int>(data1));
            break;
        }
        case LV2_MIDI_MSG_CONTROLLER: {
            if (data1 == LV2_MIDI_CTL_ALL_SOUNDS_OFF || data1 == LV2_MIDI_CTL_ALL_NOTES_OFF) {
                self->engine->allNotesOff();
            }
            break;
        }
        default:
            break;
    }
}

} // namespace flues::floozy_poly

extern "C" {

static LV2_Handle instantiate(const LV2_Descriptor*, double rate,
                              const char*, const LV2_Feature* const* features) {
    using namespace flues::floozy_poly;

    auto* self = new FloozyPolyLV2();
    self->sampleRate = static_cast<float>(rate);
    self->engine = std::make_unique<FloozyPolyEngine>(self->sampleRate);

    self->midiIn = nullptr;
    self->audioOut = nullptr;

    self->sourceAlgorithm = nullptr;
    self->sourceParam1 = nullptr;
    self->sourceParam2 = nullptr;
    self->sourceLevel = nullptr;
    self->sourceNoise = nullptr;
    self->sourceDC = nullptr;
    self->envelopeAttack = nullptr;
    self->envelopeRelease = nullptr;
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
    self->masterGain = nullptr;

    self->map = nullptr;
    self->midiEventUrid = 0;
    self->atomSequenceUrid = 0;
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

    return self;
}

static void cleanup(LV2_Handle instance) {
    auto* self = static_cast<flues::floozy_poly::FloozyPolyLV2*>(instance);
    delete self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    using namespace flues::floozy_poly;
    auto* self = static_cast<FloozyPolyLV2*>(instance);

    switch (port) {
        case PORT_AUDIO_OUT: self->audioOut = static_cast<float*>(data); break;
        case PORT_MIDI_IN: self->midiIn = static_cast<const LV2_Atom_Sequence*>(data); break;
        case PORT_SOURCE_ALGORITHM: self->sourceAlgorithm = static_cast<const float*>(data); break;
        case PORT_SOURCE_PARAM1: self->sourceParam1 = static_cast<const float*>(data); break;
        case PORT_SOURCE_PARAM2: self->sourceParam2 = static_cast<const float*>(data); break;
        case PORT_SOURCE_LEVEL: self->sourceLevel = static_cast<const float*>(data); break;
        case PORT_SOURCE_NOISE: self->sourceNoise = static_cast<const float*>(data); break;
        case PORT_SOURCE_DC: self->sourceDC = static_cast<const float*>(data); break;
        case PORT_ENVELOPE_ATTACK: self->envelopeAttack = static_cast<const float*>(data); break;
        case PORT_ENVELOPE_RELEASE: self->envelopeRelease = static_cast<const float*>(data); break;
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
        case PORT_MASTER_GAIN: self->masterGain = static_cast<const float*>(data); break;
        default: break;
    }
}

static void activate(LV2_Handle) {}

static void run(LV2_Handle instance, uint32_t n_samples) {
    using namespace flues::floozy_poly;
    auto* self = static_cast<FloozyPolyLV2*>(instance);
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
    FLOOZY_URI,
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
