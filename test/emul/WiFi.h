#ifndef WIFI_H
#define WIFI_H

#include <stdint.h>
#include <cstdio>

// Minimal IPAddress emulation for native unit tests. Backed by a single uint32_t
// so the value round-trips faithfully and comparisons/fromString behave like the
// real Arduino IPAddress at the string<->address boundary used by the WiFi settings.
class IPAddress {
 public:
  IPAddress() : addr(0) {}
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
      : addr(((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | d) {}

  bool operator==(const IPAddress& o) const { return addr == o.addr; }
  bool operator!=(const IPAddress& o) const { return addr != o.addr; }

  // Parse a dotted-quad string. On success stores the address and returns true;
  // on an empty or malformed string leaves the object unchanged and returns false
  // (matching Arduino IPAddress::fromString semantics).
  bool fromString(const char* s) {
    if (s == nullptr) {
      return false;
    }
    unsigned int a, b, c, d;
    char extra;
    if (sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4) {
      return false;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255) {
      return false;
    }
    addr = (a << 24) | (b << 16) | (c << 8) | d;
    return true;
  }

 private:
  uint32_t addr;
};

#endif
