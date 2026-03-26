/*
 * BuildingLED_Debug.h
 * Zero-cost debug macros - compiled out when DEBUG is 0
 */
#ifndef BUILDING_LED_DEBUG_H
#define BUILDING_LED_DEBUG_H

#include <Arduino.h>
#include <stdarg.h>

#ifndef BLED_DEBUG
#define BLED_DEBUG 0
#endif

#if BLED_DEBUG
  #define BLED_DBG_BEGIN(baud)  Serial.begin(baud)
  #define BLED_DBG(x)           Serial.print(x)
  #define BLED_DBGLN(x)         Serial.println(x)
  #define BLED_DBGF(x)          Serial.print(F(x))
  #define BLED_DBGFLN(x)        Serial.println(F(x))

  static void BLED_PRINTF(const char* fmt, ...) {
    char buf[120];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.print(buf);
  }

  static void BLED_HEX(uint8_t v) {
    Serial.print(F("0x"));
    if (v < 0x10) Serial.print('0');
    Serial.print(v, HEX);
  }
#else
  #define BLED_DBG_BEGIN(baud)   ((void)0)
  #define BLED_DBG(x)            ((void)0)
  #define BLED_DBGLN(x)          ((void)0)
  #define BLED_DBGF(x)           ((void)0)
  #define BLED_DBGFLN(x)         ((void)0)
  static inline void BLED_PRINTF(const char*, ...) {}
  static inline void BLED_HEX(uint8_t) {}
#endif

#endif // BUILDING_LED_DEBUG_H
