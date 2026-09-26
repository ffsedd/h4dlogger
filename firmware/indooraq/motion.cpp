#include "motion.h"
#include "config.h"
#include "state.h"

////////////////////////////////////////////////////////////
// PHASE 1 — INTERRUPT-DRIVEN PIR HANDLING
//
// The AM312 / LD1020 outputs are asynchronous digital edges, not
// something that belongs on a fixed poll timer. Each pin gets a CHANGE
// interrupt; the ISR does the absolute minimum (read the pin, stamp
// millis()) and nothing else -- no Serial, no float math, no aggregator
// work inside ISR context. read_motion_sensors() (called every loop()
// iteration) drains that captured state under a critical section and
// does the real bookkeeping.
////////////////////////////////////////////////////////////

static portMUX_TYPE am312Mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool am312_isrMotion = false;
static volatile uint32_t am312_lastEdgeTs = 0;

static portMUX_TYPE ld1020Mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool ld1020_isrMotion = false;
static volatile uint32_t ld1020_lastEdgeTs = 0;

static void IRAM_ATTR isr_am312()
{
  portENTER_CRITICAL_ISR(&am312Mux);
  am312_isrMotion = digitalRead(PIN_AM312);
  am312_lastEdgeTs = millis();
  portEXIT_CRITICAL_ISR(&am312Mux);
}

static void IRAM_ATTR isr_ld1020()
{
  portENTER_CRITICAL_ISR(&ld1020Mux);
  ld1020_isrMotion = digitalRead(PIN_LD1020);
  ld1020_lastEdgeTs = millis();
  portEXIT_CRITICAL_ISR(&ld1020Mux);
}

void setup_LD1020()
{
  if (LD1020_PRESENT)
  {
    pinMode(PIN_LD1020, INPUT);
    ld1020.present = true;
    ld1020.motion = digitalRead(PIN_LD1020);
    ld1020.lastMotion = ld1020.motion;
    ld1020_isrMotion = ld1020.motion;
    attachInterrupt(digitalPinToInterrupt(PIN_LD1020), isr_ld1020, CHANGE);
    Serial.printf("[LD1020] initialized on GPIO%d (interrupt-driven)\n", PIN_LD1020);
  }
  else
  {
    ld1020.present = false;
    ld1020.motion = false;
    Serial.println("[LD1020] not present");
  }
}

void setup_AM312()
{
  if (AM312_PRESENT)
  {
    pinMode(PIN_AM312, INPUT);
    am312.present = true;
    am312.motion = digitalRead(PIN_AM312);
    am312.lastMotion = am312.motion;
    am312_isrMotion = am312.motion;
    attachInterrupt(digitalPinToInterrupt(PIN_AM312), isr_am312, CHANGE);
    Serial.printf("[AM312] initialized on GPIO%d (interrupt-driven)\n", PIN_AM312);
  }
  else
  {
    am312.present = false;
    am312.motion = false;
    Serial.println("[AM312] not present");
  }
}

void read_motion_sensors()
{
  static uint32_t lastCallTs = 0;
  uint32_t now = millis();
  // Elapsed time since the previous call -- this is the interval we
  // attribute to whatever state each sensor was in *before* this update
  // (skip on the very first call, there's nothing to attribute yet).
  uint32_t dt = (lastCallTs == 0) ? 0 : (now - lastCallTs);
  lastCallTs = now;

  if (ld1020.present)
  {
    portENTER_CRITICAL(&ld1020Mux);
    bool motion = ld1020_isrMotion;
    uint32_t edgeTs = ld1020_lastEdgeTs;
    portEXIT_CRITICAL(&ld1020Mux);

    if (dt)
      ld1020Agg.addInterval(ld1020.lastMotion, dt);

    ld1020.motion = motion;
    if (motion)
      ld1020.lastMotionTs = edgeTs;
    if (LOG_MOTION_EVENTS && motion && !ld1020.lastMotion)
      Serial.println("[LD1020] motion detected! =====================");
    ld1020.lastMotion = motion;
  }

  if (am312.present)
  {
    portENTER_CRITICAL(&am312Mux);
    bool motion = am312_isrMotion;
    uint32_t edgeTs = am312_lastEdgeTs;
    portEXIT_CRITICAL(&am312Mux);

    if (dt)
      am312Agg.addInterval(am312.lastMotion, dt);

    am312.motion = motion;
    if (motion)
      am312.lastMotionTs = edgeTs;
    if (LOG_MOTION_EVENTS && motion && !am312.lastMotion)
      Serial.println("[AM312] motion detected! --------------------");
    am312.lastMotion = motion;
  }
}
