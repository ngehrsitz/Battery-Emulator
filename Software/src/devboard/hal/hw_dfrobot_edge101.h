#ifndef __HW_DFROBOT_EDGE101_H__
#define __HW_DFROBOT_EDGE101_H__

#include "hal.h"

// DFRobot Edge101 IOT Controller (SKU DFR0886)
// ESP32-WROOM-32E, 16 MB flash, no PSRAM.
// USB-serial via CH9102F. Isolated CAN (TJA1050), isolated RS485 (TPT75176H),
// microSD (SPI), IP101GRI Ethernet PHY (RMII), PCF8563T RTC (I2C 0x51),
// optional mini-PCIe 4G modem slot on UART1.
//
// Pin map: see arduino-esp32 PR #12742 variants/dfrobot_edge101/pins_arduino.h.
//
// Unused-on-board peripherals (documented here for a future PR):
//   Ethernet (IP101GRI, RMII):
//     ETH_PHY_TYPE  = ETH_PHY_IP101
//     ETH_PHY_ADDR  = 1
//     ETH_PHY_MDC   = GPIO 4
//     ETH_PHY_MDIO  = GPIO 13
//     ETH_PHY_POWER = GPIO 2
//     ETH_CLK_MODE  = ETH_CLOCK_GPIO0_IN
//   I2C (PCF8563T RTC + Gravity connectors):
//     SDA = GPIO 18, SCL = GPIO 23; PCF8563T at I2C address 0x51
//   UART1 (mini-PCIe 4G modem slot):
//     TX1 = GPIO 33, RX1 = GPIO 34

class DFRobotEdge101Hal : public Esp32Hal {
 public:
  const char* name() { return "DFRobot Edge101 IOT Controller"; }

  // Native CAN via TJA1050 (galvanically isolated)
  virtual gpio_num_t CAN_TX_PIN() { return GPIO_NUM_32; }
  virtual gpio_num_t CAN_RX_PIN() { return GPIO_NUM_35; }

  // RS485 via TPT75176H (galvanically isolated)
  // DE and /RE are tied together
  virtual gpio_num_t RS485_TX_PIN() { return GPIO_NUM_17; }
  virtual gpio_num_t RS485_RX_PIN() { return GPIO_NUM_36; }
  virtual gpio_num_t RS485_DE_PIN() { return GPIO_NUM_16; }

  // microSD (SPI)
  virtual gpio_num_t SD_MOSI_PIN() { return GPIO_NUM_12; }
  virtual gpio_num_t SD_MISO_PIN() { return GPIO_NUM_39; }
  virtual gpio_num_t SD_SCLK_PIN() { return GPIO_NUM_14; }
  virtual gpio_num_t SD_CS_PIN() { return GPIO_NUM_5; }

  // User LED
  virtual gpio_num_t LED_PIN() { return GPIO_NUM_15; }

  // User button — GPIO 38 is input-only on ESP32, no internal pull-up available; board has external pull-up
  virtual gpio_num_t AP_BUTTON_PIN() { return GPIO_NUM_38; }

  std::vector<comm_interface> available_interfaces() {
    return {comm_interface::Modbus, comm_interface::RS485, comm_interface::CanNative};
  }

  virtual const char* name_for_comm_interface(comm_interface comm) {
    switch (comm) {
      case comm_interface::CanNative:
        return "CAN (Native)";
      case comm_interface::CanFdNative:
        return "";
      case comm_interface::CanAddonMcp2515:
        return "";
      case comm_interface::CanFdAddonMcp2518:
        return "";
      case comm_interface::Modbus:
        return "Modbus";
      case comm_interface::RS485:
        return "RS485";
      case comm_interface::Highest:
        return "";
    }
    return Esp32Hal::name_for_comm_interface(comm);
  }
};

#define HalClass DFRobotEdge101Hal

/* ----- Error checks below, don't change (can't be moved to separate file) ----- */
#ifndef HW_CONFIGURED
#define HW_CONFIGURED
#else
#error Multiple HW defined! Please select a single HW
#endif

#endif
