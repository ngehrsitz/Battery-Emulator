#include "network_status.h"

#include <WiFi.h>

#include "../ethernet/ethernet.h"  // ethernet_connected/localIP/hostname (no-ops on non-ETH boards)

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

const char* network_hostname() {
  if (ethernet_connected()) {
    return ethernet_hostname();
  }
  return WiFi.getHostname();
}

const char* network_interface_name() {
  return ethernet_connected() ? "Ethernet" : "WiFi";
}
