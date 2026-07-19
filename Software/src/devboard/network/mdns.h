#ifndef NETWORK_MDNS_H
#define NETWORK_MDNS_H

// Interface-agnostic mDNS responder. mDNS is advertised over whatever netif is
// up (WiFi STA or Ethernet), so it lives in the network module rather than in
// either interface's driver. init_mDNS() is called from whichever interface
// first acquires an IP; both interface modules depend on this, not on each other.

// If true, allow the battery emulator to be found by its .local address.
extern bool mdns_enabled;

// Start the mDNS responder (advertises http/tcp/80) bound to the active
// interface's hostname. Self-guarding: honours mdns_enabled and starts at most
// once. Called from whichever interface (WiFi STA or Ethernet) first gets an IP.
void init_mDNS();

#endif  // NETWORK_MDNS_H
