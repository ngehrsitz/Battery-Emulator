#ifndef UDAN_BATTERY_H
#define UDAN_BATTERY_H

#include "PYLON-BATTERY.h"

// UdanBattery — Udan / Youdan (优旦) HV BMS, rebadged as Asgoft "ASHHESS".
//
// The pack speaks the full Pylontech protocol on the wire (which the inverter
// side of Battery-Emulator relies on), so this driver INHERITS PylonBattery
// unchanged for all normal telemetry / inverter support. On top of that it adds
// a strictly READ-ONLY probe of the manufacturer diagnostic protocol, whose
// goal is to obtain per-individual-cell voltages that the Pylontech plane does
// not expose.
//
// SAFETY (read-only by construction): the Udan diagnostic plane also carries
// contactor / MOS force-control commands. This driver deliberately defines and
// builds ONLY the read request. It contains NONE of the control-command
// constants (MosForceControl, WorkModeControl, SystemReset, RestoreFactory,
// PreChg, firmware UpdateRun, ...). A control frame is therefore not
// constructible here. Do not add them.
//
// PHASE 1 (this file): the only question is whether the pack responds to a
// diagnostic read AT ALL. The captured CAN logs show the pack broadcasts
// nothing on the diagnostic plane, so a request must be sent. We send one
// minimal read request and log every candidate response frame verbatim, then
// back off / self-disable so a deployed build is safe to leave running on the
// remote pack indefinitely. Transport reassembly + protobuf decode come in
// Phase 2, driven by the raw responses this phase captures.

class UdanBattery : public PylonBattery {
 public:
  UdanBattery() : PylonBattery() {}

  // Second-battery constructor, mirrors PylonBattery's for completeness.
  UdanBattery(DATALAYER_BATTERY_TYPE* datalayer_ptr, bool* contactor_closing_allowed_ptr, CAN_Interface targetCan)
      : PylonBattery(datalayer_ptr, contactor_closing_allowed_ptr, targetCan) {}

  void setup(void) override;
  void transmit_can(unsigned long currentMillis) override;
  void handle_incoming_can_frame(CAN_frame rx_frame) override;

  static constexpr const char* Name = "Udan /Asgoft (Pylon + cell diagnostics)";

 private:
  // ---- Diagnostic read-probe (Phase 1) ----------------------------------
  // Candidate request/response 11-bit ID pairs to try, from the reverse-
  // engineered protocol (Udan BMS/analysis/PROTOCOL.md §3.1). Primary first.
  static const uint32_t UDAN_DIAG_TX_PRIMARY = 0x740;
  static const uint32_t UDAN_DIAG_RX_PRIMARY = 0x748;
  static const uint32_t UDAN_DIAG_TX_ALT = 0x36E;
  static const uint32_t UDAN_DIAG_RX_ALT = 0x37E;

  // After this many polls with zero diagnostic-plane response, stop probing so
  // the build degrades to a harmless plain-Pylon driver on the remote pack.
  static const uint8_t UDAN_MAX_UNANSWERED = 12;

  // Returns true if the given RX id is one we consider a diagnostic response.
  bool is_diag_response_id(uint32_t id) const;

  // Send one read request for BatteryCellMessage on the currently-selected TX id.
  void send_cell_read_request();

  unsigned long previousMillisUdan = 0;  // last time a diagnostic poll was sent
  uint8_t unanswered_polls = 0;          // consecutive polls with no diag response
  bool diag_probe_active = true;         // cleared once we give up (or get a response)
  bool diag_response_seen = false;       // set true on first response on a diag RX id
  bool use_alt_id_pair = false;          // false = primary (0x740/0x748), true = alt (0x36E/0x37E)
};

#endif
