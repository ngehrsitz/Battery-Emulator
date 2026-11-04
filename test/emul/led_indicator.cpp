#include "led_indicator.h"
#include <Arduino.h>
#include <algorithm>
#include <cassert>
#include <limits>
#include "led_indicator_ledc.h"
#include "led_indicator_strips.h"

uint64_t millis64();

namespace {
struct MockIndicator {
  led_indicator_config_t config;
  std::vector<int> steps;
  std::vector<uint32_t> pixels;
  bool pwm;
  uint8_t brightness = 0;
  int active = -1;
  int preempt = -1;
  int last_base = -1;
  uint64_t due = UINT64_MAX;
};
struct Runtime {
  std::vector<MockIndicator*> indicators;
  std::vector<LedTestCall> calls;
  bool fail_creation = false;
  led_indicator_ledc_config_t pwm_config{};
  led_indicator_strips_config_t strip_config{};
};
Runtime& runtime() {
  // Outlive cross-translation-unit controller destruction at process shutdown.
  static auto* state = new Runtime;
  return *state;
}
MockIndicator& indicator() {
  assert(runtime().indicators.size() == 1);
  return *runtime().indicators.front();
}
void select(MockIndicator& led) {
  led.active = led.preempt;
  if (led.active == -1) {
    for (size_t i = 0; i < led.steps.size(); i++) {
      if (led.steps[i] != LED_BLINK_STOP) {
        led.active = i;
        break;
      }
    }
  }
}
void write_rgb(MockIndicator& led, uint32_t value) {
  const auto index = (value >> 25) & 0x7F;
  assert(!led.pwm && index < led.pixels.size());
  led.pixels[index] = value & 0xFFFFFF;
}
void run(MockIndicator& led) {
  led.due = UINT64_MAX;
  for (size_t guard = 0; guard < 1024; guard++) {
    if (led.active == -1) {
      return;
    }
    auto& cursor = led.steps[led.active];
    const auto& step = led.config.blink_lists[led.active][cursor++];
    if (step.type == LED_BLINK_LOOP) {
      cursor = 0;
      continue;
    }
    if (step.type == LED_BLINK_STOP) {
      cursor = LED_BLINK_STOP;
      led.preempt = -1;
      select(led);
      continue;
    }
    if (step.type == LED_BLINK_RGB) {
      write_rgb(led, step.value);
    } else {
      assert(step.type == LED_BLINK_BRIGHTNESS && led.pwm);
      led.brightness = step.value;
    }
    if (step.hold_time_ms) {
      led.due = millis64() + step.hold_time_ms;
      return;
    }
  }
  assert(false && "zero-duration infinite program");
}
esp_err_t create(const led_indicator_config_t* config, bool pwm, uint32_t count, led_indicator_handle_t* handle) {
  if (runtime().fail_creation) {
    *handle = nullptr;
    return ESP_FAIL;
  }
  auto* led = new MockIndicator;
  led->config = *config;
  led->steps.assign(config->blink_list_num, LED_BLINK_STOP);
  led->pixels.assign(count, 0);
  led->pwm = pwm;
  runtime().indicators.push_back(led);
  *handle = led;
  return ESP_OK;
}
}  // namespace

// Defining only the selected factory makes the host link catch unused backends.
#ifdef HW_DFROBOT_EDGE101
esp_err_t led_indicator_new_ledc_device(const led_indicator_config_t* config, const led_indicator_ledc_config_t* pwm,
                                        led_indicator_handle_t* handle) {
  runtime().pwm_config = *pwm;
  return create(config, true, 1, handle);
}
#else
esp_err_t led_indicator_new_strips_device(const led_indicator_config_t* config,
                                          const led_indicator_strips_config_t* strip, led_indicator_handle_t* handle) {
  runtime().strip_config = *strip;
  return create(config, false, strip->led_strip_cfg.max_leds, handle);
}
#endif
esp_err_t led_indicator_delete(led_indicator_handle_t handle) {
  auto* led = static_cast<MockIndicator*>(handle);
  auto& handles = runtime().indicators;
  handles.erase(std::remove(handles.begin(), handles.end(), led), handles.end());
  delete led;
  return ESP_OK;
}
esp_err_t led_indicator_start(led_indicator_handle_t handle, int program) {
  auto& led = *static_cast<MockIndicator*>(handle);
  assert(program >= 0 && program < led.config.blink_list_num && led.config.blink_lists[program]);
  runtime().calls.push_back({LedTestCallType::START, program, 0});
  led.last_base = program;
  led.steps[program] = 0;
  select(led);
  if (led.active == program) {
    led.due = millis64() + 1;  // Native start() schedules its first callback for the next RTOS tick.
  }
  return ESP_OK;
}
esp_err_t led_indicator_stop(led_indicator_handle_t handle, int program) {
  auto& led = *static_cast<MockIndicator*>(handle);
  runtime().calls.push_back({LedTestCallType::STOP, program, 0});
  led.steps[program] = LED_BLINK_STOP;
  select(led);  // Native stop() does not wake the timer.
  return ESP_OK;
}
esp_err_t led_indicator_preempt_start(led_indicator_handle_t handle, int program) {
  auto& led = *static_cast<MockIndicator*>(handle);
  runtime().calls.push_back({LedTestCallType::PREEMPT_START, program, 0});
  if (led.preempt != -1) {
    led.steps[led.preempt] = LED_BLINK_STOP;
  }
  led.steps[program] = 0;
  led.preempt = program;
  select(led);
  led.due = millis64() + 1;
  return ESP_OK;
}
esp_err_t led_indicator_preempt_stop(led_indicator_handle_t handle, int program) {
  auto& led = *static_cast<MockIndicator*>(handle);
  runtime().calls.push_back({LedTestCallType::PREEMPT_STOP, program, 0});
  if (led.preempt == program) {
    led.steps[program] = LED_BLINK_STOP;
    led.preempt = -1;
  }
  select(led);
  return ESP_OK;
}
esp_err_t led_indicator_set_rgb(led_indicator_handle_t handle, uint32_t value) {
  runtime().calls.push_back({LedTestCallType::SET_RGB, -1, value});
  write_rgb(*static_cast<MockIndicator*>(handle), value);
  return ESP_OK;
}

const std::vector<LedTestCall>& led_test_calls() {
  return runtime().calls;
}
void led_test_clear_calls() {
  runtime().calls.clear();
}
void led_test_fail_creation(bool fail) {
  runtime().fail_creation = fail;
}
void led_test_advance_to(uint64_t time_ms) {
  assert(time_ms >= millis64());
  while (true) {
    MockIndicator* next = nullptr;
    for (auto* led : runtime().indicators) {
      if (led->due <= time_ms && (!next || led->due < next->due)) {
        next = led;
      }
    }
    if (!next) {
      break;
    }
    set_millis64(next->due);
    run(*next);
  }
  set_millis64(time_ms);
}
uint8_t led_test_brightness() {
  const auto& led = indicator();
  if (led.pwm) {
    return led.brightness;
  }
  const uint32_t rgb = led.pixels[0];
  return std::max({(rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF});
}
const std::vector<uint32_t>& led_test_pixels() {
  return indicator().pixels;
}
std::vector<blink_step_t> led_test_program(int program) {
  const auto* steps = indicator().config.blink_lists[program];
  std::vector<blink_step_t> result;
  do {
    result.push_back(*steps++);
  } while (result.back().type != LED_BLINK_LOOP && result.back().type != LED_BLINK_STOP);
  return result;
}
int led_test_base_program() {
  return indicator().last_base;
}
int led_test_preempt_program() {
  return indicator().preempt;
}
const led_indicator_ledc_config_t& led_test_pwm_config() {
  return runtime().pwm_config;
}
const led_indicator_strips_config_t& led_test_strip_config() {
  return runtime().strip_config;
}
