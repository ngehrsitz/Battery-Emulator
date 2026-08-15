#include "ethernet.h"

#ifdef ETHERNET

#include "../hal/hal.h"
#include "../network/hostname.h"        // active_hostname() (shared across interfaces)
#include "../network/network_status.h"  // network_bring_services_up()
#include "../utils/events.h"
#include "../utils/logging.h"

static bool eth_has_ip = false;

// Map project-local ETH_PHY_KIND_* tags (from hal.h) to arduino-esp32's
// eth_phy_type_t. Board HAL headers return the kind tag so they don't have to
// include <ETH.h> — see hw_dfrobot_edge101.h for why that matters.
static eth_phy_type_t phy_type_from_kind(int kind) {
  switch (kind) {
    case ETH_PHY_KIND_IP101:
      return ETH_PHY_IP101;
    default:
      return ETH_PHY_MAX;  // sentinel — ETH.begin() will report failure
  }
}

static eth_clock_mode_t clk_mode_from_kind(int kind) {
  switch (kind) {
    case ETH_CLK_KIND_GPIO0_IN:
      return ETH_CLOCK_GPIO0_IN;
    default:
      return ETH_CLOCK_GPIO0_IN;  // safe default; unreachable for real boards (they all override ETH_CLK_MODE_ID)
  }
}

// Single multiplexed handler for the ETH_* subset of arduino-esp32's WiFi event
// dispatcher. Emits our EVENT_ETHERNET_* pair and tracks whether we have an IP.
static void onEthEvent(WiFiEvent_t event, WiFiEventInfo_t /*info*/) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      DEBUG_PRINTF("Ethernet started\n");
      // Hostname must be re-applied after ETH_START; setting it before begin()
      // isn't sufficient because the netif is created here.
      ETH.setHostname(active_hostname().c_str());
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      DEBUG_PRINTF("Ethernet link up: %u Mbps, %s duplex\n", ETH.linkSpeed(), ETH.fullDuplex() ? "full" : "half");
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      eth_has_ip = true;
      clear_event(EVENT_ETHERNET_DISCONNECT);
      set_event(EVENT_ETHERNET_CONNECT, 0);
      clear_event(EVENT_ETHERNET_CONNECT);
      // eth_has_ip is set above, so network_localIP() inside the shared helper
      // resolves to the Ethernet interface
      network_bring_services_up(ETH.localIP());  // log IP + syslog_start() + init_mDNS()
      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
    case ARDUINO_EVENT_ETH_STOP:
      if (eth_has_ip) {
        set_event(EVENT_ETHERNET_DISCONNECT, 0);
      }
      eth_has_ip = false;
      logging.println("Ethernet disconnected.");
      break;

    default:
      break;
  }
}

void init_Ethernet() {
  DEBUG_PRINTF("init_Ethernet: MDC=%d MDIO=%d POWER=%d phy_type=%d phy_addr=%d clk_mode=%d\n",
               (int)esp32hal->ETH_PHY_MDC_PIN(), (int)esp32hal->ETH_PHY_MDIO_PIN(), (int)esp32hal->ETH_PHY_POWER_PIN(),
               esp32hal->ETH_PHY_TYPE_ID(), esp32hal->ETH_PHY_ADDR_NUM(), esp32hal->ETH_CLK_MODE_ID());

  // Track pins that go through the HAL allocator. The RMII data/clock pins
  // (GPIO 0/21/22/25/26/27 on classic ESP32) are fixed by the EMAC peripheral
  // and can't be reassigned, so they don't need allocator tracking. MDC/MDIO
  // and the PHY power/reset pin are configurable and can conflict with SD/CAN.
  esp32hal->alloc_pins_ignore_unused("Ethernet", esp32hal->ETH_PHY_MDC_PIN(), esp32hal->ETH_PHY_MDIO_PIN(),
                                     esp32hal->ETH_PHY_POWER_PIN());

  // Register handler BEFORE ETH.begin(); events fire as soon as begin() runs.
  WiFi.onEvent(onEthEvent, ARDUINO_EVENT_ETH_START);
  WiFi.onEvent(onEthEvent, ARDUINO_EVENT_ETH_CONNECTED);
  WiFi.onEvent(onEthEvent, ARDUINO_EVENT_ETH_GOT_IP);
  WiFi.onEvent(onEthEvent, ARDUINO_EVENT_ETH_DISCONNECTED);
  WiFi.onEvent(onEthEvent, ARDUINO_EVENT_ETH_STOP);

  const bool ok = ETH.begin(phy_type_from_kind(esp32hal->ETH_PHY_TYPE_ID()), esp32hal->ETH_PHY_ADDR_NUM(),
                            esp32hal->ETH_PHY_MDC_PIN(), esp32hal->ETH_PHY_MDIO_PIN(), esp32hal->ETH_PHY_POWER_PIN(),
                            clk_mode_from_kind(esp32hal->ETH_CLK_MODE_ID()));
  if (!ok) {
    logging.println("Ethernet init failed (ETH.begin returned false).");
    set_event(EVENT_ETHERNET_DISCONNECT, 0);
  }
}

bool ethernet_connected() {
  return eth_has_ip;
}

IPAddress ethernet_localIP() {
  return ETH.localIP();
}

#endif  // ETHERNET
