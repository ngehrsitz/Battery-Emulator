#pragma once

#include <WiFi.h>

#include "hostname.h"  // wifi_hostname(), eth_hostname()

// Shared network-state helpers

bool network_connected();

IPAddress network_localIP();

// Returns the hostname of the currently active interface: the Ethernet name
// when Ethernet has an IP, otherwise the WiFi name. Mirrors network_localIP()'s
// active-interface selection.
String network_active_hostname();

// Bring up the interface-agnostic services that run once any interface acquires
// an IP: log the address, start syslog, and start the mDNS responder under the
// given hostname (the name of the interface that came up). Called from GOT_IP
// handlers.
void network_bring_services_up(const String& hostname, const IPAddress& ip);
