#pragma once

#include <Arduino.h>  // String

// mDNS responder, registered under the hostname of the interface that came up

#ifndef SMALL_FLASH_DEVICE
// Start the mDNS responder (advertises http/tcp/80) bound to the given
// hostname. Self-guarding: starts at most once, so whichever interface
// acquires an IP first owns the single .local name.
void init_mDNS(const String& hostname);
#endif
