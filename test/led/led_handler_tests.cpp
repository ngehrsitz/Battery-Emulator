#include <Arduino.h>
#include <gtest/gtest.h>
#include <led_indicator.h>
#include <led_indicator_ledc.h>
#include <led_indicator_strips.h>
#include <algorithm>
#include <memory>
#include "src/datalayer/battery_aggregate.h"
#include "src/devboard/hal/hal.h"
#include "src/devboard/utils/led_handler.h"
#ifdef HW_DFROBOT_EDGE101
#include "src/devboard/hal/hw_dfrobot_edge101.h"
#endif

uint64_t millis64();

namespace {
EMULATOR_STATUS emulator_status = EMULATOR_STATUS::STATUS_OK;

#ifdef HW_DFROBOT_EDGE101
using TestHalBase = DFRobotEdge101Hal;
#else
class TestHalBase : public Esp32Hal {
 public:
  const char* name() override { return "Test RGB indicator chain"; }
  std::vector<comm_interface> available_interfaces() override { return {}; }
};
#endif
class TestHal : public TestHalBase {
 public:
  uint8_t maximum = 40;
  gpio_num_t pin = GPIO_NUM_15;
  gpio_num_t LED_PIN() override { return pin; }
  uint8_t LED_MAX_BRIGHTNESS() override { return maximum; }
  uint8_t LED_COUNT() override {
#ifdef HW_DFROBOT_EDGE101
    return 1;
#else
    return 5;
#endif
  }
};
std::unique_ptr<TestHal> test_hal;

uint32_t program_duration(int program) {
  uint32_t total = 0;
  for (const auto& step : led_test_program(program)) {
    total += step.hold_time_ms;
  }
  return total;
}

// Reference the previous renderer's integer calculations, independent of the
// new keyframe sampler. Compare every 10 ms sample, including low brightness.
uint8_t legacy_brightness(led_mode_enum mode, bool warning, int32_t power, uint16_t ms, uint8_t maximum) {
  if (mode != HEARTBEAT) {
    const uint16_t middle = mode == FLOW && (power < -200 || power > 200) ? 2910 : 1500;
    const uint8_t up_down =
        ms < middle ? uint32_t(ms) * maximum / middle : maximum - uint32_t(ms - middle) * maximum / (3000 - middle);
    return mode == FLOW && power < -200 ? maximum - up_down : up_down;
  }
  const uint16_t phase = uint32_t(ms) * 1000 / (warning ? 857 : 1714);
  const auto map = [phase](int32_t from, int32_t to, int32_t low, int32_t high) {
    return low + (phase - from) * (high - low) / (to - from);
  };
  int32_t scaled;
  if (phase < 100) {
    scaled = map(0, 100, 150, 100);
  } else if (phase < 200) {
    scaled = map(100, 200, 100, 50);
  } else if (phase < 250) {
    scaled = map(200, 250, 50, 800);
  } else if (phase < 300) {
    scaled = map(250, 300, 800, 100);
  } else if (phase < 400) {
    scaled = map(300, 400, 100, 550);
  } else if (phase < 550) {
    scaled = map(400, 550, 550, 250);
  } else {
    scaled = map(550, 1000, 250, 150);
  }
  return scaled * maximum / 1000;
}

class LedHandlerTest : public testing::Test {
 protected:
  uint64_t epoch = 1;

  void SetUp() override {
    led_test_fail_creation(false);
    datalayer = DataLayer{};
    emulator_status = EMULATOR_STATUS::STATUS_OK;
    set_led_feedback(LedFeedback::NONE);
    for (uint8_t i = 0; i < 4; i++) {
      set_indicator_led(static_cast<IndicatorLed>(i), false);
    }
    set_millis64(0);
    test_hal = std::make_unique<TestHal>();
    ASSERT_TRUE(initialize());
    led_test_clear_calls();
  }

  bool initialize() {
    // A fresh HAL resets its GPIO allocator without copying allocated pins.
    auto next = std::make_unique<TestHal>();
    next->maximum = test_hal->maximum;
    next->pin = test_hal->pin;
    test_hal = std::move(next);
    esp32hal = test_hal.get();
    const bool ok = led_init();
    if (ok && test_hal->pin != GPIO_NUM_NC) {
      epoch = millis64() + 1;
      led_test_advance_to(epoch);
    }
    return ok;
  }

  void select() {
    led_test_clear_calls();
    led_exe();
    if (std::any_of(led_test_calls().begin(), led_test_calls().end(),
                    [](const auto& call) { return call.type == LedTestCallType::START; })) {
      epoch = millis64() + 1;
      led_test_advance_to(epoch);
    }
  }

  void feedback(LedFeedback tier) {
    set_led_feedback(tier);
    led_exe();
    epoch = millis64() + 1;
    led_test_advance_to(epoch);
  }

  void at(uint32_t phase_ms, uint8_t brightness, uint32_t rgb) {
    led_test_advance_to(epoch + phase_ms);
    EXPECT_EQ(led_test_brightness(), brightness) << "phase=" << phase_ms;
#ifndef HW_DFROBOT_EDGE101
    ASSERT_EQ(led_test_pixels().size(), 5u);
    EXPECT_EQ(led_test_pixels()[0], rgb) << "phase=" << phase_ms;
#else
    (void)rgb;
#endif
  }
};
}  // namespace

Esp32Hal* esp32hal = nullptr;
bool Esp32Hal::system_booted_up() {
  return true;
}
void set_event(EVENTS_ENUM_TYPE, int16_t) {}
EMULATOR_STATUS get_emulator_status() {
  return emulator_status;
}
int32_t pack_power_W(const DATALAYER_BATTERY_STATUS_TYPE& status) {
  return status.active_power_W;
}

TEST_F(LedHandlerTest, BoardHalSelectsNativeHardwareConfiguration) {
#ifdef HW_DFROBOT_EDGE101
  EXPECT_EQ(led_test_pwm_config().gpio_num, GPIO_NUM_15);
  EXPECT_FALSE(led_test_pwm_config().is_active_level_high);
  EXPECT_FALSE(led_test_pwm_config().timer_inited);
  EXPECT_EQ(led_test_pwm_config().channel, LEDC_CHANNEL_4);
  EXPECT_EQ(led_test_pwm_config().timer_num, LEDC_TIMER_2);
#else
  EXPECT_EQ(led_test_strip_config().led_strip_cfg.strip_gpio_num, GPIO_NUM_15);
  EXPECT_EQ(led_test_strip_config().led_strip_cfg.max_leds, 5u);
  EXPECT_EQ(led_test_strip_config().led_strip_cfg.led_model, LED_MODEL_WS2812);
  EXPECT_EQ(led_test_strip_config().led_strip_cfg.color_component_format, LED_STRIP_COLOR_COMPONENT_FMT_GRB);
  EXPECT_EQ(led_test_strip_config().led_strip_driver, LED_STRIP_RMT);
#endif
}

TEST_F(LedHandlerTest, ClassicKeepsItsThreeSecondWaveformWithoutPollingOutput) {
  EXPECT_EQ(program_duration(led_test_base_program()), 3000u);
  at(0, 0, 0);
  at(750, 20, 0x001400);
  at(1500, 40, 0x002800);
  at(2250, 20, 0x001400);
  at(3000, 0, 0);
  // Only the native timer ran; application polling is unnecessary for animation.
  EXPECT_TRUE(led_test_calls().empty());
}

TEST_F(LedHandlerTest, UnchangedRequestsNeverRestartThePattern) {
  at(750, 20, 0x001400);
  for (int i = 0; i < 100; i++) {
    led_exe();
  }
  EXPECT_TRUE(led_test_calls().empty());
  at(1500, 40, 0x002800);
}

TEST_F(LedHandlerTest, EqualAdjacentSamplesAreCompressedAndOnlyAbsoluteStepsAreUsed) {
  const auto steps = led_test_program(led_test_base_program());
  EXPECT_LT(steps.size(), 100u);  // Not 300 individual samples plus the loop marker.
  for (const auto& step : steps) {
#ifdef HW_DFROBOT_EDGE101
    EXPECT_TRUE(step.type == LED_BLINK_BRIGHTNESS || step.type == LED_BLINK_LOOP);
#else
    EXPECT_TRUE(step.type == LED_BLINK_RGB || step.type == LED_BLINK_LOOP);
    EXPECT_EQ((step.value >> 25) & 0x7Fu, 0u);  // Status is always pixel 0.
#endif
  }
}

TEST_F(LedHandlerTest, WarningAndUpdatingUseTheSameBrightnessWithDifferentHues) {
  emulator_status = EMULATOR_STATUS::STATUS_WARNING;
  select();
  at(750, 20, 0x141400);
  emulator_status = EMULATOR_STATUS::STATUS_UPDATING;
  select();
  at(750, 20, 0x000014);
}

TEST_F(LedHandlerTest, ErrorRemainsSteadyEvenAfterItsOneShotProgramCompletes) {
  emulator_status = EMULATOR_STATUS::STATUS_ERROR;
  select();
  EXPECT_EQ(led_test_program(led_test_base_program()).back().type, LED_BLINK_STOP);
  at(0, 40, 0x280000);
  at(5000, 40, 0x280000);
  led_test_clear_calls();
  datalayer.battery.status.led_mode = HEARTBEAT;
  led_exe();
  EXPECT_TRUE(led_test_calls().empty());
}

TEST_F(LedHandlerTest, LowBrightnessLimitsRemainVisibleAndLinear) {
  test_hal->maximum = 20;  // Stark: default native gamma would map this to zero.
  ASSERT_TRUE(initialize());
  at(750, 10, 0x000A00);
  emulator_status = EMULATOR_STATUS::STATUS_ERROR;
  select();
  at(0, 20, 0x140000);
}

TEST_F(LedHandlerTest, FlowKeepsDeadbandChargeDischargeAndAggregateSelection) {
  datalayer.battery.status.led_mode = FLOW;
  for (int32_t power : {-200, 0, 200}) {
    datalayer.battery.status.active_power_W = power;
    led_exe();
  }
  EXPECT_TRUE(led_test_calls().empty());  // Idle reuses Classic.
  at(1500, 40, 0x002800);
  datalayer.battery.status.active_power_W = 1000;
  select();
  EXPECT_EQ(program_duration(led_test_base_program()), 3000u);
  at(0, 0, 0);
  at(2910, 40, 0x002800);
  datalayer.battery.status.active_power_W = -1000;
  select();
  at(0, 40, 0x002800);
  at(2910, 0, 0);
  datalayer.system.info.configured_batteries = 2;
  datalayer.aggregate.active_power_W = 1000;
  select();
  at(0, 0, 0);
  at(2910, 40, 0x002800);
}

TEST_F(LedHandlerTest, HeartbeatRetainsThirtyFiveAndSeventyBpmPeriods) {
  datalayer.battery.status.led_mode = HEARTBEAT;
  select();
  EXPECT_EQ(program_duration(led_test_base_program()), 1714u);
  at(0, 6, 0x000600);
  at(400, 21, 0x001500);
  at(600, 13, 0x000D00);
  at(1714, 6, 0x000600);  // Fractional last sample must not lengthen the period.
  emulator_status = EMULATOR_STATUS::STATUS_WARNING;
  select();
  EXPECT_EQ(program_duration(led_test_base_program()), 857u);
  at(200, 21, 0x151500);
}

TEST_F(LedHandlerTest, GeneratedProgramsMatchEveryLegacySampleAtMultipleBrightnessLimits) {
  struct Scenario {
    led_mode_enum mode;
    bool warning;
    int32_t power;
    uint16_t period;
  };
  for (uint8_t maximum : {20, 40, 255}) {
    test_hal->maximum = maximum;
    for (const auto& scenario :
         {Scenario{CLASSIC, false, 0, 3000}, Scenario{FLOW, false, 1000, 3000}, Scenario{FLOW, false, -1000, 3000},
          Scenario{HEARTBEAT, false, 0, 1714}, Scenario{HEARTBEAT, true, 0, 857}}) {
      datalayer.battery.status.led_mode = scenario.mode;
      datalayer.battery.status.active_power_W = scenario.power;
      emulator_status = scenario.warning ? EMULATOR_STATUS::STATUS_WARNING : EMULATOR_STATUS::STATUS_OK;
      ASSERT_TRUE(initialize());
      EXPECT_EQ(program_duration(led_test_base_program()), scenario.period);
      for (uint16_t ms = 0; ms < scenario.period; ms += 10) {
        const uint8_t expected = legacy_brightness(scenario.mode, scenario.warning, scenario.power, ms, maximum);
        const uint32_t rgb = uint32_t(expected) << 8 | (scenario.warning ? uint32_t(expected) << 16 : 0);
        at(ms, expected, rgb);
      }
    }
  }
}

TEST_F(LedHandlerTest, ButtonTiersPreemptEvenErrorsAndRestoreTheCompletedErrorProgram) {
  emulator_status = EMULATOR_STATUS::STATUS_ERROR;
  select();
  for (const auto& entry : {std::pair{LedFeedback::AP_READY, 400u}, std::pair{LedFeedback::NETWORK_RESET_READY, 200u},
                            std::pair{LedFeedback::FACTORY_RESET_READY, 100u}}) {
    feedback(entry.first);
    EXPECT_EQ(program_duration(led_test_preempt_program()), entry.second);
    at(0, 0, 0);
    at(entry.second / 2, 255, 0xFFFFFF);
    at(entry.second, 0, 0);
  }
  feedback(LedFeedback::NONE);
  at(0, 40, 0x280000);  // Immediate restoration, not a wait for the old blink timer.
  at(1000, 40, 0x280000);
}

TEST_F(LedHandlerTest, RepeatedButtonRequestsDoNotResetBlinkPhase) {
  feedback(LedFeedback::FACTORY_RESET_READY);
  at(50, 255, 0xFFFFFF);
  led_test_clear_calls();
  for (int i = 0; i < 100; i++) {
    set_led_feedback(LedFeedback::FACTORY_RESET_READY);
    led_exe();
  }
  EXPECT_TRUE(led_test_calls().empty());
  at(100, 0, 0);
}

TEST_F(LedHandlerTest, StateChangesWhilePreemptedTakeEffectOnlyWhenFeedbackEnds) {
  feedback(LedFeedback::AP_READY);
  at(200, 255, 0xFFFFFF);
  datalayer.battery.status.led_mode = HEARTBEAT;
  emulator_status = EMULATOR_STATUS::STATUS_WARNING;
  led_exe();
  EXPECT_EQ(led_test_brightness(), 255);  // Preparing a new base did not affect feedback.
  feedback(LedFeedback::NONE);
  at(0, 6, 0x060600);
  at(200, 21, 0x151500);
}

#ifndef HW_DFROBOT_EDGE101
TEST_F(LedHandlerTest, AuxiliaryPixelsStayFixedWhileStatusAnimatesAndChangesHue) {
  set_indicator_led(IndicatorLed::CONTACTOR_POS, true);
  led_exe();
  EXPECT_EQ(led_test_pixels()[3], 0x002800u);
  at(750, 20, 0x001400);
  EXPECT_EQ(led_test_pixels()[3], 0x002800u);
  emulator_status = EMULATOR_STATUS::STATUS_WARNING;
  select();
  at(750, 20, 0x141400);  // Indexed auxiliary writes cannot contaminate the status hue.
  EXPECT_EQ(led_test_pixels()[3], 0x282800u);
  EXPECT_EQ(led_test_pixels()[1], 0u);
  EXPECT_EQ(led_test_pixels()[2], 0u);
  EXPECT_EQ(led_test_pixels()[4], 0u);
}

TEST_F(LedHandlerTest, AuxiliaryUpdatesFreezeDuringFeedbackAndCatchUpAfterRelease) {
  feedback(LedFeedback::AP_READY);
  set_indicator_led(IndicatorLed::PRECHARGE, true);
  led_exe();
  EXPECT_EQ(led_test_pixels()[1], 0u);
  feedback(LedFeedback::NONE);
  EXPECT_EQ(led_test_pixels()[1], 0x002800u);
}
#endif

#ifdef HW_LILYGO2CAN
TEST_F(LedHandlerTest, GrbMenuChoicesSwapRedGreenAndSwitchBackWithoutStickyDriverState) {
  datalayer.battery.status.led_mode = GRB_CLASSIC;
  select();
  at(750, 20, 0x140000);
  datalayer.battery.status.led_mode = CLASSIC;
  select();
  at(750, 20, 0x001400);
}
#endif

TEST_F(LedHandlerTest, MissingPinAndNativeCreationFailuresMakeAllRequestsNoOps) {
  led_test_fail_creation(true);
  EXPECT_FALSE(initialize());
  led_test_clear_calls();
  set_led_feedback(LedFeedback::FACTORY_RESET_READY);
  set_indicator_led(IndicatorLed::PRECHARGE, true);
  led_exe();
  EXPECT_TRUE(led_test_calls().empty());
  test_hal->pin = GPIO_NUM_NC;
  EXPECT_TRUE(initialize());
  led_exe();
  EXPECT_TRUE(led_test_calls().empty());
}

TEST_F(LedHandlerTest, TimerDrivenPatternsAreIndependentOfMillisWrap) {
  set_millis64(0xFFFFFFFFULL - 100);
  ASSERT_TRUE(initialize());
  at(750, 20, 0x001400);
  at(1500, 40, 0x002800);
  at(3000, 0, 0);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
