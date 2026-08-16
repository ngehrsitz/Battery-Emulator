#include "hostname.h"

#include <esp_mac.h>  // esp_read_mac(), ESP_MAC_WIFI_STA, ESP_MAC_ETH

std::string custom_hostname;  // If not set, a MAC-derived default is used

// Lowercase two-hex-digit suffix from the last two bytes of the given MAC type.
// Reads eFuse directly, so it is valid even before the WiFi or Ethernet
// interface has started.
static String mac_suffix(esp_mac_type_t mac_type) {
  uint8_t mac_bytes[6];
  esp_read_mac(mac_bytes, mac_type);
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%02x%02x", mac_bytes[4], mac_bytes[5]);
  return String(suffix);
}

// Effective hostname for one interface: "<custom>-<suffix>" when a custom name
// is set (kept distinct per interface), else "battery-emulator-<suffix>".
// Cached: the MACs never change at runtime.
static String hostname_for(esp_mac_type_t mac_type, String& cache) {
  if (cache.isEmpty()) {
    String suffix = mac_suffix(mac_type);
    cache = custom_hostname.empty() ? "battery-emulator-" + suffix : String(custom_hostname.c_str()) + "-" + suffix;
  }
  return cache;
}

String wifi_hostname() {
  static String cached;
  return hostname_for(ESP_MAC_WIFI_STA, cached);
}

String eth_hostname() {
  static String cached;
  return hostname_for(ESP_MAC_ETH, cached);
}
