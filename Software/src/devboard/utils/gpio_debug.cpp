#ifdef DEBUG_GPIO_INVALID_PIN

#include <Arduino.h>
#include <esp_log.h>
#include <soc/soc_caps.h>

extern "C" void __pinMode(uint8_t pin, uint8_t mode);

extern "C" void pinMode(uint8_t pin, uint8_t mode) {
  if (pin >= SOC_GPIO_PIN_COUNT) {
    // ESP_EARLY_LOGE bypasses the log-level gate — visible even before Serial init.
    ESP_EARLY_LOGE("gpio_debug", "Invalid IO %u selected, caller=%p", pin, __builtin_return_address(0));
  }
  __pinMode(pin, mode);
}

#endif  // DEBUG_GPIO_INVALID_PIN
