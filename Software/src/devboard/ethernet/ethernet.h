#ifndef ETHERNET_H
#define ETHERNET_H

#ifdef HW_HAS_ETHERNET

#include <ETH.h>
#include <WiFi.h>  // WiFiEvent_t / WiFiEventInfo_t (arduino-esp32 dispatches ETH events through this too)

// Bring up the on-board Ethernet PHY using pins provided by the HAL.
// Safe to call once at boot; if the board has no PHY it is a no-op (this
// whole module compiles to nothing when HW_HAS_ETHERNET isn't defined).
void init_Ethernet();

// True after the PHY reports link-up and DHCP (or static-config) has yielded an IP.
bool ethernet_connected();

// Current Ethernet IP address, or 0.0.0.0 if the interface has none.
IPAddress ethernet_localIP();

#endif  // HW_HAS_ETHERNET
#endif  // ETHERNET_H
