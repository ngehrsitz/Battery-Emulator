#ifndef NETWORK_STATUS_H
#define NETWORK_STATUS_H

#include <WiFi.h>  // IPAddress + WiFiClient are all reached via WiFi.h in this project

// Shared network-state helpers. Used wherever code needs "is any interface up?"
// or "what IP do we display?" — abstracts over WiFi STA + optional Ethernet
// so those semantics don't drift between call sites.

// True if the WiFi station is connected OR Ethernet has an IP.
bool network_connected();

// Prefer Ethernet's IP if it is up, else the WiFi STA IP. 0.0.0.0 if neither.
IPAddress network_localIP();

#endif  // NETWORK_STATUS_H
