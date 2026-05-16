#include "geiger_core.h"
#include <stdlib.h>

GeigerCore::GeigerCore(const GeigerConfig& c)
    : cfg(c)
{
    cpsBuffer = (float*)calloc(cfg.window, sizeof(float));
    avgBuffer = (float*)calloc(cfg.window, sizeof(float));
}

uint32_t GeigerCore::size() const
{
    return bufFull ? cfg.window : bufIndex;
}

float GeigerCore::computeMean() const
{
    uint32_t n = size();
    if (!n) return 0;

    float sum = 0;

    for(uint32_t i=0;i<n;i++)
    {
        uint32_t idx =
            bufFull ? (bufIndex+i)%cfg.window : i;

        sum += cpsBuffer[idx];
    }

    return sum / n;
}

void GeigerCore::addPulses(uint32_t pulses)
{
    float cps = pulses / (cfg.sampleIntervalMs / 1000.0f);

    out.cps = cps;
}

bool GeigerCore::update(uint32_t nowMs)
{
    if(nowMs - lastSample < cfg.sampleIntervalMs)
        return false;

    lastSample = nowMs;

    out.mean = computeMean();
    out.cpm = out.cps * 60.0f;
    out.cph = out.cpm * 60.0f;

    cpsBuffer[bufIndex] = out.cps;
    avgBuffer[bufIndex] = out.mean;

    bufIndex++;

    if(bufIndex >= cfg.window)
    {
        bufIndex = 0;
        bufFull = true;
    }

    return true;
}

const GeigerOutput& GeigerCore::output() const
{
    return out;
}
