#include "led_handler.h"
#include <led_indicator.h>
#include <led_indicator_ledc.h>
#include <led_indicator_strips.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "../../datalayer/battery_aggregate.h"
#include "../../devboard/hal/hal.h"

namespace {
constexpr uint16_t SAMPLE_MS = 10;
constexpr uint16_t FLOW_PERIOD_MS = 3000;
constexpr int32_t FLOW_IDLE_BAND_W = 200;
constexpr uint32_t WHITE = 0xFFFFFF;
#ifdef HW_DFROBOT_EDGE101
constexpr bool PWM_LED = true;
#else
constexpr bool PWM_LED = false;
#endif

// Only one base program is active. Feedback preempts it regardless of emulator
// severity, which events.cpp already resolves. Two base buffers let us prepare a
// replacement without modifying a program that the timer task is reading.
enum Program { FACTORY_FEEDBACK, NETWORK_FEEDBACK, AP_FEEDBACK, BASE_A, BASE_B, PROGRAM_COUNT };
enum class Waveform { CLASSIC, CHARGE, DISCHARGE, HEARTBEAT, WARNING_HEARTBEAT, STEADY };

struct Point {
  uint16_t time;
  int16_t value;
};

template <size_t N>
int32_t interpolate(const std::array<Point, N>& points, uint16_t time) {
  for (size_t i = 1; i < N; i++) {
    if (time < points[i].time) {
      const auto& a = points[i - 1];
      const auto& b = points[i];
      return a.value + (int32_t(b.value) - a.value) * (time - a.time) / (b.time - a.time);
    }
  }
  return points.back().value;
}

uint16_t period_ms(Waveform waveform) {
  if (waveform == Waveform::HEARTBEAT) {
    return 60000 / 35;
  }
  if (waveform == Waveform::WARNING_HEARTBEAT) {
    return 60000 / 70;
  }
  return FLOW_PERIOD_MS;
}

uint8_t sample(Waveform waveform, uint16_t ms, uint8_t maximum) {
  if (waveform == Waveform::HEARTBEAT || waveform == Waveform::WARNING_HEARTBEAT) {
    // The original heartbeat's phase and brightness are both scaled to 0-1000.
    static constexpr std::array<Point, 8> heartbeat = {
        {{0, 150}, {100, 100}, {200, 50}, {250, 800}, {300, 100}, {400, 550}, {550, 250}, {1000, 150}}};
    const uint16_t phase = uint32_t(ms) * 1000 / period_ms(waveform);
    return interpolate(heartbeat, phase) * maximum / 1000;
  }
  const uint16_t peak_ms = waveform == Waveform::CLASSIC ? 1500 : 2910;
  const std::array<Point, 3> ramp = {{{0, 0}, {peak_ms, maximum}, {FLOW_PERIOD_MS, 0}}};
  const uint8_t brightness = interpolate(ramp, ms);
  return waveform == Waveform::DISCHARGE ? maximum - brightness : brightness;
}

struct BaseState {
  EMULATOR_STATUS status;
  Waveform waveform;
  bool swap_red_green;

  bool operator==(const BaseState& other) const {
    return status == other.status && waveform == other.waveform && swap_red_green == other.swap_red_green;
  }
};

BaseState requested_base() {
  const auto status = get_emulator_status();
  led_mode_enum mode = datalayer.battery.status.led_mode;
  bool swap = false;
#ifdef HW_LILYGO2CAN
  // Preserve the existing GRB menu choices (which swapped R/G in the old driver).
  switch (mode) {
    case GRB_CLASSIC:
      mode = CLASSIC;
      swap = true;
      break;
    case GRB_FLOW:
      mode = FLOW;
      swap = true;
      break;
    case GRB_HEARTBEAT:
      mode = HEARTBEAT;
      swap = true;
      break;
    default:
      break;
  }
#endif
  Waveform waveform = Waveform::CLASSIC;
  if (status == EMULATOR_STATUS::STATUS_ERROR) {
    waveform = Waveform::STEADY;
  } else if (mode == HEARTBEAT) {
    waveform = status == EMULATOR_STATUS::STATUS_WARNING ? Waveform::WARNING_HEARTBEAT : Waveform::HEARTBEAT;
  } else if (mode == FLOW) {
    const int32_t power = datalayer.system.info.configured_batteries > 1 ? datalayer.aggregate.active_power_W
                                                                         : pack_power_W(datalayer.battery.status);
    if (power > FLOW_IDLE_BAND_W) {
      waveform = Waveform::CHARGE;
    } else if (power < -FLOW_IDLE_BAND_W) {
      waveform = Waveform::DISCHARGE;
    }
  }
  return {status, waveform, swap};
}

uint32_t hue(EMULATOR_STATUS status) {
  switch (status) {
    case EMULATOR_STATUS::STATUS_WARNING:
      return 0xFFFF00;
    case EMULATOR_STATUS::STATUS_ERROR:
      return 0xFF0000;
    case EMULATOR_STATUS::STATUS_UPDATING:
      return 0x0000FF;
    default:
      return 0x00FF00;
  }
}

uint32_t color(uint32_t hue, uint8_t brightness, bool swap) {
  uint32_t r = ((hue >> 16) & 0xFF) * brightness / 255;
  uint32_t g = ((hue >> 8) & 0xFF) * brightness / 255;
  const uint32_t b = (hue & 0xFF) * brightness / 255;
  if (swap) {
    std::swap(r, g);
  }
  return SET_RGB(r, g, b);
}

struct Controller {
  led_indicator_handle_t handle = nullptr;
  uint8_t maximum;
  uint8_t count;
  std::vector<blink_step_t> base[2];
  std::array<std::array<blink_step_t, 3>, 3> feedback;
  std::array<const blink_step_t*, PROGRAM_COUNT> programs{};
  BaseState active_base{};
  bool base_valid = false;
  int base_program = BASE_A;
  LedFeedback active_feedback = LedFeedback::NONE;
  uint8_t last_indicator_mask = 0xFF;
  uint32_t last_indicator_color = UINT32_MAX;

  Controller(uint8_t maximum, uint8_t count) : maximum(maximum), count(count) {
    // Equal adjacent samples are merged. Reserve a bounded amount for the usual
    // 20/40 brightness limits instead of storing every 10 ms sample permanently.
    for (auto& buffer : base) {
      buffer.reserve(std::min<size_t>(FLOW_PERIOD_MS / SAMPLE_MS + 1, size_t(maximum) * 3 + 10));
    }
    for (size_t i = 0; i < feedback.size(); i++) {
      const uint16_t half_period = 50 << i;
      feedback[i] = {{{step_type(), value(WHITE, 0, false), half_period},
                      {step_type(), value(WHITE, 255, false), half_period},
                      {LED_BLINK_LOOP, 0, 0}}};
      programs[i] = feedback[i].data();
    }
  }

  ~Controller() {
    if (handle) {
      // Stop/delete the timer before destroying the program buffers it reads.
      led_indicator_delete(handle);
    }
  }

  blink_step_type_t step_type() const { return PWM_LED ? LED_BLINK_BRIGHTNESS : LED_BLINK_RGB; }
  uint32_t value(uint32_t hue, uint8_t brightness, bool swap) const {
    return PWM_LED ? brightness : color(hue, brightness, swap);  // Absolute RGB for status pixel 0.
  }

  void select_base(const BaseState& state) {
    if (base_valid && state == active_base) {
      return;
    }
    const int next = base_valid && base_program == BASE_A ? BASE_B : BASE_A;
    auto& buffer = base[next - BASE_A];
    buffer.clear();
    const uint32_t status_hue = hue(state.status);
    if (state.waveform == Waveform::STEADY) {
      // STOP leaves the last output latched. No lower base program is active.
      buffer.push_back({step_type(), value(status_hue, maximum, state.swap_red_green), 0});
      buffer.push_back({LED_BLINK_STOP, 0, 0});
    } else {
      const uint16_t period = period_ms(state.waveform);
      for (uint16_t ms = 0; ms < period; ms += SAMPLE_MS) {
        const uint32_t sample_value = value(status_hue, sample(state.waveform, ms, maximum), state.swap_red_green);
        const uint16_t hold = std::min<uint16_t>(SAMPLE_MS, period - ms);
        if (!buffer.empty() && buffer.back().value == sample_value) {
          buffer.back().hold_time_ms += hold;
        } else {
          buffer.push_back({step_type(), sample_value, hold});
        }
      }
      buffer.push_back({LED_BLINK_LOOP, 0, 0});
    }
    // The next slot is inactive; publish it before start() takes the engine mutex.
    programs[next] = buffer.data();
    if (base_valid) {
      led_indicator_stop(handle, base_program);
    }
    base_program = next;
    active_base = state;
    base_valid = true;
    led_indicator_start(handle, base_program);
  }
};

std::unique_ptr<Controller> controller;
std::atomic<LedFeedback> feedback_request{LedFeedback::NONE};
std::atomic<uint8_t> indicator_mask{0};

int feedback_program(LedFeedback feedback) {
  switch (feedback) {
    case LedFeedback::FACTORY_RESET_READY:
      return FACTORY_FEEDBACK;
    case LedFeedback::NETWORK_RESET_READY:
      return NETWORK_FEEDBACK;
    default:
      return AP_FEEDBACK;
  }
}
}  // namespace

void set_led_feedback(LedFeedback feedback) {
  feedback_request.store(feedback, std::memory_order_relaxed);
}

void set_indicator_led(IndicatorLed indicator, bool on) {
  const auto index = static_cast<uint8_t>(indicator);
  if (index >= 4) {
    return;
  }
  const uint8_t bit = 1u << index;
  if (on) {
    indicator_mask.fetch_or(bit, std::memory_order_relaxed);
  } else {
    indicator_mask.fetch_and(static_cast<uint8_t>(~bit), std::memory_order_relaxed);
  }
}

bool led_init(void) {
  controller.reset();
  const auto pin = esp32hal->LED_PIN();
  if (pin == GPIO_NUM_NC) {
    return true;
  }
  if (!esp32hal->alloc_pins("LED", pin)) {
    DEBUG_PRINTF("LED setup failed\n");
    return false;
  }
  auto next = std::make_unique<Controller>(esp32hal->LED_MAX_BRIGHTNESS(), esp32hal->LED_COUNT());
  led_indicator_config_t config = {};
  config.blink_lists = next->programs.data();
  config.blink_list_num = PROGRAM_COUNT;
  esp_err_t result;
  if constexpr (PWM_LED) {
    led_indicator_ledc_config_t pwm = {};
    pwm.gpio_num = pin;
    pwm.is_active_level_high = esp32hal->LED_ACTIVE_LEVEL_HIGH();
    // sdkconfig fixes this component to the low-speed group. On classic ESP32 it
    // is separate from the Arduino contactor/precharge channels in the high-speed group.
    pwm.channel = LEDC_CHANNEL_4;
    pwm.timer_num = LEDC_TIMER_2;
    result = led_indicator_new_ledc_device(&config, &pwm, &next->handle);
  } else {
    led_indicator_strips_config_t strip = {};
    strip.led_strip_cfg.strip_gpio_num = pin;
    strip.led_strip_cfg.max_leds = next->count;
    strip.led_strip_cfg.led_model = LED_MODEL_WS2812;
    // The old driver's default offsets transmitted G/R/B, despite calling its mode "RGB".
    strip.led_strip_cfg.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    strip.led_strip_driver = LED_STRIP_RMT;
    strip.led_strip_rmt_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
    strip.led_strip_rmt_cfg.resolution_hz = 10000000;
    strip.led_strip_rmt_cfg.mem_block_symbols = 64;
    result = led_indicator_new_strips_device(&config, &strip, &next->handle);
  }
  if (result != ESP_OK) {
    return false;  // Native drivers report errors; no handle means no LED output.
  }
  controller = std::move(next);
  led_exe();
  return true;
}

void led_exe(void) {
  if (!controller) {
    return;
  }
  auto& led = *controller;
  led.select_base(requested_base());
  const auto requested_feedback = feedback_request.load(std::memory_order_relaxed);
  if (requested_feedback != led.active_feedback) {
    if (requested_feedback == LedFeedback::NONE) {
      led_indicator_preempt_stop(led.handle, feedback_program(led.active_feedback));
      // stop() doesn't wake the native timer immediately. Restart explicitly to
      // restore the base hue (including a completed steady-error program).
      led_indicator_start(led.handle, led.base_program);
    } else {
      led_indicator_preempt_start(led.handle, feedback_program(requested_feedback));
    }
    led.active_feedback = requested_feedback;
  }
  if constexpr (PWM_LED) {
    return;
  }
  if (led.active_feedback != LedFeedback::NONE || led.count <= 1) {
    return;  // Preserve the old behaviour: auxiliary pixels freeze during feedback.
  }
  const uint8_t mask = indicator_mask.load(std::memory_order_relaxed);
  const uint32_t indicator_color = color(hue(led.active_base.status), led.maximum, led.active_base.swap_red_green);
  for (uint8_t i = 0; i < 4 && i + 1 < led.count; i++) {
    const uint8_t bit = 1u << i;
    if ((mask & bit) != (led.last_indicator_mask & bit) || indicator_color != led.last_indicator_color) {
      led_indicator_set_rgb(led.handle, SET_IRGB(i + 1, 0, 0, 0) | ((mask & bit) ? indicator_color : 0));
    }
  }
  led.last_indicator_mask = mask;
  led.last_indicator_color = indicator_color;
}
