#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <algorithm>

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>

#include "DisynEngine.hpp"

#define DISYN_URI "https://danja.github.io/flues/plugins/disyn"

namespace flues::disyn {

enum PortIndex : uint32_t {
    PORT_AUDIO_OUT_L = 0,
    PORT_AUDIO_OUT_R,
    PORT_MIDI_IN,
    PORT_ALGORITHM_TYPE,
    PORT_PARAM_1,
    PORT_PARAM_2,
    PORT_PARAM_3,
    PORT_ENVELOPE_ATTACK,
    PORT_ENVELOPE_RELEASE,
    PORT_REVERB_SIZE,
    PORT_REVERB_LEVEL,
    PORT_MASTER_GAIN,
    PORT_TOTAL_COUNT
};

struct DisynLV2 {
    std::unique_ptr<DisynEngine> engine;
    float sampleRate;

    const LV2_Atom_Sequence* midiIn;
    float* audioOutLeft;
    float* audioOutRight;

    const float* algorithmType;
    const float* param1;
    const float* param2;
    const float* param3;
    const float* attack;
    const float* release;
    const float* reverbSize;
    const float* reverbLevel;
    const float* masterGain;

    LV2_URID_Map* map;
    LV2_URID midiEventUrid;
    LV2_URID atomSequenceUrid;

    int currentNote;
};

static void apply_parameters(DisynLV2* self) {
    if (!self->engine) {
        return;
    }

    auto apply = [&](const float* port, auto setter) {
        if (port) {
            (self->engine.get()->*setter)(*port);
        }
    };

    if (self->algorithmType) {
        self->engine->setAlgorithm(static_cast<int>(std::round(*self->algorithmType)));
    }
    apply(self->param1, &DisynEngine::setParam1);
    apply(self->param2, &DisynEngine::setParam2);
    apply(self->param3, &DisynEngine::setParam3);
    apply(self->attack, &DisynEngine::setAttack);
    apply(self->release, &DisynEngine::setRelease);
    apply(self->reverbSize, &DisynEngine::setReverbSize);
    apply(self->reverbLevel, &DisynEngine::setReverbLevel);
    apply(self->masterGain, &DisynEngine::setMasterGain);
}

static void handle_midi(DisynLV2* self, const uint8_t* msg, uint32_t size) {
    if (size < 1 || !self->engine) {
        return;
    }

    const uint8_t status = msg[0] & 0xF0U;
    const uint8_t data1 = size > 1 ? msg[1] : 0;
    const uint8_t data2 = size > 2 ? msg[2] : 0;

    switch (status) {
        case LV2_MIDI_MSG_NOTE_ON: {
            if (data2 == 0) {
                // Note on with velocity 0 is note off
                if (self->currentNote == data1) {
                    self->engine->noteOff();
                    self->currentNote = -1;
                }
                break;
            }
            const float freq = 440.0f * std::pow(2.0f, (static_cast<int>(data1) - 69) / 12.0f);
            const float velocity = static_cast<float>(data2) / 127.0f;
            self->engine->noteOn(freq, velocity);
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

} // namespace flues::disyn

extern "C" {

static LV2_Handle instantiate(const LV2_Descriptor*, double rate,
                              const char*, const LV2_Feature* const* features) {
    using namespace flues::disyn;

    auto* self = new DisynLV2();
    self->sampleRate = static_cast<float>(rate);
    self->engine = std::make_unique<DisynEngine>(self->sampleRate);
    self->midiIn = nullptr;
    self->audioOutLeft = nullptr;
    self->audioOutRight = nullptr;

    self->algorithmType = nullptr;
    self->param1 = nullptr;
    self->param2 = nullptr;
    self->param3 = nullptr;
    self->attack = nullptr;
    self->release = nullptr;
    self->reverbSize = nullptr;
    self->reverbLevel = nullptr;
    self->masterGain = nullptr;

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

    return self;
}

static void cleanup(LV2_Handle instance) {
    auto* self = static_cast<flues::disyn::DisynLV2*>(instance);
    delete self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    using namespace flues::disyn;
    auto* self = static_cast<DisynLV2*>(instance);

    switch (port) {
        case PORT_AUDIO_OUT_L: self->audioOutLeft = static_cast<float*>(data); break;
        case PORT_AUDIO_OUT_R: self->audioOutRight = static_cast<float*>(data); break;
        case PORT_MIDI_IN: self->midiIn = static_cast<const LV2_Atom_Sequence*>(data); break;
        case PORT_ALGORITHM_TYPE: self->algorithmType = static_cast<const float*>(data); break;
        case PORT_PARAM_1: self->param1 = static_cast<const float*>(data); break;
        case PORT_PARAM_2: self->param2 = static_cast<const float*>(data); break;
        case PORT_PARAM_3: self->param3 = static_cast<const float*>(data); break;
        case PORT_ENVELOPE_ATTACK: self->attack = static_cast<const float*>(data); break;
        case PORT_ENVELOPE_RELEASE: self->release = static_cast<const float*>(data); break;
        case PORT_REVERB_SIZE: self->reverbSize = static_cast<const float*>(data); break;
        case PORT_REVERB_LEVEL: self->reverbLevel = static_cast<const float*>(data); break;
        case PORT_MASTER_GAIN: self->masterGain = static_cast<const float*>(data); break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    auto* self = static_cast<flues::disyn::DisynLV2*>(instance);
    if (!self) {
        return;
    }
    self->engine = std::make_unique<flues::disyn::DisynEngine>(self->sampleRate);
    self->currentNote = -1;
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    using namespace flues::disyn;
    auto* self = static_cast<DisynLV2*>(instance);
    if (!self || !self->audioOutLeft) {
        return;
    }

    apply_parameters(self);

    float* outLeft = self->audioOutLeft;
    float* outRight = self->audioOutRight ? self->audioOutRight : self->audioOutLeft;
    std::memset(outLeft, 0, n_samples * sizeof(float));
    if (outRight != outLeft) {
        std::memset(outRight, 0, n_samples * sizeof(float));
    }

    uint32_t frame = 0;

    if (self->midiIn && self->midiIn->atom.type == self->atomSequenceUrid) {
        LV2_ATOM_SEQUENCE_FOREACH(self->midiIn, ev) {
            const uint32_t eventFrame = ev->time.frames >= 0
                ? static_cast<uint32_t>(ev->time.frames)
                : 0u;

            if (frame < eventFrame) {
                const uint32_t limit = std::min(eventFrame, n_samples);
                for (; frame < limit; ++frame) {
                    const AlgorithmOutput sample = self->engine->process();
                    outLeft[frame] = sample.primary;
                    outRight[frame] = sample.secondary;
                }
            }

            if (ev->body.type == self->midiEventUrid) {
                const uint8_t* msg = reinterpret_cast<const uint8_t*>(ev + 1);
                handle_midi(self, msg, ev->body.size);
            }
        }
    }

    for (; frame < n_samples; ++frame) {
        const AlgorithmOutput sample = self->engine->process();
        outLeft[frame] = sample.primary;
        outRight[frame] = sample.secondary;
    }
}

static void deactivate(LV2_Handle) {}

static const void* extension_data(const char*) {
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    DISYN_URI,
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
