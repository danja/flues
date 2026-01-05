// speculate_plugin.cpp - Spectral modulation LV2 effect

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>

#define SPECULATE_URI "https://danja.github.io/flues/plugins/speculate"

static constexpr int kFftSize = 1024;
static constexpr int kHopSize = kFftSize / 2;
static constexpr int kBins = kFftSize / 2;
static constexpr float kTwoPi = 6.283185307179586f;
static constexpr int kMaxShift = kFftSize / 4;

enum PortIndex {
    PORT_CONTROL_IN = 0,
    PORT_IN_L = 1,
    PORT_IN_R = 2,
    PORT_OUT_L = 3,
    PORT_OUT_R = 4,
    PORT_DRY_WET = 5,
    PORT_SHIFT = 6,
    PORT_BLUR = 7,
    PORT_FREEZE = 8,
    PORT_MOD_DEPTH = 9,
    PORT_MOD_FACTOR = 10
};

struct SpeculateUrids {
    LV2_URID atom_Sequence;
    LV2_URID atom_Object;
    LV2_URID atom_Blank;
    LV2_URID atom_Float;
    LV2_URID time_Position;
    LV2_URID time_beatsPerMinute;
};

struct Speculate {
    const LV2_Atom_Sequence* controlIn;
    const float* inL;
    const float* inR;
    float* outL;
    float* outR;
    const float* dryWet;
    const float* shift;
    const float* blur;
    const float* freeze;
    const float* modDepth;
    const float* modFactor;

    double sampleRate;
    float tempoBpm;
    float modPhase;

    LV2_URID_Map* map;
    SpeculateUrids urids;

    float inputRing[2][kFftSize];
    float outputRing[2][kFftSize];
    float window[kFftSize];
    float frameRe[2][kFftSize];
    float frameIm[2][kFftSize];
    float mag[2][kBins + 1];
    float phase[2][kBins + 1];
    float scratchRe[2][kBins + 1];
    float scratchIm[2][kBins + 1];
    float prevMag[2][kBins + 1];
    float magScratch[2][kBins + 1];

    uint32_t writeIndex;
    uint32_t hopCounter;
    uint64_t sampleCount;
};

static inline float clampf(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static void fft(float* re, float* im, int n, bool inverse) {
    int j = 0;
    for (int i = 1; i < n; ++i) {
        int bit = n >> 1;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1) {
        const float angle = (inverse ? kTwoPi : -kTwoPi) / static_cast<float>(len);
        const float wlenRe = std::cos(angle);
        const float wlenIm = std::sin(angle);
        for (int i = 0; i < n; i += len) {
            float wRe = 1.0f;
            float wIm = 0.0f;
            for (int k = 0; k < len / 2; ++k) {
                const int u = i + k;
                const int v = u + len / 2;
                const float vRe = re[v] * wRe - im[v] * wIm;
                const float vIm = re[v] * wIm + im[v] * wRe;
                const float uRe = re[u];
                const float uIm = im[u];
                re[u] = uRe + vRe;
                im[u] = uIm + vIm;
                re[v] = uRe - vRe;
                im[v] = uIm - vIm;
                const float nextRe = wRe * wlenRe - wIm * wlenIm;
                const float nextIm = wRe * wlenIm + wIm * wlenRe;
                wRe = nextRe;
                wIm = nextIm;
            }
        }
    }

    if (inverse) {
        const float scale = 1.0f / static_cast<float>(n);
        for (int i = 0; i < n; ++i) {
            re[i] *= scale;
            im[i] *= scale;
        }
    }
}

static void update_window(Speculate* self) {
    for (int i = 0; i < kFftSize; ++i) {
        const float phase = static_cast<float>(i) / static_cast<float>(kFftSize);
        const float hann = 0.5f - 0.5f * std::cos(kTwoPi * phase);
        self->window[i] = std::sqrt(hann);
    }
}

static void process_spectrum(Speculate* self, int channel, float shiftAmount,
                             float blurAmount, float freezeAmount) {
    for (int k = 0; k <= kBins; ++k) {
        const float re = self->frameRe[channel][k];
        const float im = self->frameIm[channel][k];
        self->mag[channel][k] = std::sqrt(re * re + im * im);
        self->phase[channel][k] = std::atan2(im, re);
    }

    if (blurAmount > 0.0001f) {
        for (int k = 0; k <= kBins; ++k) {
            self->magScratch[channel][k] = self->mag[channel][k];
        }
        for (int k = 0; k <= kBins; ++k) {
            const float prev = (k > 0) ? self->magScratch[channel][k - 1] : self->magScratch[channel][k];
            const float next = (k < kBins) ? self->magScratch[channel][k + 1] : self->magScratch[channel][k];
            const float avg = (prev + self->magScratch[channel][k] + next) * (1.0f / 3.0f);
            self->mag[channel][k] = self->magScratch[channel][k] +
                (avg - self->magScratch[channel][k]) * blurAmount;
        }
    }

    if (freezeAmount > 0.0001f) {
        const float wet = clampf(freezeAmount, 0.0f, 1.0f);
        const float dry = 1.0f - wet;
        for (int k = 0; k <= kBins; ++k) {
            self->mag[channel][k] = self->mag[channel][k] * dry + self->prevMag[channel][k] * wet;
        }
    }

    const float freezeStore = clampf(freezeAmount, 0.0f, 1.0f);
    if (freezeStore < 0.999f) {
        for (int k = 0; k <= kBins; ++k) {
            self->prevMag[channel][k] = self->mag[channel][k];
        }
    }

    const int shiftBins = static_cast<int>(std::lround(shiftAmount * static_cast<float>(kMaxShift)));
    for (int k = 0; k <= kBins; ++k) {
        self->scratchRe[channel][k] = 0.0f;
        self->scratchIm[channel][k] = 0.0f;
    }
    for (int k = 0; k <= kBins; ++k) {
        const int dest = k + shiftBins;
        if (dest < 0 || dest > kBins) {
            continue;
        }
        const float mag = self->mag[channel][k];
        const float phase = self->phase[channel][k];
        self->scratchRe[channel][dest] += mag * std::cos(phase);
        self->scratchIm[channel][dest] += mag * std::sin(phase);
    }

    self->frameRe[channel][0] = self->scratchRe[channel][0];
    self->frameIm[channel][0] = 0.0f;
    for (int k = 1; k < kBins; ++k) {
        const float re = self->scratchRe[channel][k];
        const float im = self->scratchIm[channel][k];
        self->frameRe[channel][k] = re;
        self->frameIm[channel][k] = im;
        self->frameRe[channel][kFftSize - k] = re;
        self->frameIm[channel][kFftSize - k] = -im;
    }
    self->frameRe[channel][kBins] = self->scratchRe[channel][kBins];
    self->frameIm[channel][kBins] = 0.0f;
}

static LV2_Handle instantiate(const LV2_Descriptor* descriptor,
                              double sampleRate,
                              const char* bundlePath,
                              const LV2_Feature* const* features) {
    (void)descriptor;
    (void)bundlePath;
    (void)features;

    Speculate* self = new Speculate();
    self->sampleRate = sampleRate;
    self->tempoBpm = 120.0f;
    self->modPhase = 0.0f;
    self->map = nullptr;
    std::memset(&self->urids, 0, sizeof(self->urids));

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
    }
    std::memset(self->inputRing, 0, sizeof(self->inputRing));
    std::memset(self->outputRing, 0, sizeof(self->outputRing));
    std::memset(self->frameRe, 0, sizeof(self->frameRe));
    std::memset(self->frameIm, 0, sizeof(self->frameIm));
    std::memset(self->mag, 0, sizeof(self->mag));
    std::memset(self->phase, 0, sizeof(self->phase));
    std::memset(self->scratchRe, 0, sizeof(self->scratchRe));
    std::memset(self->scratchIm, 0, sizeof(self->scratchIm));
    std::memset(self->prevMag, 0, sizeof(self->prevMag));
    std::memset(self->magScratch, 0, sizeof(self->magScratch));
    self->writeIndex = 0;
    self->hopCounter = 0;
    self->sampleCount = 0;
    update_window(self);
    return static_cast<LV2_Handle>(self);
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    Speculate* self = static_cast<Speculate*>(instance);
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
        case PORT_DRY_WET:
            self->dryWet = static_cast<const float*>(data);
            break;
        case PORT_SHIFT:
            self->shift = static_cast<const float*>(data);
            break;
        case PORT_BLUR:
            self->blur = static_cast<const float*>(data);
            break;
        case PORT_FREEZE:
            self->freeze = static_cast<const float*>(data);
            break;
        case PORT_MOD_DEPTH:
            self->modDepth = static_cast<const float*>(data);
            break;
        case PORT_MOD_FACTOR:
            self->modFactor = static_cast<const float*>(data);
            break;
        default:
            break;
    }
}

static void activate(LV2_Handle instance) {
    Speculate* self = static_cast<Speculate*>(instance);
    std::memset(self->inputRing, 0, sizeof(self->inputRing));
    std::memset(self->outputRing, 0, sizeof(self->outputRing));
    std::memset(self->prevMag, 0, sizeof(self->prevMag));
    self->writeIndex = 0;
    self->hopCounter = 0;
    self->sampleCount = 0;
    self->tempoBpm = 120.0f;
    self->modPhase = 0.0f;
}

static void run(LV2_Handle instance, uint32_t nSamples) {
    Speculate* self = static_cast<Speculate*>(instance);

    const float wet = clampf(self->dryWet ? *self->dryWet : 0.0f, 0.0f, 1.0f);
    const float baseShift = clampf(self->shift ? *self->shift : 0.0f, -0.2f, 0.2f);
    const float blur = clampf(self->blur ? *self->blur : 0.0f, 0.0f, 1.0f);
    const float freeze = clampf(self->freeze ? *self->freeze : 0.0f, 0.0f, 1.0f);
    const float modDepth = clampf(self->modDepth ? *self->modDepth : 0.0f, 0.0f, 1.0f);
    const float modFactor = clampf(self->modFactor ? *self->modFactor : 1.0f, 0.02f, 16.0f);

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
            }
        }
    }

    const float lfoHz = (self->tempoBpm / 60.0f) * modFactor;
    const float phaseInc = kTwoPi * lfoHz / static_cast<float>(self->sampleRate);

    for (uint32_t i = 0; i < nSamples; ++i) {
        const float inL = self->inL ? self->inL[i] : 0.0f;
        const float inR = self->inR ? self->inR[i] : 0.0f;

        self->inputRing[0][self->writeIndex] = inL;
        self->inputRing[1][self->writeIndex] = inR;

        const float wetL = self->outputRing[0][self->writeIndex];
        const float wetR = self->outputRing[1][self->writeIndex];
        self->outputRing[0][self->writeIndex] = 0.0f;
        self->outputRing[1][self->writeIndex] = 0.0f;

        const float dryMix = 1.0f - wet;
        if (self->outL) {
            self->outL[i] = inL * dryMix + wetL * wet;
        }
        if (self->outR) {
            self->outR[i] = inR * dryMix + wetR * wet;
        }

        self->writeIndex = (self->writeIndex + 1) % kFftSize;
        self->hopCounter++;
        self->sampleCount++;
        self->modPhase += phaseInc;
        if (self->modPhase >= kTwoPi) {
            self->modPhase -= kTwoPi;
        }

        if (self->sampleCount < static_cast<uint64_t>(kFftSize)) {
            continue;
        }

        if (self->hopCounter >= kHopSize) {
            self->hopCounter = 0;
            const uint32_t start = self->writeIndex;
            const float modShift = baseShift + std::sin(self->modPhase) * (modDepth * 0.2f);
            const float shift = clampf(modShift, -0.2f, 0.2f);
            for (int ch = 0; ch < 2; ++ch) {
                for (int n = 0; n < kFftSize; ++n) {
                    const uint32_t idx = (start + n) % kFftSize;
                    const float sample = self->inputRing[ch][idx];
                    self->frameRe[ch][n] = sample * self->window[n];
                    self->frameIm[ch][n] = 0.0f;
                }

                fft(self->frameRe[ch], self->frameIm[ch], kFftSize, false);
                process_spectrum(self, ch, shift, blur, freeze);
                fft(self->frameRe[ch], self->frameIm[ch], kFftSize, true);

                for (int n = 0; n < kFftSize; ++n) {
                    const uint32_t idx = (start + n) % kFftSize;
                    self->outputRing[ch][idx] += self->frameRe[ch][n] * self->window[n];
                }
            }
        }
    }
}

static void deactivate(LV2_Handle instance) {
    (void)instance;
}

static void cleanup(LV2_Handle instance) {
    delete static_cast<Speculate*>(instance);
}

static const void* extension_data(const char* uri) {
    (void)uri;
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    SPECULATE_URI,
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
