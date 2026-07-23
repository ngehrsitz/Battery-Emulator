#ifndef __HW_DFROBOT_EDGE101_H__
#define __HW_DFROBOT_EDGE101_H__

#include "hal.h"

/*
DFRobot Edge101 IOT Controller (SKU DFR0886)
ESP32-WROOM-32E, 16 MB flash, no PSRAM. USB-serial via CH9102F.
Isolated CAN (TJA1050), isolated RS485 (TPT75176H), microSD (SPI),
IP101GRI Ethernet PHY (RMII), PCF8563T RTC (I2C 0x51).
Pin map: see arduino-esp32 PR #12742 variants/dfrobot_edge101/pins_arduino.h.
*/

// NOTE: HW_HAS_ETHERNET is set as a -D build flag in platformio.ini (not #define'd
// here). It must be visible at file scope; this header is included from inside
// init_hal()'s function body, so a #define here would never reach other TUs.

class DFRobotEdge101Hal : public Esp32Hal {
 public:
  const char* name() { return "DFRobot Edge101 IOT Controller"; }

  // RS485 via TPT75176H (galvanically isolated), DE and /RE are tied together
  virtual gpio_num_t RS485_TX_PIN() { return GPIO_NUM_17; }
  virtual gpio_num_t RS485_RX_PIN() { return GPIO_NUM_36; }
  virtual gpio_num_t RS485_DE_PIN() { return GPIO_NUM_16; }

  // Native CAN via TJA1050 (galvanically isolated)
  virtual gpio_num_t CAN_TX_PIN() { return GPIO_NUM_32; }
  virtual gpio_num_t CAN_RX_PIN() { return GPIO_NUM_35; }

  // microSD (SPI)
  virtual gpio_num_t SD_MOSI_PIN() { return GPIO_NUM_12; }
  virtual gpio_num_t SD_MISO_PIN() { return GPIO_NUM_39; }
  virtual gpio_num_t SD_SCLK_PIN() { return GPIO_NUM_14; }
  virtual gpio_num_t SD_CS_PIN() { return GPIO_NUM_5; }

  // User LED
  virtual gpio_num_t LED_PIN() { return GPIO_NUM_15; }

  // Momentary push-button that can be long-pressed at runtime to start the Wi-Fi AP.
  // GPIO 38 is input-only on ESP32, no internal pull-up available; board has external pull-up.
  virtual gpio_num_t AP_BUTTON_PIN() { return GPIO_NUM_38; }

  // On-board IP101GRI Ethernet PHY (RMII).
  // GPIO0 is the 50 MHz RMII clock input (ETH_CLOCK_GPIO0_IN); data pins are
  // fixed by the EMAC hardware. <ETH.h> excluded — see NOTE above class definition.
  virtual bool HAS_ETH() override { return true; }
  virtual int ETH_PHY_TYPE_ID() override { return ETH_PHY_KIND_IP101; }
  virtual int ETH_PHY_ADDR_NUM() override { return 1; }
  virtual gpio_num_t ETH_PHY_MDC_PIN() override { return GPIO_NUM_4; }
  virtual gpio_num_t ETH_PHY_MDIO_PIN() override { return GPIO_NUM_13; }
  virtual gpio_num_t ETH_PHY_POWER_PIN() override { return GPIO_NUM_2; }
  virtual int ETH_CLK_MODE_ID() override { return ETH_CLK_KIND_GPIO0_IN; }

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
      default:
        return Esp32Hal::name_for_comm_interface(comm);
    }
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
