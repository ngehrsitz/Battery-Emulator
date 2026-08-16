#pragma once

#include <Arduino.h>  // String
#include <string>

// Network interface a hostname is derived for. Each interface has its own MAC
// (ESP_MAC_WIFI_STA vs ESP_MAC_ETH, which differ by +3 in the last octet), so
// deriving the name per-interface gives each a distinct hostname. Routers such
// as OpenWRT/dnsmasq key their host table on the hostname string, not the MAC,
// so two interfaces sharing one name collapse to a single entry ("last lease
// wins"). Distinct names keep both visible.
enum class NetIface { Wifi, Eth };

// User-configured hostname. Loaded from NVM ("HOSTNAME"); empty when unset
extern std::string custom_hostname;

// Returns the default hostname for the given interface:
// "battery-emulator-" + last two bytes of THAT interface's MAC (lowercase).
String default_hostname(NetIface iface);

// Returns the effective hostname for the given interface: when a custom
// hostname is set it is used with the interface's MAC suffix appended (so the
// two interfaces still differ); otherwise default_hostname(iface).
String active_hostname(NetIface iface);
