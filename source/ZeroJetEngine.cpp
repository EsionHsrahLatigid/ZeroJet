#include "zerojet/ZeroJetEngine.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace zerojet
{
namespace
{
constexpr float ceiling = 0.98f;

[[nodiscard]] float sanitizeAudio (float value) noexcept
{
    return clampFinite (value, -8.0f, 8.0f, 0.0f);
}

[[nodiscard]] float wrapUnit (float value) noexcept
{
    value -= std::floor (value);
    return value < 0.0f ? value + 1.0f : value;
}
} // namespace

ZeroJetEngine::ZeroJetEngine()
{
    prepare (44100.0);
    reset();
}

void ZeroJetEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 1.0 ? newSampleRate : 44100.0;
    reset();
}

void ZeroJetEngine::reset() noexcept
{
    writeIndex = 0;
    lfoPhase = 0.0f;
    leftFeedback = 0.0f;
    rightFeedback = 0.0f;
    leftDamp = 0.0f;
    rightDamp = 0.0f;
    leftDelay.fill (0.0f);
    rightDelay.fill (0.0f);
}

void ZeroJetEngine::setParameters (const ZeroJetParameters& parameters) noexcept
{
    params.rate = clampFinite (parameters.rate, 0.0f, 1.0f, ZeroJetParameters {}.rate);
    params.depth = clampFinite (parameters.depth, 0.0f, 1.0f, ZeroJetParameters {}.depth);
    params.center = clampFinite (parameters.center, 0.0f, 1.0f, ZeroJetParameters {}.center);
    params.feedback = clampFinite (parameters.feedback, -0.95f, 0.95f, ZeroJetParameters {}.feedback);
    params.color = clampFinite (parameters.color, 0.0f, 1.0f, ZeroJetParameters {}.color);
    params.zero = clampFinite (parameters.zero, 0.0f, 1.0f, ZeroJetParameters {}.zero);
    params.mix = clampFinite (parameters.mix, 0.0f, 1.0f, ZeroJetParameters {}.mix);
}

StereoFrame ZeroJetEngine::processSample (float inputLeft, float inputRight) noexcept
{
    const auto dryLeft = sanitizeAudio (inputLeft);
    const auto dryRight = sanitizeAudio (inputRight);
    const auto rateHz = 0.018f * std::pow (360.0f, params.rate);
    lfoPhase = wrapUnit (lfoPhase + rateHz / static_cast<float> (sampleRate));

    const auto spread = 0.08f + params.zero * 0.34f;
    const auto wetLeft = processChannel (dryLeft, leftDelay, leftFeedback, leftDamp, spread);
    const auto wetRight = processChannel (dryRight, rightDelay, rightFeedback, rightDamp, 0.5f - spread);

    ++writeIndex;
    if (writeIndex >= maxDelaySamples)
        writeIndex = 0;

    const auto dry = 1.0f - params.mix;
    return sanitizeFrame (dryLeft * dry + wetLeft * params.mix,
                          dryRight * dry + wetRight * params.mix);
}

void ZeroJetEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto frame = processSample (left[i], right[i]);
        left[i] = frame.left;
        right[i] = frame.right;
    }
}

float ZeroJetEngine::processChannel (float input, DelayBuffer& buffer, float& feedbackState, float& dampState, float phaseOffset) noexcept
{
    const auto damping = 0.08f + params.color * 0.84f;
    dampState += damping * (feedbackState - dampState);
    const auto writeSample = sanitizeAudio (input + dampState * params.feedback);
    buffer[static_cast<std::size_t> (writeIndex)] = writeSample;

    const auto phase = wrapUnit (lfoPhase + phaseOffset);
    const auto triangle = 1.0f - std::fabs (phase * 2.0f - 1.0f);
    const auto centerMs = 0.08f + params.center * 8.5f;
    const auto depthMs = params.depth * (0.06f + params.zero * 7.8f);
    const auto base = centerMs * static_cast<float> (sampleRate) * 0.001f;
    const auto excursion = depthMs * static_cast<float> (sampleRate) * 0.001f;
    const auto headA = std::clamp (base + (triangle - 0.5f) * 2.0f * excursion, 0.25f, static_cast<float> (maxDelaySamples - 4));
    const auto headB = std::clamp (base + ((1.0f - triangle) - 0.5f) * 2.0f * excursion, 0.25f, static_cast<float> (maxDelaySamples - 4));

    const auto fade = 0.5f - 0.5f * std::cos (phase * 2.0f * std::numbers::pi_v<float>);
    const auto gainA = std::cos (fade * std::numbers::pi_v<float> * 0.5f);
    const auto gainB = std::sin (fade * std::numbers::pi_v<float> * 0.5f);
    const auto delayed = readDelay (buffer, headA) * gainA + readDelay (buffer, headB) * gainB;
    const auto polarity = params.zero >= 0.5f ? -1.0f : 1.0f;
    const auto wet = delayed + input * polarity * (params.zero - 0.5f) * 0.55f;
    feedbackState = boundedDrive (wet, 0.92f);
    return sanitizeAudio (wet);
}

float ZeroJetEngine::readDelay (const DelayBuffer& buffer, float delaySamples) const noexcept
{
    const auto safeDelay = clampFinite (delaySamples, 0.0f, static_cast<float> (maxDelaySamples - 4), 1.0f);
    auto readPosition = static_cast<float> (writeIndex) - safeDelay;
    while (readPosition < 0.0f)
        readPosition += static_cast<float> (maxDelaySamples);

    const auto index0 = static_cast<int> (std::floor (readPosition)) % maxDelaySamples;
    const auto frac = readPosition - std::floor (readPosition);
    const auto index1 = (index0 + 1) % maxDelaySamples;
    return sanitizeAudio (buffer[static_cast<std::size_t> (index0)] * (1.0f - frac)
                          + buffer[static_cast<std::size_t> (index1)] * frac);
}

StereoFrame ZeroJetEngine::sanitizeFrame (float left, float right) const noexcept
{
    auto safeLeft = boundedDrive (left, 1.03f + std::fabs (params.feedback) * 0.45f);
    auto safeRight = boundedDrive (right, 1.03f + std::fabs (params.feedback) * 0.45f);
    if (std::fabs (safeLeft) < 1.0e-20f)
        safeLeft = 0.0f;
    if (std::fabs (safeRight) < 1.0e-20f)
        safeRight = 0.0f;
    return { std::clamp (safeLeft, -ceiling, ceiling),
             std::clamp (safeRight, -ceiling, ceiling) };
}

} // namespace zerojet
