#pragma once

#include <Arduino.h>  // String
#include <string>

// Each network interface derives its hostname from its own MAC
// (ESP_MAC_WIFI_STA vs ESP_MAC_ETH, which differ by +3 in the last octet), so
// WiFi and Ethernet get distinct names. Routers such as OpenWRT/dnsmasq key
// their host table on the hostname string, not the MAC, so two interfaces
// sharing one name collapse to a single entry ("last lease wins"). Distinct
// names keep both visible.

// User-configured hostname. Loaded from NVM ("HOSTNAME"); empty when unset
extern std::string custom_hostname;

// Effective hostname for the WiFi STA interface: when a custom hostname is set
// it is used with the WiFi MAC suffix appended (so WiFi and Ethernet still
// differ); otherwise "battery-emulator-" + last two bytes of the WiFi STA MAC.
String wifi_hostname();

// Effective hostname for the Ethernet interface: same rule as wifi_hostname()
// but derived from the Ethernet MAC (ESP_MAC_ETH).
String eth_hostname();
