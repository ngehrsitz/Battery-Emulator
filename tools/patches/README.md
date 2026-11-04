# Native LED size experiment

`led_indicator-2.1.2-size.patch` contains local changes to the pinned upstream
component, not a replacement animation engine:

- Remove the unused common duty calculation and replace LEDC/RGB powers of two
  with integer shifts.
- Add default-enabled guards for SPI strips and advanced blink steps (HSV,
  RGB/HSV rings, and breathe).

The application build disables those guards because it uses RMT strips or PWM
and supplies sampled RGB/brightness steps itself. Priorities, feedback, and the
existing animations stay on the native engine.

`tools/led_indicator_components.py` applies the patch idempotently after managed
sources are available, then rebuilds only the LED components with LTO. The
controller is also compiled with LTO; other application/SDK sources are not.
CI caches the managed sources and includes this patch in the cache key.
