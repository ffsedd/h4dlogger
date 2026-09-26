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
// LED SEQUENCES (Phase 3: non-blocking NTP success pattern)
////////////////////////////////////////////////////////////

struct FlashStep
{
  uint8_t r, y, g;
  uint16_t ms;
};

// off(150), G, off, Y, off, R, off, Y, off, G, off, solid-G(400), off
static const FlashStep ntpSeq[] = {
    {0, 0, 0, 150},
    {0, 0, 255, 180}, {0, 0, 0, 120}, // G
    {0, 255, 0, 180}, {0, 0, 0, 120}, // Y
    {255, 0, 0, 180}, {0, 0, 0, 120}, // R
    {0, 255, 0, 180}, {0, 0, 0, 120}, // Y
    {0, 0, 255, 180}, {0, 0, 0, 120}, // G
    {0, 0, 255, 400},                 // solid G
};
static constexpr size_t ntpSeqLen = sizeof(ntpSeq) / sizeof(ntpSeq[0]);

static bool ntpSeqActive = false;
static size_t ntpSeqIndex = 0;
static uint32_t ntpSeqStepStart = 0;

void led_start_ntp_success_sequence()
{
  ntpSeqActive = true;
  ntpSeqIndex = 0;
  ntpSeqStepStart = millis();
  led_set(ntpSeq[0].r, ntpSeq[0].y, ntpSeq[0].g);
}

// Advances the sequence if one is running. Returns true while it still
// owns the LED this frame (so updateLED() knows to skip the CO2 gradient
// rather than fight it for the same PWM channels).
static bool serviceNtpSequence()
{
  if (!ntpSeqActive)
    return false;

  uint32_t now = millis();
  if (now - ntpSeqStepStart >= ntpSeq[ntpSeqIndex].ms)
  {
    ntpSeqIndex++;
    if (ntpSeqIndex >= ntpSeqLen)
    {
      ntpSeqActive = false;
      led_off();
      return false;
    }
    ntpSeqStepStart = now;
    led_set(ntpSeq[ntpSeqIndex].r, ntpSeq[ntpSeqIndex].y, ntpSeq[ntpSeqIndex].g);
  }
  return true;
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
  // Phase 3: the NTP-success flash pattern, if one is in progress, owns
  // the LED this frame -- skip the normal CO2 gradient until it's done.
  if (serviceNtpSequence())
    return;

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

////////////////////////////////////////////////////////////
// ONBOARD LED — 4-BIT FRAMED ERROR CODE
////////////////////////////////////////////////////////////
//
// Single-color, active-LOW onboard LED (GPIO8, see config.h). Loops a
// 4-bit pattern: bit 0 and bit 3 are always 1 (start/stop framing bits),
// the middle two bits encode current system state:
//
//   1 0 0 1   OK
//   1 1 1 1   WiFi not connected
//   1 0 1 1   MQTT not connected
//   1 1 0 1   other error
//
// Checked in that priority order every time a new pass starts (WiFi loss
// is worth knowing about even if MQTT also happens to be down as a
// result of it). Non-blocking -- advanced a bit-slot at a time from
// updateOnboardLED(), same style as the CO2 blink engine above.
////////////////////////////////////////////////////////////

enum class SysCode : uint8_t
{
  OK,
  WIFI_DOWN,
  MQTT_DOWN,
  OTHER
};

static const bool onboardCodeBits[4][4] = {
    {1, 0, 0, 1}, // 9 = OK
    {1, 0, 1, 1}, // 11 = WIFI_DOWN
    {1, 1, 0, 1}, // 13 = MQTT_DOWN
    {1, 1, 1, 1}, // 15 = OTHER
};

static constexpr uint32_t OB_BIT_SLOT_MS = 250;  // 4 bits × 250 ms = 1 s
static constexpr uint32_t OB_GAP_MS = 4000;       // then wait 4 s before repeating

static uint8_t obBitIndex = 0;
static bool obInGap = false;
static uint32_t obPhaseStart = 0;
static SysCode obActiveCode = SysCode::OK;

static inline void onboardWrite(bool on)
{
  digitalWrite(PIN_ONBOARD_LED, on ? LOW : HIGH); // active-LOW
}

void report_other_error(bool active)
{
  otherErrorFlag = active;
}

// Any I2C sensor that was detected on the bus but failed init counts as
// an "other" error automatically -- no extra wiring needed for that case.
static bool anySensorFailed()
{
  return (shtStat.present && !shtStat.initialized) ||
         (bmpStat.present && !bmpStat.initialized) ||
         (tslStat.present && !tslStat.initialized) ||
         (scdStat.present && !scdStat.initialized);
}

static SysCode currentSysCode()
{
  if (WiFi.status() != WL_CONNECTED)
    return SysCode::WIFI_DOWN;
  if (!mqtt.connected())
    return SysCode::MQTT_DOWN;
  if (otherErrorFlag || anySensorFailed())
    return SysCode::OTHER;
  return SysCode::OK;
}

void setupOnboardLED()
{
  pinMode(PIN_ONBOARD_LED, OUTPUT);

  obActiveCode = SysCode::OK; // corrected at the end of the first pass below
  obBitIndex = 0;
  obInGap = false;
  obPhaseStart = millis();
  onboardWrite(onboardCodeBits[(uint8_t)obActiveCode][0]); // show bit 0 immediately
}

void updateOnboardLED()
{
  uint32_t now = millis();

  if (obInGap)
  {
    if (now - obPhaseStart >= OB_GAP_MS)
    {
      obInGap = false;
      obBitIndex = 0;
      obPhaseStart = now;
      obActiveCode = currentSysCode(); // re-evaluate once per pass, not mid-code
      onboardWrite(onboardCodeBits[(uint8_t)obActiveCode][0]);
    }
    return;
  }

  if (now - obPhaseStart >= OB_BIT_SLOT_MS)
  {
    obBitIndex++;
    obPhaseStart = now;

    if (obBitIndex >= 4)
    {
      onboardWrite(false);
      obInGap = true;
      return;
    }

    onboardWrite(onboardCodeBits[(uint8_t)obActiveCode][obBitIndex]);
  }
}
