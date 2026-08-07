#ifdef DEBUG_GPIO_INVALID_PIN

#include <Arduino.h>
#include <soc/soc_caps.h>

// Forward declaration of the real implementation in the framework.
extern void __pinMode(uint8_t pin, uint8_t mode);

// Strong override of the weak alias so every pinMode() call goes here first.
// Only compiled when -D DEBUG_GPIO_INVALID_PIN is set.
extern "C" void pinMode(uint8_t pin, uint8_t mode) {
  if (pin >= SOC_GPIO_PIN_COUNT) {
    log_e("Invalid IO %u selected, caller=%p", pin, __builtin_return_address(0));
  }
  __pinMode(pin, mode);
}

#endif  // DEBUG_GPIO_INVALID_PIN
