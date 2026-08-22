#include "UDAN-BATTERY.h"
#include "../datalayer/datalayer.h"
#include "../devboard/utils/events.h"
#include "../devboard/utils/logging.h"

// See UDAN-BATTERY.h for the design and the read-only-by-construction safety
// note. This file is Phase 1: probe whether the pack answers a diagnostic read
// at all, and log any candidate response verbatim.

// -------------------------------------------------------------------------
// Diagnostic read request payload (application layer).
//
// Reverse-engineered protobuf (Udan BMS/analysis/P700_protobuf_pb.proto):
//
//   RemoteOperationMessage {
//     RequestId       = 2 (uint32)          -> we send 1
//     RemoteReadMessage = 3 (nested msg) {
//       RemoteReadType = 1 (RemoteReadEnum) -> BatteryCellMessageRead = 3
//     }
//   }
//
// proto3 wire encoding:
//   inner RemoteReadMessage: field 1, varint, value 3  -> 08 03
//   outer RequestId:         field 2, varint, value 1  -> 10 01
//   outer RemoteReadMessage: field 3, len-delimited,
//                            len 2, payload {08 03}     -> 1A 02 08 03
//   => RemoteOperationMessage payload = 10 01 1A 02 08 03   (6 bytes)
//
// This application payload is well-recovered and low-risk. What is NOT
// documented is the "UART-over-CAN" transport that wraps it (header byte,
// sequence counter, single-vs-multi-frame rule). For Phase 1 we send the
// most likely single-frame guess: an ISO-TP-style Single Frame where the
// low nibble of byte 0 is the payload length, followed by the payload. If the
// pack does not answer, the raw response bytes (or silence) captured here tell
// us how to correct the framing in Phase 2. This is a GUESS, intentionally
// isolated to one place.
static const uint8_t UDAN_READ_CELL_PB[] = {0x10, 0x01, 0x1A, 0x02, 0x08, 0x03};

bool UdanBattery::is_diag_response_id(uint32_t id) const {
  return id == UDAN_DIAG_RX_PRIMARY || id == UDAN_DIAG_RX_ALT;
}

void UdanBattery::send_cell_read_request() {
  CAN_frame req;
  req.FD = false;
  req.ext_ID = false;  // diagnostic plane uses 11-bit standard IDs (PROTOCOL.md §3.1)
  req.ID = use_alt_id_pair ? UDAN_DIAG_TX_ALT : UDAN_DIAG_TX_PRIMARY;
  req.DLC = 8;
  for (uint8_t i = 0; i < 8; i++) {
    req.data.u8[i] = 0x00;
  }

  // ISO-TP-style Single Frame guess: byte0 = length, then the protobuf payload.
  const uint8_t pb_len = sizeof(UDAN_READ_CELL_PB);
  req.data.u8[0] = pb_len;  // SF PCI (low nibble is length for classic ISO-TP)
  for (uint8_t i = 0; i < pb_len && (i + 1) < 8; i++) {
    req.data.u8[i + 1] = UDAN_READ_CELL_PB[i];
  }

  transmit_can_frame(&req);

  DEBUG_PRINTF("UDAN: sent diag cell-read probe on 0x%03X (attempt %u/%u, pair=%s)\n", req.ID,
               (unsigned)(unanswered_polls + 1), (unsigned)UDAN_MAX_UNANSWERED, use_alt_id_pair ? "alt" : "primary");
}

void UdanBattery::setup(void) {
  // Inherit all Pylontech setup (protocol string, cell count, design voltages,
  // etc.) so the inverter side keeps working exactly as with the Pylon driver.
  PylonBattery::setup();

  // Advertise this variant in the UI without disturbing Pylon telemetry.
  strncpy(datalayer.system.info.battery_protocol, "Udan /Asgoft (Pylon + cell diagnostics)", 63);
  datalayer.system.info.battery_protocol[63] = '\0';
}

void UdanBattery::transmit_can(unsigned long currentMillis) {
  // Always run the full Pylontech transmit first (heartbeat, requests, etc.).
  PylonBattery::transmit_can(currentMillis);

  if (!diag_probe_active) {
    return;  // gave up (or already got a response) - behave as plain Pylon.
  }

  // Slow, low-load diagnostic poll (5 s cadence, matching Pylon's cell poll).
  if (currentMillis - previousMillisUdan >= INTERVAL_5_S) {
    previousMillisUdan = currentMillis;

    send_cell_read_request();

    // Count this as unanswered until/unless a response arrives. If we exhaust
    // the primary ID pair, switch to the alternate pair once before giving up.
    unanswered_polls++;
    if (unanswered_polls == (UDAN_MAX_UNANSWERED / 2) && !use_alt_id_pair) {
      use_alt_id_pair = true;
      DEBUG_PRINTF("UDAN: no response on primary pair, switching to alt pair 0x%03X/0x%03X\n", UDAN_DIAG_TX_ALT,
                   UDAN_DIAG_RX_ALT);
    }
    if (unanswered_polls >= UDAN_MAX_UNANSWERED) {
      diag_probe_active = false;
      DEBUG_PRINTF("UDAN: diagnostic probe got no response after %u attempts - disabling probe, "
                   "continuing as plain Pylon.\n",
                   (unsigned)UDAN_MAX_UNANSWERED);
    }
  }
}

void UdanBattery::handle_incoming_can_frame(CAN_frame rx_frame) {
  // Let the Pylontech decoder handle all its own frames first.
  PylonBattery::handle_incoming_can_frame(rx_frame);

  // Phase 1: capture any diagnostic-plane response verbatim. (The frame is also
  // recorded by the built-in CAN logger; this adds a decoded/annotated line.)
  if (is_diag_response_id(rx_frame.ID)) {
    if (!diag_response_seen) {
      diag_response_seen = true;
      diag_probe_active = false;  // we have what Phase 1 needed - stop poking the pack.
    }
    unanswered_polls = 0;
    DEBUG_PRINTF("UDAN: *** DIAG RESPONSE on 0x%03X *** DLC=%u data=%02X %02X %02X %02X %02X %02X %02X %02X\n",
                 rx_frame.ID, rx_frame.DLC, rx_frame.data.u8[0], rx_frame.data.u8[1], rx_frame.data.u8[2],
                 rx_frame.data.u8[3], rx_frame.data.u8[4], rx_frame.data.u8[5], rx_frame.data.u8[6],
                 rx_frame.data.u8[7]);
  }
}
