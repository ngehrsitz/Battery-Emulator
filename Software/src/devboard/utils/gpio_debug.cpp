#ifdef DEBUG_GPIO_INVALID_PIN

#include <Arduino.h>
#include <soc/soc_caps.h>

// __pinMode is a C function — must be declared extern "C" to suppress mangling.
extern "C" void __pinMode(uint8_t pin, uint8_t mode);

// Strong definition of the weak alias: replaces the framework's no-op redirect.
// Logs the return address when an out-of-range signal index is passed as a pin,
// then delegates to the real implementation so normal pins still work.
extern "C" void pinMode(uint8_t pin, uint8_t mode) {
  if (pin >= SOC_GPIO_PIN_COUNT) {
    log_e("Invalid IO %u selected, caller=%p", pin, __builtin_return_address(0));
  }
  __pinMode(pin, mode);
}

#endif  // DEBUG_GPIO_INVALID_PIN
