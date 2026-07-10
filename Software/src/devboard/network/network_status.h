#ifndef NETWORK_STATUS_H
#define NETWORK_STATUS_H

#include <WiFi.h>  // IPAddress + WiFiClient are all reached via WiFi.h in this project

// Shared network-state helpers. Used wherever code needs "is any interface up?"
// or "what IP do we display?" — abstracts over WiFi STA + optional Ethernet
// so those semantics don't drift between call sites.

#ifdef HW_HAS_ETHERNET

// Ethernet-capable boards need the runtime check across both interfaces, so the
// implementations live out-of-line in network_status.cpp.

// True if the WiFi station is connected OR Ethernet has an IP.
bool network_connected();

// Prefer Ethernet's IP if it is up, else the WiFi STA IP. 0.0.0.0 if neither.
IPAddress network_localIP();

#else

// No Ethernet: these are exactly the WiFi calls. Inline so the compiler folds
// them into the (already WiFi-linked) call sites — no standalone functions or
// extra translation unit on small-flash boards.
inline bool network_connected() {
  return WiFi.status() == WL_CONNECTED;
}

inline IPAddress network_localIP() {
  return WiFi.localIP();
}

#endif  // HW_HAS_ETHERNET

#endif  // NETWORK_STATUS_H
