#include "network_status.h"

// On non-Ethernet boards network_connected()/network_localIP() are inline in the
// header (they are exactly the WiFi calls); only the Ethernet-capable variant
// needs out-of-line definitions here.
#ifdef HW_HAS_ETHERNET

#include <WiFi.h>

#include "../ethernet/ethernet.h"

bool network_connected() {
  if (ethernet_connected()) {
    return true;
  }
  return WiFi.status() == WL_CONNECTED;
}

IPAddress network_localIP() {
  if (ethernet_connected()) {
    return ethernet_localIP();
  }
  return WiFi.localIP();
}

#endif  // HW_HAS_ETHERNET
