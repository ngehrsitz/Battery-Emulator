#include "mdns.h"

#ifndef SMALL_FLASH_DEVICE

#include <ESPmDNS.h>
#include "../utils/logging.h"

// Safe to call from both interfaces. Both call sites run on the single
// arduino-esp32 arduino_events task, so the guard needs no lock.
void init_mDNS(const String& hostname) {
  static bool mdns_started = false;
  if (mdns_started) {
    return;
  }

  // Initialize mDNS .local resolution under the hostname of the interface that
  // brought the network up.
  if (!MDNS.begin(hostname)) {
    logging.println("Error setting up mDNS responder!");
  } else {
    // Advertise the web interface via bonjour
    MDNS.addService("http", "tcp", 80);
    logging.println("mDNS responder started.");
  }
  mdns_started = true;  // set even on begin() failure — preserves prior one-shot semantics
}

#endif  // SMALL_FLASH_DEVICE
