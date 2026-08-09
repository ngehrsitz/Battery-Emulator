#ifndef _HAL_H_
#define _HAL_H_

#include <SPI.h>
#include <soc/gpio_num.h>
#include <chrono>
#include <unordered_map>
#include "../../../src/communication/nvm/comm_nvm.h"
#include "../../../src/devboard/utils/events.h"
#include "../../../src/devboard/utils/logging.h"
#include "../../../src/devboard/utils/types.h"

// Hardware Abstraction Layer base class.
// Derive a class to define board-specific parameters such as GPIO pin numbers
// This base class implements a mechanism for allocating GPIOs.
class Esp32Hal {
 public:
  virtual const char* name() = 0;

  // Time it takes before system is considered fully started up.
  virtual duration BOOTUP_TIME() { return milliseconds(1000); }
  virtual bool system_booted_up();

  // Core assignment
  virtual int CORE_FUNCTION_CORE() { return 1; }
  virtual int MODBUS_CORE() { return 0; }
  virtual int WIFICORE() { return 0; }

  virtual void set_default_configuration_values() {}

  template <typename... Pins>
  bool alloc_pins(const char* name, Pins... pins) {
    std::vector<gpio_num_t> requested_pins = {static_cast<gpio_num_t>(pins)...};

    for (gpio_num_t pin : requested_pins) {
      if (pin < 0) {
        set_event(EVENT_GPIO_NOT_DEFINED, (int)pin);
        allocator_name = name;
        DEBUG_PRINTF("%s attempted to allocate pin %d that wasn't defined for the selected HW.\n", name, (int)pin);
        return false;
      }

      auto it = allocated_pins.find(pin);
      if (it != allocated_pins.end()) {
        allocator_name = name;
        allocated_name = it->second.c_str();
        DEBUG_PRINTF("GPIO conflict for pin %d between %s and %s.\n", (int)pin, name, it->second.c_str());
        set_event(EVENT_GPIO_CONFLICT, (int)pin);
        return false;
      }
    }

    for (gpio_num_t pin : requested_pins) {
      allocated_pins[pin] = name;
    }

    return true;
  }

  // Helper to forward vector to variadic template
  template <typename Vec, size_t... Is>
  bool alloc_pins_from_vector(const char* name, const Vec& pins, std::index_sequence<Is...>) {
    return alloc_pins(name, pins[Is]...);
  }

  // Base case: no more pins
  inline bool alloc_pins_ignore_unused_impl(const char* name) {
    return alloc_pins(name);  // Call with 0 pins
  }

  // Recursive case: process one pin at a time
  template <typename... Rest>
  bool alloc_pins_ignore_unused_impl(const char* name, gpio_num_t first, Rest... rest) {
    if (first == GPIO_NUM_NC) {
      return alloc_pins_ignore_unused_impl(name, rest...);
    } else {
      return call_alloc_pins_filtered(name, first, rest...);
    }
  }

  // This helper just forwards pins after filtering is done
  template <typename... Pins>
  bool call_alloc_pins_filtered(const char* name, Pins... pins) {
    return alloc_pins(name, pins...);
  }

  // Entry point
  template <typename... Pins>
  bool alloc_pins_ignore_unused(const char* name, Pins... pins) {
    return alloc_pins_ignore_unused_impl(name, static_cast<gpio_num_t>(pins)...);
  }

  virtual bool always_enable_bms_power() { return false; }

  virtual gpio_num_t PIN_5V_EN() { return GPIO_NUM_NC; }
  virtual gpio_num_t RS485_EN_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t RS485_TX_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t RS485_RX_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t RS485_SE_PIN() { return GPIO_NUM_NC; }

  // Direction pin for half-duplex RS485 transceivers with DE and /RE tied together.
  // Default polarity: HIGH = transmit, LOW = receive.
  virtual gpio_num_t RS485_DE_PIN() { return GPIO_NUM_NC; }
  virtual bool RS485_DE_ACTIVE_HIGH() { return true; }

  virtual gpio_num_t CAN_TX_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t CAN_RX_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t CAN_SE_PIN() { return GPIO_NUM_NC; }

  // CAN_ADDON
  virtual SPIClass& MCP2515_SPI() = 0;
  virtual gpio_num_t MCP2515_CS()  { return GPIO_NUM_NC; }
  virtual gpio_num_t MCP2515_INT() { return GPIO_NUM_NC; }
  virtual gpio_num_t MCP2515_RST() { return GPIO_NUM_NC; }
  virtual uint32_t MCP2515_FREQ()  { return 0; }  // 0 means unknown

  // CANFD_ADDON defines for MCP2517
  virtual SPIClass& MCP2517_SPI() = 0;
  virtual gpio_num_t MCP2517_CS()   { return GPIO_NUM_NC; }
  virtual gpio_num_t MCP2517_INT()  { return GPIO_NUM_NC; }
  virtual gpio_num_t MCP2517_INT0() { return GPIO_NUM_NC; }
  virtual gpio_num_t MCP2517_INT1() { return GPIO_NUM_NC; }
  virtual uint32_t MCP2517_FREQ()   { return 0; }  // 0 means unknown

  // 2nd CANFD Interface: MCP2517/8 — defaults to same bus as primary
  virtual SPIClass& MCP2517_SPI2() { return MCP2517_SPI(); }
  virtual gpio_num_t MCP2517_CS2() { return GPIO_NUM_NC; }
  virtual gpio_num_t MCP2517_INT2(){ return GPIO_NUM_NC; }
  virtual uint32_t MCP2517_FREQ2() { return 0; }  // 0 means unknown

  // Value for first MCP2517 CLKODIV register (default, divide by 10)
  virtual int MCP2517_CLKODIV() { return 0b11; }

  // CHAdeMO support pin dependencies
  virtual gpio_num_t CHADEMO_PIN_2() { return GPIO_NUM_NC; }
  virtual gpio_num_t CHADEMO_PIN_10() { return GPIO_NUM_NC; }
  virtual gpio_num_t CHADEMO_PIN_7() { return GPIO_NUM_NC; }
  virtual gpio_num_t CHADEMO_PIN_4() { return GPIO_NUM_NC; }
  virtual gpio_num_t CHADEMO_LOCK() { return GPIO_NUM_NC; }
  virtual gpio_num_t CHADEMO_CT_PIN() { return GPIO_NUM_NC; }

  // Contactor handling
  virtual gpio_num_t POSITIVE_CONTACTOR_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t NEGATIVE_CONTACTOR_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t PRECHARGE_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t BMS_POWER() { return GPIO_NUM_NC; }
  virtual gpio_num_t SECOND_BATTERY_CONTACTORS_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t TRIPLE_BATTERY_CONTACTORS_PIN() { return GPIO_NUM_NC; }

  // Output pins to latch at their driven level across a firmware-initiated reset/OTA
  // reboot, so they don't float during the boot window. RTC-capable pins only.
  virtual std::vector<gpio_num_t> reset_hold_pins() { return {}; }

  // Automatic precharging
  virtual gpio_num_t HIA4V1_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t INVERTER_DISCONNECT_CONTACTOR_PIN() { return GPIO_NUM_NC; }

  // SMA CAN contactor pins
  virtual gpio_num_t INVERTER_CONTACTOR_ENABLE_PIN() { return GPIO_NUM_NC; }

  virtual gpio_num_t INVERTER_CONTACTOR_ENABLE_LED_PIN() { return GPIO_NUM_NC; }

#ifdef SDCARD
  // SD card — boards must override SD_SPI() to return the correct bus.
  virtual SPIClass& SD_SPI() = 0;
  virtual gpio_num_t SD_CS_PIN() { return GPIO_NUM_NC; }
#endif  // SDCARD

  // Physical SPI bus pin declarations. Boards override the buses they actually wire.
  // Default: GPIO_NUM_NC — bus will not be initialized.
  struct SpiBus {
    uint8_t bus;
    gpio_num_t sck;
    gpio_num_t mosi;
    gpio_num_t miso;
  };

  // Boards override the bus methods they actually wire. Default = not connected (sck == GPIO_NUM_NC).
  virtual SpiBus HSPI_bus() { return {HSPI, GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC}; }
#ifndef CONFIG_IDF_TARGET_ESP32S3
  virtual SpiBus VSPI_bus() { return {VSPI, GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC}; }
#else
  virtual SpiBus FSPI_bus() { return {FSPI, GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC}; }
#endif

  // Initialize all declared buses. Called once from main after init_stored_settings().
  void init_spi() {
#ifndef CONFIG_IDF_TARGET_ESP32S3
    for (auto& b : {HSPI_bus(), VSPI_bus()}) {
#else
    for (auto& b : {HSPI_bus(), FSPI_bus()}) {
#endif
      if (b.sck != GPIO_NUM_NC) {
        alloc_pins("SPI", b.sck, b.mosi, b.miso);
        spi(b.bus).begin(b.sck, b.miso, b.mosi);
      }
    }
  }

  // LED
  virtual gpio_num_t LED_PIN() { return GPIO_NUM_NC; }
  virtual uint8_t LED_MAX_BRIGHTNESS() { return 40; }
  // Number of LEDs chained off LED_PIN(). Pixel 0 is always the STATUS LED; boards that replace
  // additional hardwired indicator LEDs with RGB LEDs on the same chain report more than 1 here.
  virtual uint8_t LED_COUNT() { return 1; }

#ifndef SMALL_FLASH_DEVICE
  // i2c display
  virtual gpio_num_t DISPLAY_SDA_PIN() { return GPIO_NUM_NC; }
  virtual gpio_num_t DISPLAY_SCL_PIN() { return GPIO_NUM_NC; }
#endif  // SMALL_FLASH_DEVICE

  // Equipment stop pin
  virtual gpio_num_t EQUIPMENT_STOP_PIN() { return GPIO_NUM_NC; }

  // Battery wake up pins
  virtual gpio_num_t WUP_PIN1() { return GPIO_NUM_NC; }
  virtual gpio_num_t WUP_PIN2() { return GPIO_NUM_NC; }

  // Momentary push-button that can be long-pressed at runtime to start the Wi-Fi AP. Usually the BOOT button on GPIO0.
  virtual gpio_num_t AP_BUTTON_PIN() { return GPIO_NUM_NC; }

  // Returns the available comm interfaces on this HW
  virtual std::vector<comm_interface> available_interfaces() = 0;

  virtual const char* name_for_comm_interface(comm_interface comm) {
    switch (comm) {
      case comm_interface::Modbus:
        return "Modbus";
      case comm_interface::RS485:
        return "RS485";
      case comm_interface::CanNative:
        return "CAN (Native)";
      case comm_interface::CanFdNative:
        return "";
      case comm_interface::CanAddonMcp2515:
        return "CAN (MCP2515 add-on)";
      case comm_interface::CanFdAddonMcp2518:
        return "CAN FD (MCP2518 add-on)";
      case comm_interface::CanFdAddonMcp2518_2:
        return "";
      default:
        return nullptr;
    }
  }

  String failed_allocator() { return allocator_name; }
  String conflicting_allocator() { return allocated_name; }

 protected:
  SPIClass _spi_hspi{HSPI};
#ifndef CONFIG_IDF_TARGET_ESP32S3
  SPIClass _spi_vspi{VSPI};
#else
  SPIClass _spi_fspi{FSPI};
#endif
  SPIClass _spi_none{0};  // sentinel — returned by boards that don't use a peripheral

 private:
  SPIClass& spi(uint8_t bus) {
#ifndef CONFIG_IDF_TARGET_ESP32S3
    return bus == HSPI ? _spi_hspi : _spi_vspi;
#else
    return bus == HSPI ? _spi_hspi : _spi_fspi;
#endif
  }

  std::unordered_map<gpio_num_t, std::string> allocated_pins;
  // For event logging, store the name of the allocator/allocated
  // for failed gpio allocations.
  String allocator_name;
  String allocated_name;
};

extern Esp32Hal* esp32hal;

// Needed for AsyncTCPSock library.
#define WIFI_CORE (esp32hal->WIFICORE())

void init_hal();

#endif
