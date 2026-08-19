#pragma once

#include <stdint.h>

class IGeigerCounter
{
public:
    virtual bool begin() = 0;
    virtual uint32_t readAndReset() = 0;
    virtual uint32_t totalCount() const = 0;
    virtual ~IGeigerCounter() = default;
};