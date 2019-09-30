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
#else
#define CODE_RAM_LOCATION
#endif

esp_err_t CODE_RAM_LOCATION start_ota(const char * bin_url, bool bFromAppMain);
const char * CODE_RAM_LOCATION ota_get_status();
uint8_t CODE_RAM_LOCATION ota_get_pct_complete();


