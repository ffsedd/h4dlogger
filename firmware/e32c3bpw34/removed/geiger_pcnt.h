#pragma once

#include <stdint.h>
#include <driver/pcnt.h> // remove??
#include "geiger_interface.h"

class GeigerPCNT : public IGeigerCounter
{
public:
    explicit GeigerPCNT(int gpio);

    bool begin() override;
    uint32_t readAndReset() override;
    uint32_t totalCount() const override;

private:
    int gpio_;
    pcnt_unit_t unit_;
    pcnt_channel_t channel_;

    uint32_t total_ = 0;
    int16_t last_read_ = 0;
};