#include "zerojet/ZeroJetEngine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using zerojet::ZeroJetEngine;
using zerojet::ZeroJetParameters;

namespace
{

std::vector<float> renderTone (ZeroJetParameters params, int samples)
{
    ZeroJetEngine engine;
    engine.prepare (48000.0);
    engine.setParameters (params);
    engine.reset();

    std::vector<float> output;
    output.reserve (static_cast<std::size_t> (samples));
    for (int i = 0; i < samples; ++i)
    {
        const auto sample = std::sin (static_cast<float> (i) * 0.052f) * 0.38f
                          + std::sin (static_cast<float> (i) * 0.133f) * 0.16f;
        output.push_back (engine.processSample (sample, sample * 0.77f).left);
    }
    return output;
}

float averageDifference (const std::vector<float>& a, const std::vector<float>& b)
{
    assert (a.size() == b.size());
    float total = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i)
        total += std::fabs (a[i] - b[i]);
    return total / static_cast<float> (a.size());
}

float peak (const std::vector<float>& samples)
{
    float result = 0.0f;
    for (const auto sample : samples)
        result = std::max (result, std::fabs (sample));
    return result;
}

void testSilenceStaysSilent()
{
    ZeroJetEngine engine;
    engine.prepare (48000.0);
    engine.reset();

    for (int i = 0; i < 8192; ++i)
    {
        const auto frame = engine.processSample (0.0f, 0.0f);
        assert (std::fabs (frame.left) <= 1.0e-7f);
        assert (std::fabs (frame.right) <= 1.0e-7f);
    }
}

void testDepthAndCenterMoveDelayHeads()
{
    ZeroJetParameters shallow;
    shallow.depth = 0.05f;
    shallow.center = 0.2f;
    shallow.mix = 1.0f;

    auto deep = shallow;
    deep.depth = 1.0f;
    deep.center = 0.72f;

    const auto shallowOutput = renderTone (shallow, 12000);
    const auto deepOutput = renderTone (deep, 12000);
    assert (averageDifference (shallowOutput, deepOutput) > 0.012f);
}

void testZeroControlChangesThroughZeroBlend()
{
    ZeroJetParameters lowZero;
    lowZero.depth = 0.8f;
    lowZero.zero = 0.05f;
    lowZero.mix = 1.0f;

    auto highZero = lowZero;
    highZero.zero = 0.95f;

    const auto lowOutput = renderTone (lowZero, 12000);
    const auto highOutput = renderTone (highZero, 12000);
    assert (averageDifference (lowOutput, highOutput) > 0.02f);
}

void testSignedFeedbackAndColorChangeMotion()
{
    ZeroJetParameters dullNegative;
    dullNegative.feedback = -0.72f;
    dullNegative.color = 0.05f;
    dullNegative.mix = 1.0f;

    auto brightPositive = dullNegative;
    brightPositive.feedback = 0.72f;
    brightPositive.color = 0.95f;

    const auto dullOutput = renderTone (dullNegative, 12000);
    const auto brightOutput = renderTone (brightPositive, 12000);
    assert (averageDifference (dullOutput, brightOutput) > 0.012f);
    assert (peak (brightOutput) <= 0.9801f);
}

void testDeterministic()
{
    ZeroJetParameters params;
    params.rate = 0.37f;
    params.depth = 0.83f;
    params.center = 0.42f;
    params.feedback = -0.33f;
    params.color = 0.64f;
    params.zero = 0.81f;

    const auto a = renderTone (params, 4096);
    const auto b = renderTone (params, 4096);
    assert (a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i)
        assert (std::fabs (a[i] - b[i]) <= 1.0e-6f);
}

void testFiniteBoundedExtremeParameters()
{
    ZeroJetParameters params;
    params.rate = 1000.0f;
    params.depth = 1000.0f;
    params.center = 1000.0f;
    params.feedback = 1000.0f;
    params.color = 1000.0f;
    params.zero = std::numeric_limits<float>::infinity();
    params.mix = 1000.0f;

    ZeroJetEngine engine;
    engine.prepare (0.0);
    engine.setParameters (params);
    engine.reset();

    for (int i = 0; i < 8192; ++i)
    {
        const auto frame = engine.processSample (1000.0f, -1000.0f);
        assert (std::isfinite (frame.left));
        assert (std::isfinite (frame.right));
        assert (frame.left >= -0.9801f && frame.left <= 0.9801f);
        assert (frame.right >= -0.9801f && frame.right <= 0.9801f);
    }
}

void testDenormalInputDoesNotLeak()
{
    ZeroJetEngine engine;
    engine.prepare (48000.0);
    engine.reset();

    for (int i = 0; i < 1024; ++i)
    {
        const auto frame = engine.processSample (1.0e-30f, -1.0e-30f);
        assert (std::fabs (frame.left) <= 1.0e-7f);
        assert (std::fabs (frame.right) <= 1.0e-7f);
    }
}

} // namespace

int main()
{
    testSilenceStaysSilent();
    testDepthAndCenterMoveDelayHeads();
    testZeroControlChangesThroughZeroBlend();
    testSignedFeedbackAndColorChangeMotion();
    testDeterministic();
    testFiniteBoundedExtremeParameters();
    testDenormalInputDoesNotLeak();

    std::cout << "ZeroJetEngineTests passed\n";
    return 0;
}
