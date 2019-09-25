/*
 * squeezelite-ota.h
 *
 *  Created on: 25 sept. 2019
 *      Author: sle11
 */

#ifndef COMPONENTS_SQUEEZELITE_OTA_SQUEEZELITE_OTA_H_
#define COMPONENTS_SQUEEZELITE_OTA_SQUEEZELITE_OTA_H_

void start_ota(const char * bin_url);
const char * ota_get_status();
uint8_t ota_get_pct_complete();



#endif /* COMPONENTS_SQUEEZELITE_OTA_SQUEEZELITE_OTA_H_ */
