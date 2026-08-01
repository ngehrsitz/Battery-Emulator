#ifndef SDCARD_H
#define SDCARD_H

#ifdef SD_OVER_SPI
#include <SD.h>
#else
#include <SD_MMC.h>
#endif
#include "../../communication/can/comm_can.h"
#include "../hal/hal.h"
#include "../utils/events.h"

#define CAN_LOG_FILE "/canlog.txt"
#define LOG_FILE "/log.txt"

#ifdef SD_OVER_SPI
using SdCard = fs::SDFS;
#else
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

#endif  // SDCARD_H
