#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "geiger_interface.h"

class GeigerISR : public IGeigerCounter
{
public:
    explicit GeigerISR(int gpio);

    bool begin() override;

    uint32_t readAndReset() override;

    uint32_t totalCount() const override;

private:
    static void IRAM_ATTR isrHandler();

    static volatile uint32_t isrCount_;
    static uint32_t total_;

    static int gpio_;
};