// memone_plugin.cpp - LV2 LSTM time-series predictor

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "MemoneEngine.hpp"

#define MEMONE_URI "https://danja.github.io/flues/plugins/memone"

enum PortIndex {
    PORT_CONTROL = 0,
    PORT_AUDIO_IN_L = 1,
    PORT_AUDIO_IN_R = 2,
    PORT_AUDIO_OUT_L = 3,
    PORT_AUDIO_OUT_R = 4,
    PORT_BPM = 5,
    PORT_BEATS_WARMUP = 6,
    PORT_PREDICT_GAIN = 7,
    PORT_PREDICT_HORIZON = 8,
    PORT_LEARNING_RATE = 9,
    PORT_BPTT_LENGTH = 10,
    PORT_GRAD_CLIP = 11,
    PORT_HIDDEN_SIZE = 12,
    PORT_RESET = 13,
    PORT_LR_CLAMP = 14,
    PORT_LR_MIN = 15,
    PORT_LR_MAX = 16,
    PORT_STATUS = 17
};

struct MemoneLV2 {
    const LV2_Atom_Sequence* control = nullptr;
    const float* inputL = nullptr;
    const float* inputR = nullptr;
    float* outputL = nullptr;
    float* outputR = nullptr;
    const float* bpmPort = nullptr;
    const float* warmupPort = nullptr;
    const float* gainPort = nullptr;
    const float* horizonPort = nullptr;
    const float* learningRatePort = nullptr;
    const float* bpttPort = nullptr;
    const float* gradClipPort = nullptr;
    const float* resetPort = nullptr;
    const float* lrClampPort = nullptr;
    const float* lrMinPort = nullptr;
    const float* lrMaxPort = nullptr;
    const float* hiddenSizePort = nullptr;
    float* statusPort = nullptr;

    LV2_URID_Map* map = nullptr;
    LV2_URID atomObjectUrid = 0;
    LV2_URID atomSequenceUrid = 0;
    LV2_URID atomFloatUrid = 0;
    LV2_URID timePositionUrid = 0;
    LV2_URID timeBeatsPerMinuteUrid = 0;
    LV2_URID timeSpeedUrid = 0;

    MemoneEngine engine{};
    float lastReset = 0.0f;
};

static LV2_Handle instantiate(
    const LV2_Descriptor*,
    double sample_rate,
    const char*,
    const LV2_Feature* const* features
) {
    MemoneLV2* self = new MemoneLV2();
    for (int i = 0; features[i]; ++i) {
        if (!std::strcmp(features[i]->URI, LV2_URID__map)) {
            self->map = static_cast<LV2_URID_Map*>(features[i]->data);
        }
    }

    if (!self->map) {
        delete self;
        return nullptr;
    }

    self->atomObjectUrid = self->map->map(self->map->handle, LV2_ATOM__Object);
    self->atomSequenceUrid = self->map->map(self->map->handle, LV2_ATOM__Sequence);
    self->atomFloatUrid = self->map->map(self->map->handle, LV2_ATOM__Float);
    self->timePositionUrid = self->map->map(self->map->handle, "http://lv2plug.in/ns/ext/time#Position");
    self->timeBeatsPerMinuteUrid = self->map->map(self->map->handle, "http://lv2plug.in/ns/ext/time#beatsPerMinute");
    self->timeSpeedUrid = self->map->map(self->map->handle, "http://lv2plug.in/ns/ext/time#speed");

    self->engine.prepare(sample_rate);

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    MemoneLV2* self = static_cast<MemoneLV2*>(instance);
    switch (port) {
        case PORT_CONTROL:
            self->control = static_cast<const LV2_Atom_Sequence*>(data);
            break;
        case PORT_AUDIO_IN_L:
            self->inputL = static_cast<const float*>(data);
            break;
        case PORT_AUDIO_IN_R:
            self->inputR = static_cast<const float*>(data);
            break;
        case PORT_AUDIO_OUT_L:
            self->outputL = static_cast<float*>(data);
            break;
        case PORT_AUDIO_OUT_R:
            self->outputR = static_cast<float*>(data);
            break;
        case PORT_BPM:
            self->bpmPort = static_cast<const float*>(data);
            break;
        case PORT_BEATS_WARMUP:
            self->warmupPort = static_cast<const float*>(data);
            break;
        case PORT_PREDICT_GAIN:
            self->gainPort = static_cast<const float*>(data);
            break;
        case PORT_PREDICT_HORIZON:
            self->horizonPort = static_cast<const float*>(data);
            break;
        case PORT_LEARNING_RATE:
            self->learningRatePort = static_cast<const float*>(data);
            break;
        case PORT_BPTT_LENGTH:
            self->bpttPort = static_cast<const float*>(data);
            break;
        case PORT_GRAD_CLIP:
            self->gradClipPort = static_cast<const float*>(data);
            break;
        case PORT_HIDDEN_SIZE:
            self->hiddenSizePort = static_cast<const float*>(data);
            break;
        case PORT_RESET:
            self->resetPort = static_cast<const float*>(data);
            break;
        case PORT_LR_CLAMP:
            self->lrClampPort = static_cast<const float*>(data);
            break;
        case PORT_LR_MIN:
            self->lrMinPort = static_cast<const float*>(data);
            break;
        case PORT_LR_MAX:
            self->lrMaxPort = static_cast<const float*>(data);
            break;
        case PORT_STATUS:
            self->statusPort = static_cast<float*>(data);
            break;
        default:
            break;
    }
}

static void activate(LV2_Handle instance) {
    MemoneLV2* self = static_cast<MemoneLV2*>(instance);
    self->engine.reset();
    self->lastReset = 0.0f;
}

static void run(LV2_Handle instance, uint32_t nframes) {
    MemoneLV2* self = static_cast<MemoneLV2*>(instance);
    if (!self->outputL || !self->outputR) {
        return;
    }

    float manualBpm = self->bpmPort ? *self->bpmPort : 120.0f;
    float warmupBeats = self->warmupPort ? *self->warmupPort : 4.0f;
    float predictGain = self->gainPort ? *self->gainPort : 1.0f;
    float predictHorizon = self->horizonPort ? *self->horizonPort : 64.0f;
    float learningRate = self->learningRatePort ? *self->learningRatePort : 0.0005f;
    float bpttLength = self->bpttPort ? *self->bpttPort : 32.0f;
    float gradClip = self->gradClipPort ? *self->gradClipPort : 5.0f;
    float resetValue = self->resetPort ? *self->resetPort : 0.0f;
    float lrClamp = self->lrClampPort ? *self->lrClampPort : 1.0f;
    float lrMin = self->lrMinPort ? *self->lrMinPort : 0.0001f;
    float lrMax = self->lrMaxPort ? *self->lrMaxPort : 0.005f;
    float hiddenSize = self->hiddenSizePort ? *self->hiddenSizePort : 8.0f;

    bool timeSeen = false;
    float hostBpm = 0.0f;
    bool transportPlaying = true;

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

            if (!obj) {
                continue;
            }

            timeSeen = true;
            const LV2_Atom_Float* bpmAtom = nullptr;
            lv2_atom_object_get(obj, self->timeBeatsPerMinuteUrid, &bpmAtom, 0);
            if (bpmAtom && bpmAtom->atom.type == self->atomFloatUrid) {
                hostBpm = bpmAtom->body;
            }

            const LV2_Atom_Float* speedAtom = nullptr;
            lv2_atom_object_get(obj, self->timeSpeedUrid, &speedAtom, 0);
            if (speedAtom && speedAtom->atom.type == self->atomFloatUrid) {
                transportPlaying = speedAtom->body > 0.0f;
            }
        }
    }

    if (resetValue > 0.5f && self->lastReset <= 0.5f) {
        self->engine.reset();
    }
    self->lastReset = resetValue;

    self->engine.setManualBpm(manualBpm);
    self->engine.setWarmupBeats(warmupBeats);
    self->engine.setPredictGain(predictGain);
    self->engine.setPredictionHorizon(predictHorizon);
    self->engine.setLearningRate(learningRate);
    self->engine.setBpttLength(bpttLength);
    self->engine.setGradClip(gradClip);
    self->engine.setHiddenSize(hiddenSize);
    self->engine.setLearningRateClamp(lrClamp > 0.5f, lrMin, lrMax);
    self->engine.setTransport(hostBpm, timeSeen, transportPlaying);

    static thread_local std::vector<float> monoBuffer;
    if (monoBuffer.size() < nframes) {
        monoBuffer.resize(nframes);
    }

    for (uint32_t i = 0; i < nframes; ++i) {
        const float inL = self->inputL ? self->inputL[i] : 0.0f;
        const float inR = self->inputR ? self->inputR[i] : 0.0f;
        monoBuffer[i] = 0.5f * (inL + inR);
    }

    self->engine.process(monoBuffer.data(), self->outputL, nframes);
    std::memcpy(self->outputR, self->outputL, sizeof(float) * nframes);

    if (self->statusPort) {
        *self->statusPort = self->engine.isWarmupComplete() ? 1.0f : 0.0f;
    }
}

static void deactivate(LV2_Handle) {}

static void cleanup(LV2_Handle instance) {
    MemoneLV2* self = static_cast<MemoneLV2*>(instance);
    delete self;
}

static const void* extension_data(const char*) { return nullptr; }

static const LV2_Descriptor descriptor = {
    MEMONE_URI,
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
    return index == 0 ? &descriptor : nullptr;
}
