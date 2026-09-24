#include "leds.h"
#include "config.h"
#include "state.h"

////////////////////////////////////////////////////////////
// LED HELPERS (blocking)
////////////////////////////////////////////////////////////

void setupLED()
{
  ledcAttach(PIN_LED_R, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_G, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_Y, PWM_FREQ, PWM_RES);

  led_off();
}

void led_set(uint8_t r, uint8_t y, uint8_t g)
{
  ledcWrite(PIN_LED_R, r);
  ledcWrite(PIN_LED_Y, y);
  ledcWrite(PIN_LED_G, g);
}

void led_off()
{
  led_set(0, 0, 0);
}

void led_flash(
    uint8_t r,
    uint8_t y,
    uint8_t g,
    uint16_t on_ms,
    uint16_t off_ms)
{
  led_set(r, y, g);
  delay(on_ms);
  led_off();
  delay(off_ms);
}

////////////////////////////////////////////////////////////
// LED SEQUENCES
////////////////////////////////////////////////////////////

void led_ntp_success_sequence()
{
  const uint16_t ON = 180;
  const uint16_t OFF = 120;

  led_off();
  delay(150);

  led_flash(0, 0, 255, ON, OFF); // G
  led_flash(0, 255, 0, ON, OFF); // Y
  led_flash(255, 0, 0, ON, OFF); // R
  led_flash(0, 255, 0, ON, OFF); // Y
  led_flash(0, 0, 255, ON, OFF); // G

  led_set(0, 0, 255);
  delay(400);

  led_off();
}

////////////////////////////////////////////////////////////
// CO2 LED
////////////////////////////////////////////////////////////

enum class BlinkMode : uint8_t
{
  NONE,
  GREEN_HEARTBEAT,
  SLOW_RED,
  FAST_RED
};

static inline uint8_t lerp8(float t)
{
  t = constrain(t, 0.0f, 1.0f);
  return static_cast<uint8_t>(t * 255.0f);
}

static void co2_to_led(
    float co2,
    uint8_t &r,
    uint8_t &y,
    uint8_t &g,
    BlinkMode &blink)
{
  r = y = g = 0;
  blink = BlinkMode::NONE;

  if (co2 < CO2_EXCELLENT)
  {
    g = 255;
    blink = BlinkMode::GREEN_HEARTBEAT;
  }
  else if (co2 < CO2_GOOD)
  {
    g = 255;
  }
  else if (co2 < CO2_ELEVATED)
  {
    const float t =
        (co2 - CO2_GOOD) / (CO2_ELEVATED - CO2_GOOD);

    g = lerp8(1.0f - t);
    y = lerp8(t);
  }
  else if (co2 < CO2_HIGH)
  {
    y = 255;
  }
  else if (co2 < CO2_VENTILATE)
  {
    const float t =
        (co2 - CO2_HIGH) / (CO2_VENTILATE - CO2_HIGH);

    y = lerp8(1.0f - t);
    r = lerp8(t);
  }
  else if (co2 < CO2_SLOW_BLINK)
  {
    r = 255;
  }
  else if (co2 < CO2_FAST_BLINK)
  {
    r = 255;
    blink = BlinkMode::SLOW_RED;
  }
  else
  {
    r = 255;
    blink = BlinkMode::FAST_RED;
  }
}

////////////////////////////////////////////////////////////
// CO2 STATUS LED UPDATE
////////////////////////////////////////////////////////////

void updateLED()
{
  const uint32_t now = millis();

  ////////////////////////////////////////////////////////////
  // MOTION GATE
  ////////////////////////////////////////////////////////////

  const uint32_t last_motion_ts =
      max(ld1020.lastMotionTs, am312.lastMotionTs);

  if (
      last_motion_ts == 0 ||
      now - last_motion_ts > MOTION_HOLD_MS)
  {
    led_off();
    return;
  }

  ////////////////////////////////////////////////////////////
  // CO2 VALIDITY
  ////////////////////////////////////////////////////////////

  if (isnan(co2Smooth))
  {
    led_off();
    return;
  }

  ////////////////////////////////////////////////////////////
  // CO2 -> LED
  ////////////////////////////////////////////////////////////

  uint8_t r = 0;
  uint8_t y = 0;
  uint8_t g = 0;
  BlinkMode blink = BlinkMode::NONE;

  co2_to_led(co2Smooth, r, y, g, blink);

  ////////////////////////////////////////////////////////////
  // BLINK ENGINE
  ////////////////////////////////////////////////////////////

  static uint32_t phase_start = 0;

  uint32_t period = 0;
  uint32_t on_time = 0;

  switch (blink)
  {
  case BlinkMode::GREEN_HEARTBEAT:
    period = 2000;
    on_time = 150;
    break;

  case BlinkMode::SLOW_RED:
    period = 2000;
    on_time = 1000;
    break;

  case BlinkMode::FAST_RED:
    period = 500;
    on_time = 250;
    break;

  case BlinkMode::NONE:
    break;
  }

  if (blink != BlinkMode::NONE)
  {
    if (now - phase_start >= period)
      phase_start = now;

    if (now - phase_start >= on_time)
      r = y = g = 0;
  }

  ////////////////////////////////////////////////////////////
  // OUTPUT
  ////////////////////////////////////////////////////////////

  ledcWrite(PIN_LED_G, g);
  ledcWrite(PIN_LED_Y, y);
  ledcWrite(PIN_LED_R, r);

  ////////////////////////////////////////////////////////////
  // LOGGING
  ////////////////////////////////////////////////////////////

  if (LOG_LED_EVENTS)
  {
    static uint32_t last_log = 0;

    if (now - last_log > 5000)
    {
      last_log = now;

      Serial.printf(
          "[LED] CO2=%.1f G=%u Y=%u R=%u mode=%u\n",
          co2Smooth,
          g,
          y,
          r,
          static_cast<uint8_t>(blink));
    }
  }
}
