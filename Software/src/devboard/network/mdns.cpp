#include "mdns.h"

#include "network_status.h"  // network_hostname()

#ifndef SMALL_FLASH_DEVICE
#include <ESPmDNS.h>
#endif

#include "../utils/logging.h"

bool mdns_enabled = true;

// Safe to call from either the WiFi STA GOT_IP or the Ethernet GOT_IP handler:
// the mdns_enabled check and the one-shot guard live here, so the responder
// begins exactly once on whichever interface acquires an IP first. Both call
// sites run on the single arduino-esp32 arduino_events task, so the guard needs
// no lock.
void init_mDNS() {
#ifndef SMALL_FLASH_DEVICE
  static bool mdns_started = false;
  if (!mdns_enabled || mdns_started) {
    return;
  }

  // Reuse the active interface's hostname (Ethernet if it is up, else the WiFi
  // custom/"battery-emulator-<mac>" default set in init_WiFi()). Consistent with AP too.
  String mdnsHost = String(network_hostname());

  // Initialize mDNS .local resolution
  if (!MDNS.begin(mdnsHost)) {
    logging.println("Error setting up mDNS responder!");
  } else {
    // Advertise via bonjour the web inteface so we can auto discover these battery emulators on the local network.
    MDNS.addService("http", "tcp", 80);
    logging.println("mDNS responder started.");
  }
  mdns_started = true;  // set even on begin() failure — preserves prior one-shot semantics
#endif
}
