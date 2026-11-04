#pragma once

#include <cstdint>
#include <vector>

// Host stand-in for the public led_indicator 2.1.2 API. Tests execute only the
// absolute RGB/brightness steps used by the emulator, not a second fade engine.
using esp_err_t = int;
#define ESP_OK 0
#define ESP_FAIL -1
#define SET_RGB(r, g, b) ((((r) & 0xFFu) << 16) | (((g) & 0xFFu) << 8) | ((b) & 0xFFu))
#define SET_IRGB(i, r, g, b) ((((i) & 0x7Fu) << 25) | SET_RGB(r, g, b))

enum blink_step_type_t {
  LED_BLINK_STOP = -1,
  LED_BLINK_HOLD,
  LED_BLINK_BREATHE,
  LED_BLINK_BRIGHTNESS,
  LED_BLINK_RGB,
  LED_BLINK_RGB_RING,
  LED_BLINK_HSV,
  LED_BLINK_HSV_RING,
  LED_BLINK_LOOP
};
struct blink_step_t {
  blink_step_type_t type;
  uint32_t value;
  uint32_t hold_time_ms;
};
struct led_indicator_config_t {
  const blink_step_t** blink_lists;
  uint16_t blink_list_num;
};
using led_indicator_handle_t = void*;

esp_err_t led_indicator_delete(led_indicator_handle_t handle);
esp_err_t led_indicator_start(led_indicator_handle_t handle, int program);
esp_err_t led_indicator_stop(led_indicator_handle_t handle, int program);
esp_err_t led_indicator_preempt_start(led_indicator_handle_t handle, int program);
esp_err_t led_indicator_preempt_stop(led_indicator_handle_t handle, int program);
esp_err_t led_indicator_set_rgb(led_indicator_handle_t handle, uint32_t value);

enum class LedTestCallType { START, STOP, PREEMPT_START, PREEMPT_STOP, SET_RGB };
struct LedTestCall {
  LedTestCallType type;
  int program;
  uint32_t value;
};
const std::vector<LedTestCall>& led_test_calls();
void led_test_clear_calls();
void led_test_advance_to(uint64_t time_ms);
void led_test_fail_creation(bool fail);
uint8_t led_test_brightness();
const std::vector<uint32_t>& led_test_pixels();
std::vector<blink_step_t> led_test_program(int program);
int led_test_base_program();
int led_test_preempt_program();
