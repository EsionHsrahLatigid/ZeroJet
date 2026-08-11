#pragma once

#include "zerojet/ZeroJetDspPrimitives.h"

#include <array>
#include <memory>

namespace zerojet
{

/** Realtime-safe parameter set for the ZeroJet stereo effect. */
struct ZeroJetParameters
{
    float rate = 0.24f;
    float depth = 0.68f;
    float center = 0.36f;
    float feedback = 0.22f;
    float color = 0.55f;
    float zero = 0.62f;
    float mix = 0.78f;
};

/** Dual-head fractional-delay through-zero-style flanger. */
class ZeroJetEngine
{
public:
    ZeroJetEngine();

    /** Sets the sample rate and rebuilds coefficients; invalid rates fall back to 44.1 kHz. */
    void prepare (double sampleRate) noexcept;

    /** Clears hold, filter, and deterministic state. */
    void reset() noexcept;

    /** Clamps and applies all public parameters. */
    void setParameters (const ZeroJetParameters& parameters) noexcept;

    /** Processes one stereo input frame and returns finite output bounded to +/-0.98. */
    [[nodiscard]] StereoFrame processSample (float inputLeft, float inputRight) noexcept;

    /** Processes stereo buffers in-place. Null buffers and non-positive sizes are ignored. */
    void process (float* left, float* right, int numSamples) noexcept;

private:
    static constexpr int maxDelaySamples = 65536;
    using DelayBuffer = std::array<float, maxDelaySamples>;

    struct ClampedParameters
    {
        float rate = 0.24f;
        float depth = 0.68f;
        float center = 0.36f;
        float feedback = 0.22f;
        float color = 0.55f;
        float zero = 0.62f;
        float mix = 0.78f;
    };

    [[nodiscard]] float processChannel (float input, DelayBuffer& buffer, float& feedbackState, float& dampState, float phaseOffset) noexcept;
    [[nodiscard]] float readDelay (const DelayBuffer& buffer, float delaySamples) const noexcept;
    [[nodiscard]] StereoFrame sanitizeFrame (float left, float right) const noexcept;

    ClampedParameters params;
    double sampleRate = 44100.0;
    int writeIndex = 0;
    float lfoPhase = 0.0f;
    float leftFeedback = 0.0f;
    float rightFeedback = 0.0f;
    float leftDamp = 0.0f;
    float rightDamp = 0.0f;
    std::unique_ptr<DelayBuffer> leftDelay;
    std::unique_ptr<DelayBuffer> rightDelay;
};

} // namespace zerojet
