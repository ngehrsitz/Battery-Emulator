#include <gtest/gtest.h>

#include "../../Software/src/battery/THUNDERSTRUCK-BMS.h"
#include "../../Software/src/datalayer/datalayer.h"

#include "Arduino.h"

/* Pin the current-limit -> power-limit conversion in the Thunderstruck driver.

   The bug (fixed): update_values() computed max_*_power_W as
   DCLMax * (packvoltage_dV / 10), pre-dividing the voltage and discarding the
   deci-volt before the multiply. Now it multiplies first:
   (packvoltage_dV * DCLMax) / 10. DCLMax/CCLMax are whole Amps (1 A/LSB, no
   scale at decode), so this only recovers the lost deci-volt.

   Frames are big-endian (handle_incoming_can_frame):
     0x14ff25d0: DCLMin @0, CCLMin @2, DCLMax @4, CCLMax @6
     0x14ff21d0: packvoltage_dV @2, pack_current_dA @4 */

namespace {

void put_be16(CAN_frame& frame, uint8_t index, uint16_t value) {
  frame.data.u8[index] = (value >> 8) & 0xFF;
  frame.data.u8[index + 1] = value & 0xFF;
}

CAN_frame thunderstruck_limits(uint16_t dcl_max, uint16_t ccl_max) {
  CAN_frame frame = {};
  frame.DLC = 8;
  frame.ID = 0x14ff25d0;
  put_be16(frame, 0, 0);  // DCLMin, unused
  put_be16(frame, 2, 0);  // CCLMin, unused
  put_be16(frame, 4, dcl_max);
  put_be16(frame, 6, ccl_max);
  return frame;
}

CAN_frame thunderstruck_voltage(uint16_t voltage_dV) {
  CAN_frame frame = {};
  frame.DLC = 8;
  frame.ID = 0x14ff21d0;
  put_be16(frame, 2, voltage_dV);
  put_be16(frame, 4, 0);  // pack current, unused
  return frame;
}

void decode(ThunderstruckBMS* battery, uint16_t voltage_dV, uint16_t dcl_max, uint16_t ccl_max) {
  battery->handle_incoming_can_frame(thunderstruck_voltage(voltage_dV));
  battery->handle_incoming_can_frame(thunderstruck_limits(dcl_max, ccl_max));
  battery->update_values();
}

}  // namespace

// A voltage with a non-zero deci-volt digit must not lose it. 100 A @ 359.7 V:
// new (3597 * 100) / 10 = 35970 W; old 100 * (3597/10) = 100*359 = 35900 W.
TEST(ThunderstruckBmsTests, DeciVoltIsNotTruncated) {
  auto battery = new ThunderstruckBMS();
  decode(battery, 3597, 100, 90);

  EXPECT_EQ(datalayer.battery.status.max_discharge_power_W, 35970u);  // 100 A discharge
  EXPECT_EQ(datalayer.battery.status.max_charge_power_W, 32373u);     // 90 A: 3597*90/10

  delete battery;
}

// Round value: the fix leaves exact-tenths results unchanged. 50 A @ 360.0 V = 18000 W.
TEST(ThunderstruckBmsTests, RoundVoltagePreserved) {
  auto battery = new ThunderstruckBMS();
  decode(battery, 3600, 50, 50);

  EXPECT_EQ(datalayer.battery.status.max_discharge_power_W, 18000u);
  EXPECT_EQ(datalayer.battery.status.max_charge_power_W, 18000u);

  delete battery;
}
