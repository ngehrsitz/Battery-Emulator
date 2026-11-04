#pragma once

#include "led_indicator.h"

enum ledc_timer_t { LEDC_TIMER_0, LEDC_TIMER_1, LEDC_TIMER_2, LEDC_TIMER_3 };
enum ledc_channel_t { LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3, LEDC_CHANNEL_4 };
struct led_indicator_ledc_config_t {
  bool is_active_level_high;
  bool timer_inited;
  ledc_timer_t timer_num;
  int32_t gpio_num;
  ledc_channel_t channel;
};
esp_err_t led_indicator_new_ledc_device(const led_indicator_config_t* config, const led_indicator_ledc_config_t* pwm,
                                        led_indicator_handle_t* handle);
const led_indicator_ledc_config_t& led_test_pwm_config();
