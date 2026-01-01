#pragma once

#include <array>
#include <cstdint>
#include <cmath>

#include "DspUtils.hpp"

class LstmModule {
public:
    static constexpr int kHiddenSize = 16;
    static constexpr uint32_t kMaxHistory = 512;

    LstmModule() {
        initWeights();
        reset();
    }

    void reset() {
        hidden_.fill(0.0f);
        cell_.fill(0.0f);
        historyIndex_ = 0;
        historySize_ = 0;
    }

    void setActiveSize(uint32_t size) {
        if (size < 1) size = 1;
        if (size > kHiddenSize) size = kHiddenSize;
        if (size == activeSize_) {
            return;
        }
        activeSize_ = size;
        initWeights();
        reset();
    }

    float process(float input) {
        const std::array<float, kHiddenSize> prevHidden = hidden_;
        const std::array<float, kHiddenSize> prevCell = cell_;

        Step step{};
        step.x = input;
        step.h_prev = prevHidden;
        step.c_prev = prevCell;

        for (int i = 0; i < activeSize_; ++i) {
            const float h = hidden_[i];
            const float iGate = sigmoidf(w_xi_[i] * input + w_hi_[i] * h + b_i_[i]);
            const float fGate = sigmoidf(w_xf_[i] * input + w_hf_[i] * h + b_f_[i]);
            const float oGate = sigmoidf(w_xo_[i] * input + w_ho_[i] * h + b_o_[i]);
            const float gGate = tanhf_fast(w_xg_[i] * input + w_hg_[i] * h + b_g_[i]);
            const float nextCell = fGate * cell_[i] + iGate * gGate;
            cell_[i] = clampf(nextCell, -2.0f, 2.0f);
            hidden_[i] = oGate * tanhf_fast(cell_[i]);

            step.i[i] = iGate;
            step.f[i] = fGate;
            step.o[i] = oGate;
            step.g[i] = gGate;
            step.h[i] = hidden_[i];
            step.c[i] = cell_[i];
        }

        float output = out_b_;
        for (int i = 0; i < activeSize_; ++i) {
            output += out_w_[i] * hidden_[i];
        }
        output = clampf(output, -1.5f, 1.5f);
        step.y = output;
        recordStep(step);
        return output;
    }

    const std::array<float, kHiddenSize>& hidden() const {
        return hidden_;
    }

    void trainReadout(const std::array<float, kHiddenSize>& hidden, float error, float rate) {
        const float lr = clampf(rate, 0.0f, 0.1f);
        out_b_ = clampf(out_b_ + lr * error, -2.0f, 2.0f);
        for (int i = 0; i < activeSize_; ++i) {
            const float updated = out_w_[i] + lr * error * hidden[i];
            out_w_[i] = clampf(updated, -2.0f, 2.0f);
        }
    }

    bool getDelayedPrediction(uint32_t delay, float* prediction) const {
        if (!prediction || historySize_ <= delay) {
            return false;
        }
        const uint32_t index = historyIndex_ + kMaxHistory - 1 - delay;
        const uint32_t offset = index % kMaxHistory;
        *prediction = history_[offset].y;
        return true;
    }

    bool trainFromDelay(uint32_t delay, float target, uint32_t bpttLength, float rate, float gradClip) {
        if (historySize_ <= delay) {
            return false;
        }
        if (bpttLength == 0) {
            return false;
        }

        const uint32_t maxLen = historySize_ - delay;
        const uint32_t steps = bpttLength > maxLen ? maxLen : bpttLength;
        if (steps == 0) {
            return false;
        }

        const uint32_t startIndex = (historyIndex_ + kMaxHistory - 1 - delay) % kMaxHistory;
        const Step& startStep = history_[startIndex];

        const float lr = clampf(rate, 0.0f, 0.1f);
        const float clip = clampf(gradClip, 0.1f, 20.0f);
        const float error = clampf(startStep.y - target, -clip, clip);
        const float dy = error;

        std::array<float, kHiddenSize> dhNext{};
        std::array<float, kHiddenSize> dcNext{};
        for (int i = 0; i < activeSize_; ++i) {
            dhNext[i] = dy * out_w_[i];
            out_w_[i] = clampf(out_w_[i] - lr * dy * startStep.h[i], -2.0f, 2.0f);
        }
        out_b_ = clampf(out_b_ - lr * dy, -2.0f, 2.0f);

        for (uint32_t stepIndex = 0; stepIndex < steps; ++stepIndex) {
            const uint32_t idx = (startIndex + kMaxHistory - stepIndex) % kMaxHistory;
            const Step& step = history_[idx];

            for (int i = 0; i < activeSize_; ++i) {
                float dh = clampf(dhNext[i], -clip, clip);
                float dc = clampf(dcNext[i], -clip, clip);

                const float tanhC = tanhf_fast(step.c[i]);
                const float oGate = step.o[i];
                const float iGate = step.i[i];
                const float fGate = step.f[i];
                const float gGate = step.g[i];

                const float dcTotal = dc + dh * oGate * (1.0f - tanhC * tanhC);
                const float di = dcTotal * gGate * iGate * (1.0f - iGate);
                const float df = dcTotal * step.c_prev[i] * fGate * (1.0f - fGate);
                const float doGate = dh * tanhC * oGate * (1.0f - oGate);
                const float dg = dcTotal * iGate * (1.0f - gGate * gGate);

                const float w_hi = w_hi_[i];
                const float w_hf = w_hf_[i];
                const float w_ho = w_ho_[i];
                const float w_hg = w_hg_[i];

                w_xi_[i] = clampf(w_xi_[i] - lr * di * step.x, -2.0f, 2.0f);
                w_hi_[i] = clampf(w_hi_[i] - lr * di * step.h_prev[i], -2.0f, 2.0f);
                b_i_[i] = clampf(b_i_[i] - lr * di, -2.0f, 2.0f);

                w_xf_[i] = clampf(w_xf_[i] - lr * df * step.x, -2.0f, 2.0f);
                w_hf_[i] = clampf(w_hf_[i] - lr * df * step.h_prev[i], -2.0f, 2.0f);
                b_f_[i] = clampf(b_f_[i] - lr * df, -2.0f, 2.0f);

                w_xo_[i] = clampf(w_xo_[i] - lr * doGate * step.x, -2.0f, 2.0f);
                w_ho_[i] = clampf(w_ho_[i] - lr * doGate * step.h_prev[i], -2.0f, 2.0f);
                b_o_[i] = clampf(b_o_[i] - lr * doGate, -2.0f, 2.0f);

                w_xg_[i] = clampf(w_xg_[i] - lr * dg * step.x, -2.0f, 2.0f);
                w_hg_[i] = clampf(w_hg_[i] - lr * dg * step.h_prev[i], -2.0f, 2.0f);
                b_g_[i] = clampf(b_g_[i] - lr * dg, -2.0f, 2.0f);

                const float dhPrev = di * w_hi + df * w_hf + doGate * w_ho + dg * w_hg;
                const float dcPrev = dcTotal * fGate;

                dhNext[i] = clampf(dhPrev, -clip, clip);
                dcNext[i] = clampf(dcPrev, -clip, clip);
            }
        }

        return true;
    }

private:
    struct Step {
        float x = 0.0f;
        float y = 0.0f;
        std::array<float, kHiddenSize> h{};
        std::array<float, kHiddenSize> c{};
        std::array<float, kHiddenSize> h_prev{};
        std::array<float, kHiddenSize> c_prev{};
        std::array<float, kHiddenSize> i{};
        std::array<float, kHiddenSize> f{};
        std::array<float, kHiddenSize> o{};
        std::array<float, kHiddenSize> g{};
    };

    std::array<float, kHiddenSize> w_xi_{};
    std::array<float, kHiddenSize> w_hi_{};
    std::array<float, kHiddenSize> b_i_{};

    std::array<float, kHiddenSize> w_xf_{};
    std::array<float, kHiddenSize> w_hf_{};
    std::array<float, kHiddenSize> b_f_{};

    std::array<float, kHiddenSize> w_xo_{};
    std::array<float, kHiddenSize> w_ho_{};
    std::array<float, kHiddenSize> b_o_{};

    std::array<float, kHiddenSize> w_xg_{};
    std::array<float, kHiddenSize> w_hg_{};
    std::array<float, kHiddenSize> b_g_{};

    std::array<float, kHiddenSize> out_w_{};
    float out_b_ = 0.0f;

    std::array<float, kHiddenSize> hidden_{};
    std::array<float, kHiddenSize> cell_{};

    std::array<Step, kMaxHistory> history_{};
    uint32_t historyIndex_ = 0;
    uint32_t historySize_ = 0;
    uint32_t activeSize_ = 8;

    uint32_t rng_ = 0x12345678u;

    float nextRand() {
        rng_ = rng_ * 1664525u + 1013904223u;
        const uint32_t value = (rng_ >> 8) & 0x00FFFFFFu;
        return static_cast<float>(value) / static_cast<float>(0x00FFFFFFu);
    }

    void initWeights() {
        rng_ = 0x12345678u;
        initArray(w_xi_, 0.2f);
        initArray(w_hi_, 0.2f);
        initArray(b_i_, 0.0f);

        initArray(w_xf_, 0.2f);
        initArray(w_hf_, 0.2f);
        initArray(b_f_, 0.5f);

        initArray(w_xo_, 0.2f);
        initArray(w_ho_, 0.2f);
        initArray(b_o_, 0.0f);

        initArray(w_xg_, 0.2f);
        initArray(w_hg_, 0.2f);
        initArray(b_g_, 0.0f);

        initArray(out_w_, 0.05f);
        out_b_ = 0.0f;
    }

    void initArray(std::array<float, kHiddenSize>& arr, float scale) {
        for (int i = 0; i < kHiddenSize; ++i) {
            const float r = nextRand() * 2.0f - 1.0f;
            arr[i] = r * scale;
        }
    }

    void recordStep(const Step& step) {
        history_[historyIndex_] = step;
        historyIndex_ = (historyIndex_ + 1) % kMaxHistory;
        if (historySize_ < kMaxHistory) {
            ++historySize_;
        }
    }
};
