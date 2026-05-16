#pragma once
#include <stdint.h>

struct GeigerConfig
{
    uint32_t window;
    uint32_t sampleIntervalMs;
};

struct GeigerOutput
{
    float cps;
    float mean;
    float cpm;
    float cph;
};

class GeigerCore
{
public:
    GeigerCore(const GeigerConfig& cfg);

    void addPulses(uint32_t pulses);
    bool update(uint32_t nowMs);

    const GeigerOutput& output() const;

    const float* cpsData() const { return cpsBuffer; }
    const float* meanData() const { return avgBuffer; }

    uint32_t size() const;
    uint32_t index() const { return bufIndex; }
    bool full() const { return bufFull; }

private:
    float computeMean() const;

    GeigerConfig cfg;

    float* cpsBuffer;
    float* avgBuffer;

    uint32_t bufIndex = 0;
    bool bufFull = false;

    uint32_t lastSample = 0;

    GeigerOutput out {};
};
