#include "motion.h"
#include "config.h"
#include "state.h"

void setup_LD1020()
{
  if (LD1020_PRESENT)
  {
    pinMode(PIN_LD1020, INPUT);
    ld1020.present = true;
    ld1020.motion = digitalRead(PIN_LD1020);
    ld1020.lastMotion = ld1020.motion;
    Serial.printf("[LD1020] initialized on GPIO%d\n", PIN_LD1020);
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
    Serial.printf("[AM312] initialized on GPIO%d\n", PIN_AM312);
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
  if (ld1020.present)
  {
    ld1020.motion = digitalRead(PIN_LD1020);
    ld1020Agg.add(ld1020.motion);
    if (ld1020.motion)
      ld1020.lastMotionTs = millis();
    if (LOG_MOTION_EVENTS && ld1020.motion && !ld1020.lastMotion)
      Serial.println("[LD1020] motion detected! =====================");
    ld1020.lastMotion = ld1020.motion;
  }

  if (am312.present)
  {
    am312.motion = digitalRead(PIN_AM312);
    am312Agg.add(am312.motion);
    if (am312.motion)
      am312.lastMotionTs = millis();
    if (LOG_MOTION_EVENTS && am312.motion && !am312.lastMotion)
      Serial.println("[AM312] motion detected! --------------------");
    am312.lastMotion = am312.motion;
  }
}
