/* 
 *  (c) 2004,2006 Richard Titmuss for SlimProtoLib 
 *  (c) Philippe G. 2019, philippe_44@outlook.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "esp_log.h"
#include "config.h"
#include "embedded.h"
#include "display.h"

#define TAG 		"display"

static bool (*slimp_handler_chain)(u8_t *data, int len);
static struct display_handle_s *handle;

static bool display_handler(u8_t *data, int len);

/****************************************************************************************
 * 
 */
void display_init(void) {
	char *item = config_alloc_get(NVS_TYPE_STR, "display_config");

	if (item && *item) {
		char * drivername=strstr(item,"driver");
		if( !drivername  || (drivername && (strstr(drivername,"SSD1306") || strstr(drivername,"ssd1306")))){
			handle = &SSD1306_handle;
			if (handle->init(item)) {
				slimp_handler_chain = slimp_handler;
				slimp_handler = display_handler;
				ESP_LOGI(TAG, "Display initialization successful");
			} else {
				ESP_LOGE(TAG, "Display initialization failed");
			}
		}else {
			ESP_LOGE(TAG,"Unknown display driver name in display config: %s",item);

		}
	} else {
		ESP_LOGW(TAG, "no display");
	}

	if (item) free(item);
}

/****************************************************************************************
 * Process graphic display data
 */
static bool display_handler(u8_t *data, int len){
	bool res = true;

	if (!strncmp((char*) data, "vfdc", 4)) {
		handle->vfdc_handler(data, len);
	} else if (!strncmp((char*) data, "grfe", 4)) {
		handle->grfe_handler(data, len);
	} else {
		res = false;
	}
	
	// chain protocol handlers (bitwise or is fine)
	if (*slimp_handler_chain) res |= (*slimp_handler_chain)(data, len);
	return res;
}

