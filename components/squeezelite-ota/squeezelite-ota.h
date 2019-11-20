/*
 * squeezelite-ota.h
 *
 *  Created on: 25 sept. 2019
 *      Author: sle11
 */

#pragma once
#include "esp_attr.h"
#if RECOVERY_APPLICATION
#define CODE_RAM_LOCATION
#define RECOVERY_IRAM_FUNCTION IRAM_ATTR
#else
#define RECOVERY_IRAM_FUNCTION
#define CODE_RAM_LOCATION
#endif



// ERASE BLOCK needs to be a multiple of wear leveling's sector size
#define OTA_FLASH_ERASE_BLOCK (uint32_t)512000
#define OTA_STACK_SIZE 5120
#define OTA_TASK_PRIOTITY 6
esp_err_t start_ota(const char * bin_url);
const char * ota_get_status();
uint8_t ota_get_pct_complete();


