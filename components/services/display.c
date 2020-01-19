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
#include "display.h"

// here we should include all possible drivers
extern struct display_s SSD1306_display;

struct display_s *display;

static const char *TAG = "display";

/****************************************************************************************
 * 
 */
void display_init(char *welcome) {
	char *item = config_alloc_get(NVS_TYPE_STR, "display_config");

	if (item && *item) {
		char * drivername=strstr(item,"driver");
		if (!drivername  || (drivername && strcasestr(drivername,"SSD1306"))) {
			display = &SSD1306_display;
			if (display->init(item, welcome)) {
				ESP_LOGI(TAG, "Display initialization successful");
			} else {
				ESP_LOGE(TAG, "Display initialization failed");
			}
		} else {
			ESP_LOGE(TAG,"Unknown display driver name in display config: %s",item);
		}
	} else {
		ESP_LOGW(TAG, "no display");
	}
	
	if (item) free(item);
}
