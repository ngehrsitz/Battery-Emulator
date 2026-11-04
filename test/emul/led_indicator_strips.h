#pragma once

#include "led_indicator.h"

enum led_model_t { LED_MODEL_WS2812, LED_MODEL_SK6812 };
enum led_strip_color_component_format_t { LED_STRIP_COLOR_COMPONENT_FMT_RGB, LED_STRIP_COLOR_COMPONENT_FMT_GRB };
enum rmt_clock_source_t { RMT_CLK_SRC_DEFAULT };
enum led_strip_driver_t { LED_STRIP_RMT, LED_STRIP_SPI };
struct led_strip_config_t {
  int strip_gpio_num;
  uint32_t max_leds;
  led_model_t led_model;
  led_strip_color_component_format_t color_component_format;
};
struct led_strip_rmt_config_t {
  rmt_clock_source_t clk_src;
  uint32_t resolution_hz;
  uint32_t mem_block_symbols;
};
struct led_indicator_strips_config_t {
  led_strip_config_t led_strip_cfg;
  led_strip_driver_t led_strip_driver;
  led_strip_rmt_config_t led_strip_rmt_cfg;
};
esp_err_t led_indicator_new_strips_device(const led_indicator_config_t* config,
                                          const led_indicator_strips_config_t* strip, led_indicator_handle_t* handle);
const led_indicator_strips_config_t& led_test_strip_config();
