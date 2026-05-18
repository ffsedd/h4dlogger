#include "threshold_alarm.hpp"

namespace alarm
{

    ThresholdAlarm::ThresholdAlarm(Config cfg)
        : cfg_(cfg) {}

    bool ThresholdAlarm::should_trigger(double v) const noexcept
    {
        return v >= cfg_.threshold;
    }

    bool ThresholdAlarm::should_clear(double v) const noexcept
    {
        return v <= (cfg_.threshold - cfg_.hysteresis);
    }

    void ThresholdAlarm::set_callback(Callback cb)
    {
        callback_ = std::move(cb);
    }

    State ThresholdAlarm::state() const noexcept
    {
        return state_;
    }

    void ThresholdAlarm::change_state(State s)
    {
        if (s == state_)
            return;

        state_ = s;

        if (callback_)
            callback_(state_);
    }

    void ThresholdAlarm::update(
        double value,
        std::chrono::steady_clock::time_point now)
    {
        if (state_ == State::Normal)
        {
            if (should_trigger(value))
            {
                if (now - last_transition_ >= cfg_.hold_time)
                {
                    last_transition_ = now;
                    change_state(State::Alarm);
                }
            }
        }
        else
        {
            if (should_clear(value))
            {
                if (now - last_transition_ >= cfg_.hold_time)
                {
                    last_transition_ = now;
                    change_state(State::Normal);
                }
            }
        }
    }

} // namespace alarm