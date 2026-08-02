#ifndef SDCARD_H
#define SDCARD_H

// SD support is gated at compile time. A board defines exactly one of
// SD_VIA_SPI / SD_VIA_SDIO if it has an SD slot; boards without a slot define
// neither, compile no SD code, and lib_ignore both SD libraries. All callers
// of the SD API must guard their calls with #ifdef SD_CARD_ENABLED so that a
// missed guard fails at compile time on slotless boards.
#if defined(SD_VIA_SPI) && defined(SD_VIA_SDIO)
#error "Define at most one of SD_VIA_SPI / SD_VIA_SDIO"
#endif

#if defined(SD_VIA_SPI) || defined(SD_VIA_SDIO)
#define SD_CARD_ENABLED
#endif

#include "../../communication/can/comm_can.h"
#include "../hal/hal.h"
#include "../utils/events.h"

#define CAN_LOG_FILE "/canlog.txt"
#define LOG_FILE "/log.txt"

#ifdef SD_CARD_ENABLED

#ifdef SD_VIA_SPI
#include <SD.h>
using SdCard = fs::SDFS;
#else  // SD_VIA_SDIO
#include <SD_MMC.h>
using SdCard = fs::SDMMCFS;
#endif

void init_logging_buffers();
void deinit_logging_buffers();

bool init_sdcard();
void log_sdcard_details(SdCard& card);

// Returns the concrete board's SD filesystem type
SdCard& sdcard_fs();

void add_can_frame_to_buffer(CAN_frame frame, frameDirection msgDir);
void write_can_frame_to_sdcard();

void pause_can_writing();
void resume_can_writing();
void delete_can_log();
void delete_log();
void resume_log_writing();
void pause_log_writing();

void add_log_to_buffer(const uint8_t* buffer, size_t size);
void write_log_to_sdcard();

#endif  // SD_CARD_ENABLED

#endif  // SDCARD_H
