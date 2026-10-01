#include <gtest/gtest.h>

#include "../../Software/src/battery/SIMPBMS-BATTERY.h"
#include "../../Software/src/datalayer/datalayer.h"

#include "Arduino.h"

/* Pin the current-limit -> power-limit conversion in the SimpBMS driver.

   The bug (fixed): update_values() computed the power limit as
   max_charge_current * (voltage_dV / 10), and the decode of frame 0x351
   additionally truncated the raw 0.1 A/LSB current field with an integer / 10.
   Together those discarded every sub-1 A and fractional-amp limit. The decode
   now carries deci-Amps and the power uses current_dA_to_power_W(), matching the
   Pylon fix: a 0.1 A limit on a 360 V pack is 36 W, not 0 W.

   Frame layout (handle_incoming_can_frame):
     0x351: charge cutoff @0, charge current (0.1A/LSB) @2, discharge current @4, discharge cutoff @6
     0x356: voltage (0.01V/LSB -> /10 = dV) @0, current @2 */

namespace {

void put_le16(CAN_frame& frame, uint8_t index, uint16_t value) {
  frame.data.u8[index] = value & 0xFF;
  frame.data.u8[index + 1] = (value >> 8) & 0xFF;
}

CAN_frame simpbms_351(uint16_t charge_current_dA, uint16_t discharge_current_dA) {
  CAN_frame frame = {};
  frame.DLC = 8;
  frame.ID = 0x351;
  put_le16(frame, 0, 4088);  // charge cutoff voltage, unused by these assertions
  put_le16(frame, 2, charge_current_dA);
  put_le16(frame, 4, discharge_current_dA);
  put_le16(frame, 6, 2968);  // discharge cutoff voltage, unused
  return frame;
}

// 0x356 voltage field is 0.01 V/LSB; the driver divides by 10 to get deci-Volts.
CAN_frame simpbms_356(uint16_t voltage_dV) {
  CAN_frame frame = {};
  frame.DLC = 8;
  frame.ID = 0x356;
  put_le16(frame, 0, static_cast<uint16_t>(voltage_dV * 10));
  put_le16(frame, 2, 0);  // current, unused
  return frame;
}

void decode_limits(SimpBmsBattery* battery, uint16_t voltage_dV, uint16_t charge_current_dA,
                   uint16_t discharge_current_dA) {
  battery->handle_incoming_can_frame(simpbms_356(voltage_dV));
  battery->handle_incoming_can_frame(simpbms_351(charge_current_dA, discharge_current_dA));
  battery->update_values();
}

}  // namespace

// 0.1 A limit on a 360 V pack: 1 dA * 3600 dV / 100 = 36 W (was 0 W).
TEST(SimpBmsBatteryTests, SubOneAmpLimitDoesNotTruncateToZero) {
  auto battery = new SimpBmsBattery();
  decode_limits(battery, 3600, 1, 1);

  EXPECT_EQ(datalayer.battery.status.max_charge_power_W, 36u);
  EXPECT_EQ(datalayer.battery.status.max_discharge_power_W, 36u);

  delete battery;
}

// A fractional-amp mid-range limit must keep the 0.5 A: 155 dA * 3600 dV / 100 = 5580 W
// (was (155/10)*(3600/10) = 15*360 = 5400 W).
TEST(SimpBmsBatteryTests, FractionalAmpLimitIsNotDiscarded) {
  auto battery = new SimpBmsBattery();
  decode_limits(battery, 3600, 155, 155);

  EXPECT_EQ(datalayer.battery.status.max_charge_power_W, 5580u);
  EXPECT_EQ(datalayer.battery.status.max_discharge_power_W, 5580u);

  delete battery;
}
