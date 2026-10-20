#pragma once
#include <array>
#include <cstdint>

namespace esp {
struct Detection;

// Independent continuous-pitch estimator. Does not consume LOOSE estimates or
// transpose their result. Fixed storage; all work is bounded per analysis hop.
class StrictPitch {
public:
    void prepare(float sampleRate);
    void reset();
    int window() const { return windowSize; }
    Detection analyse(const std::array<float,2048>& ring, int position, int filled, int available,
                      float envelope, float confidence, const Detection* spectrum);
private:
    struct Snapshot {std::array<double,17> coefficient{};float frequency=0;int harmonics=0;int64_t stamp=0;};
    std::array<Snapshot,49> predictor{};
    struct Estimate { float note=0, clarity=0; bool valid=false; };
    Estimate periodic(int length, float confidence);
    float modelFit(float frequency, int length) const;
    bool spectralPeak(float frequency, int length, float spacing=0) const;
    float rate=12000;
    int windowSize=480;
    std::array<float,768> frame{},weighted{};
    std::array<float,384> nsdf{};
    std::array<int,49> presence{},absence{};
    std::array<uint64_t,49> order{};
    std::array<bool,49> qualified{},rising{};
    std::array<int64_t,49> riseAt{},consumedRise{};
    std::array<float,49> originNote{};
    std::array<float,32> rawHistory{};
    int rawPosition=0,rawCount=0,rawStable=0;
    int64_t sampleClock=0;
    uint64_t epoch=0;
    std::array<float,49> resonator{},powerReference{},capturePower{};
    std::array<int,49> novelty{};
    int selected=-1,gap=0,stable=0;
    float levelBlend=0,lastNote=0,previousNote=-1,previousRaw=-1,motion=0;
    bool anchored=false,overlap=false;
};
}
