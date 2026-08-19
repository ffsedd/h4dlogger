#include "geiger_pcnt.h"
#include <driver/pcnt.h>

GeigerPCNT::GeigerPCNT(int gpio)
    : gpio_(gpio)
{
    unit_ = PCNT_UNIT_0;
    channel_ = PCNT_CHANNEL_0;
    total_ = 0;
}

bool GeigerPCNT::begin()
{
    pcnt_config_t cfg = {};

    cfg.pulse_gpio_num = gpio_;
    cfg.ctrl_gpio_num = PCNT_PIN_NOT_USED;

    cfg.channel = channel_;
    cfg.unit = unit_;

    cfg.pos_mode = PCNT_COUNT_DIS;
    cfg.neg_mode = PCNT_COUNT_INC;

    cfg.lctrl_mode = PCNT_MODE_KEEP;
    cfg.hctrl_mode = PCNT_MODE_KEEP;

    cfg.counter_h_lim = 32767;
    cfg.counter_l_lim = 0;

    if (pcnt_unit_config(&cfg) != ESP_OK)
        return false;

    pcnt_counter_pause(unit_);
    pcnt_counter_clear(unit_);
    pcnt_counter_resume(unit_);

    return true;
}

uint32_t GeigerPCNT::readAndReset()
{
    int16_t count = 0;

    pcnt_get_counter_value(unit_, &count);
    pcnt_counter_clear(unit_);

    if (count > 0)
        total_ += count;

    return (uint32_t)count;
}

uint32_t GeigerPCNT::totalCount() const
{
    return total_;
}