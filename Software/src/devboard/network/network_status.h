#pragma once

#include <WiFi.h>

// Shared network-state helpers

bool network_connected();

IPAddress network_localIP();

// Bring up the interface-agnostic services that run once any interface acquires
// an IP: log the address, start syslog, and start the mDNS responder. Called
// from GOT_IP handlers
void network_bring_services_up(const IPAddress& ip);

#ifdef ETHERNET
// Set the default route and DNS source to the highest-priority connected
// interface (Ethernet > WiFi STA). Call from every GOT_IP and DISCONNECTED
// handler so the choice is always deterministic.
void network_update_default_interface();

// When true, network_update_default_interface() probes DNS reachability
// (getaddrinfo on a fixed host) before committing to the preferred interface,
// falling back to the other one if the probe fails. Loaded from NVM key
// "NETPROBEEN". Default: false.
extern bool net_probe_enabled;
#endif
