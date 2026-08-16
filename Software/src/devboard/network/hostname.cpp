#include "hostname.h"

#include <esp_mac.h>  // esp_read_mac(), ESP_MAC_WIFI_STA, ESP_MAC_ETH

std::string custom_hostname;  // If not set, defaults to default_hostname()

// Lowercase two-hex-digit suffix from the last two bytes of the given
// interface's MAC. Reads eFuse directly, so it is valid even before the WiFi
// or Ethernet interface has started.
static String mac_suffix(NetIface iface) {
  esp_mac_type_t mac_type = (iface == NetIface::Eth) ? ESP_MAC_ETH : ESP_MAC_WIFI_STA;
  uint8_t mac_bytes[6];
  esp_read_mac(mac_bytes, mac_type);
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%02x%02x", mac_bytes[4], mac_bytes[5]);
  return String(suffix);
}

String default_hostname(NetIface iface) {
  // Cache per-interface: the MACs never change at runtime.
  static String cached[2];
  const int idx = (iface == NetIface::Eth) ? 1 : 0;
  if (cached[idx].isEmpty()) {
    cached[idx] = "battery-emulator-" + mac_suffix(iface);
  }
  return cached[idx];
}

String active_hostname(NetIface iface) {
  if (!custom_hostname.empty()) {
    // Append the interface MAC suffix so WiFi and Ethernet still present
    // distinct names to the router even with a user-configured base name.
    return String(custom_hostname.c_str()) + "-" + mac_suffix(iface);
  }
  return default_hostname(iface);
}
