#include <gtest/gtest.h>

#include "../../Software/src/battery/BATTERIES.h"  // user_selected_use_estimated_SOC
#include "../../Software/src/battery/RIVIAN-BATTERY.h"
#include "../../Software/src/datalayer/datalayer.h"

#include "Arduino.h"

/* Pin the current-limit -> power-limit conversion in the Rivian driver.

   The bug (fixed): update_values() computed max_*_power_W as
   (pre_contactor_voltage / 10) * battery_charge_limit_amp, pre-dividing the
   voltage and discarding the deci-volt before the multiply. Multiply first:
   (pre_contactor_voltage * battery_charge_limit_amp) / 10. The current limit
   stays whole Amps (decoded as raw / 20); only the lost deci-volt is recovered.

   The power calc only runs in the non-estimated-SOC branch, so the test forces
   the global flag off.

   Frames (handle_incoming_can_frame):
     0x100: 12-bit charge limit field @ (u8[3]&0x0F)<<8 | u8[2]<<4 | u8[1]>>4, /20 = A;
            discharge limit @ (u8[5]&0x0F)<<8 | u8[4]<<4 | u8[3]>>4, /20 = A
     0x120: pre_contactor_voltage (13-bit) @ (u8[7]&0x1F)<<8 | u8[6] */

namespace {

// Packs a 12-bit charge-limit field (raw = amps*20) into bytes 1-3 the way 0x100 expects.
// Only the charge field is set; discharge is left zero to keep the bit math obvious.
CAN_frame rivian_100(uint16_t charge_raw) {
  CAN_frame frame = {};
  frame.DLC = 8;
  frame.ID = 0x100;
  // charge: (u8[3]&0x0F)<<8 | u8[2]<<4 | u8[1]>>4
  frame.data.u8[1] = (charge_raw & 0x00F) << 4;
  frame.data.u8[2] = (charge_raw >> 4) & 0xFF;
  frame.data.u8[3] = (charge_raw >> 12) & 0x0F;
  return frame;
}

CAN_frame rivian_120(uint16_t voltage_dV) {
  CAN_frame frame = {};
  frame.DLC = 8;
  frame.ID = 0x120;
  frame.data.u8[6] = voltage_dV & 0xFF;
  frame.data.u8[7] = (voltage_dV >> 8) & 0x1F;
  return frame;
}

}  // namespace

// A voltage with a non-zero deci-volt digit must not lose it. 100 A @ 359.7 V:
// new (3597 * 100) / 10 = 35970 W; old (3597/10)*100 = 359*100 = 35900 W.
TEST(RivianBatteryTests, DeciVoltIsNotTruncated) {
  bool saved = user_selected_use_estimated_SOC;
  user_selected_use_estimated_SOC = false;

  auto battery = new RivianBattery();
  battery->handle_incoming_can_frame(rivian_120(3597));
  battery->handle_incoming_can_frame(rivian_100(100 * 20));  // raw = amps * 20
  battery->update_values();

  EXPECT_EQ(datalayer.battery.status.max_charge_power_W, 35970u);

  delete battery;
  user_selected_use_estimated_SOC = saved;
}

// Round value: exact-tenths results are unchanged. 50 A @ 360.0 V = 18000 W.
TEST(RivianBatteryTests, RoundVoltagePreserved) {
  bool saved = user_selected_use_estimated_SOC;
  user_selected_use_estimated_SOC = false;

  auto battery = new RivianBattery();
  battery->handle_incoming_can_frame(rivian_120(3600));
  battery->handle_incoming_can_frame(rivian_100(50 * 20));
  battery->update_values();

  EXPECT_EQ(datalayer.battery.status.max_charge_power_W, 18000u);

  delete battery;
  user_selected_use_estimated_SOC = saved;
}
