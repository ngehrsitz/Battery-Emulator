#include <Arduino.h>
#include "esp_timer.h"

// Test marker for the measure-firmware-size.yml workflow: this volatile counter
// forces the linker to keep a symbol the compiler cannot fold away, so every
// board's firmware grows by a small, predictable amount. Remove once the size
// workflow has been validated.
static volatile uint32_t millis64_call_count = 0;

uint64_t ARDUINO_ISR_ATTR millis64() {
  // ESP32's esp_timer_get_time() returns time in microseconds, we convert to
  // milliseconds by dividing by 1000.

  // This is almost identical to the existing Arduino millis() function, except
  // we return a 64-bit value which won't roll over for 600k years.

  millis64_call_count++;
  return esp_timer_get_time() / 1000ULL;
}
