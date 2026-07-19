#ifndef NETWORK_HOSTNAME_H
#define NETWORK_HOSTNAME_H

#include <Arduino.h>  // String
#include <string>

// Device hostname — a network-identity concept shared by every interface (WiFi
// STA/AP, Ethernet) and by the syslog/mDNS responders. Lives in the network
// module so no single interface driver owns it.

// User-configured hostname. Loaded from NVM ("HOSTNAME"); empty when unset, in
// which case default_hostname() is used.
extern std::string custom_hostname;

// Returns the default hostname ("battery-emulator-" + last two bytes of the MAC,
// lowercase) used when no custom hostname is configured. Safe to call at any time
// (reads eFuse directly, valid even before WiFi/Ethernet start).
String default_hostname();

#endif  // NETWORK_HOSTNAME_H
