#pragma once

#ifdef ETHERNET

#include <ETH.h>
#include <WiFi.h>  // WiFiEvent_t / WiFiEventInfo_t (arduino-esp32 dispatches ETH events through this too)

// Bring up the on-board Ethernet PHY using pins provided by the HAL
void init_Ethernet();

// True after the PHY reports link-up and DHCP
bool ethernet_connected();

// Current Ethernet IP address, or 0.0.0.0 if the interface has none.
IPAddress ethernet_localIP();

#endif  // ETHERNET
