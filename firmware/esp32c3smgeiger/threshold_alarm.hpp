#pragma once

#include <functional>
#include <chrono>

namespace soundAlarm
{

    enum class State
    {
        Normal,
        Alarm
    };

    struct Config
    {
        double threshold;                       // trigger level
        double hysteresis;                      // prevents chatter
        std::chrono::milliseconds hold_time{0}; // debounce
    };

    class ThresholdAlarm
    {
    public:
        using Callback = std::function<void(State)>;

        explicit ThresholdAlarm(const Config &cfg);

        void update(double value,
                    std::chrono::steady_clock::time_point now);

        State state() const noexcept;

        void set_callback(Callback cb);

    private:
        Config cfg_;
        State state_{State::Normal};

        std::chrono::steady_clock::time_point last_transition_{};
        Callback callback_{};

        bool should_trigger(double v) const noexcept;
        bool should_clear(double v) const noexcept;

        void change_state(State s);
    };

} // namespace soundAlarm