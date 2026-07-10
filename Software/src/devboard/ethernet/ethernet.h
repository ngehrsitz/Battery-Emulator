#ifndef ETHERNET_H
#define ETHERNET_H

#include <WiFi.h>  // IPAddress (the wrapper signatures below use no <ETH.h> types)

// On-board Ethernet PHY helpers. These wrappers deliberately expose only
// bool / IPAddress / const char* so callers never need <ETH.h> — that header
// (and its extern "C" ESP-IDF headers) is confined to ethernet.cpp. Whether a
// board actually has Ethernet is decided at runtime by esp32hal->HAS_ETH(), so
// these compile and are safe to call on every board.

// Bring up the on-board Ethernet PHY using pins provided by the HAL.
// A no-op on boards whose HAL reports HAS_ETH() == false. Safe to call once at boot.
void init_Ethernet();

// True after the PHY reports link-up and DHCP (or static-config) has yielded an IP.
// Always false on boards without Ethernet (the PHY is never brought up).
bool ethernet_connected();

// Current Ethernet IP address, or 0.0.0.0 if the interface has none.
IPAddress ethernet_localIP();

// Ethernet interface hostname (wraps ETH.getHostname()). Keeps the raw ETH
// object out of every caller.
const char* ethernet_hostname();

#endif  // ETHERNET_H
