#ifndef NETWORK_STATUS_H
#define NETWORK_STATUS_H

#include <WiFi.h>  // IPAddress + WiFiClient are all reached via WiFi.h in this project

// Shared network-state helpers. Used wherever code needs "is any interface up?",
// "what IP do we display?", or "what hostname?" — abstracts over WiFi STA +
// optional Ethernet so those semantics don't drift between call sites. Which
// interface is active is decided at runtime (Ethernet wins if it has an IP), so
// these need no per-board compile-time gating.

// True if the WiFi station is connected OR Ethernet has an IP.
bool network_connected();

// Prefer Ethernet's IP if it is up, else the WiFi STA IP. 0.0.0.0 if neither.
IPAddress network_localIP();

// Hostname of the active interface (Ethernet if up, else WiFi).
const char* network_hostname();

// Human-readable name of the active interface: "Ethernet" if Ethernet is up,
// else "WiFi".
const char* network_interface_name();

#endif  // NETWORK_STATUS_H
