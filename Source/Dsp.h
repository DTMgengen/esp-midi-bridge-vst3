#pragma once
#include "StrictPitch.h"
#include <array>
#include <complex>
#include <cmath>
#include <cstdint>

namespace esp {
constexpr double pi = 3.14159265358979323846;
constexpr int maxVoices = 6;
inline float hz(float note) { return 440.0f * std::exp2((note - 69.0f) / 12.0f); }
inline float midi(float frequency) { return 69.0f + 12.0f * std::log2(frequency / 440.0f); }

struct Detection {
    std::array<float, maxVoices> notes{};
    std::array<float, maxVoices> strength{};
    int count = 0;
    float confidence = 0;
};

// Bounded work and fixed storage. No allocation, lock, or I/O in the audio path.
class Tracker {
public:
    void prepare(double sampleRate);
    void reset();
    bool push(float input, bool poly, float threshold, float confidence, bool strict = false);
    const Detection& result() const { return detected; }
    float envelope() const { return env; }
    float analysisRate() const { return rate; }
    int monoWindow(bool strict=false) const { return strict?accurate.window():monoHistory; }
    int polyWindow() const { return fastSize; }
private:
    static constexpr int size = 2048, bins = size / 2, notes = 49, fastSize = 1024;
    static constexpr int maxTaps = 6 * (2 * ((3 * size + fastSize - 1) / fastSize) + 1);
    struct TemplateBin { int index = 0; float value = 0; };
    struct PitchTemplate { std::array<TemplateBin,maxTaps> taps{}; int count = 0; };
    void analyseMono(float confidence);
    void analysePoly();
    Detection analyseSpectrum();
    void selectLastMono();
    StrictPitch accurate;
    std::array<float, size> ring{}, frame{};
    std::array<float, 513> difference{};
    std::array<std::complex<float>, size> spectrum{};
    std::array<float, bins> magnitude{}, residual{};
    std::array<PitchTemplate,notes> dictionary{};
    std::array<float,fastSize> fastWindow{};
    std::array<float,notes> activation{};
    std::array<int,notes> presence{},absent{};
    std::array<uint64_t,notes> onsetOrder{};
    Detection monoSpectrum{},heldMono{};
    uint64_t onsetSequence=0;
    int selectedMono=-1,spectralSamples=0,spectralHop=0,monoGapSamples=0,overlapHold=0;
    bool spectralInitialised=false;
    bool monoAnchored=false;
    int periodicStreak=0;
    float previousPeriodic=-1;
    Detection detected;
    int position = 0, filled = 0, hop = 0, decimate = 4, decimationCount = 0;
    int monoActiveSamples = 0, monoHistory = 386;
    float rate = 12000, antiAlias = 0, low1 = 0, low2 = 0, low3 = 0, env = 0;
    float envAttack = 0, envRelease = 0;
};

}
