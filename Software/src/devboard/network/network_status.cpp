#include "network_status.h"

#include <WiFi.h>

#ifdef HW_HAS_ETHERNET
#include "../ethernet/ethernet.h"
#endif

bool network_connected() {
#ifdef HW_HAS_ETHERNET
  if (ethernet_connected()) {
    return true;
  }
#endif
  return WiFi.status() == WL_CONNECTED;
}

IPAddress network_localIP() {
#ifdef HW_HAS_ETHERNET
  if (ethernet_connected()) {
    return ethernet_localIP();
  }
#endif
  return WiFi.localIP();
}

const char* network_hostname() {
#ifdef HW_HAS_ETHERNET
  if (ethernet_connected()) {
    return ETH.getHostname();
  }
#endif
  return WiFi.getHostname();
}

const char* network_active_ifname() {
#ifdef HW_HAS_ETHERNET
  if (ethernet_connected()) {
    return "Ethernet";
  }
#endif
  return "WiFi";
}
