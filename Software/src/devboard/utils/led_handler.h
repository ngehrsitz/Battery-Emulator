#ifndef LED_H_
#define LED_H_

#include <cstdint>

// Indicator LEDs 1-4 in the chain, for boards that can replace their hardwired 12V-output LEDs
// with RGB LEDs (pixel 0 is always the STATUS LED and is unaffected by this).
enum class IndicatorLed : uint8_t { PRECHARGE = 0, CONTACTOR_NEG = 1, CONTACTOR_POS = 2, BMS_POWER = 3 };

enum class LedFeedback : uint8_t { NONE, AP_READY, NETWORK_RESET_READY, FACTORY_RESET_READY };

bool led_init(void);
// Selects programs on state changes; the native timer animates independently.
// A new state or feedback release restarts the selected waveform from its beginning.
void led_exe(void);

// Requests button feedback from any task. led_exe() submits changes to the native
// preemption engine; repeating the same tier does not restart its blink program.
void set_led_feedback(LedFeedback feedback);

// Mirrors on/off state onto an RGB indicator LED, for boards where an indicator position is an
// RGB LED instead of a plain hardwired GPIO LED. No-op on boards without RGB indicator LEDs.
void set_indicator_led(IndicatorLed indicator, bool on);

#endif  // LED_H_
