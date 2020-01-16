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

static const char *TAG = "display";

static bool (*slimp_handler_chain)(u8_t *data, int len);
static struct display_handle_s *handle;
static void (*chained_notify)(in_addr_t ip, u16_t hport, u16_t cport);

static void server_attach(in_addr_t ip, u16_t hport, u16_t cport);
static bool display_handler(u8_t *data, int len);

/* scrolling undocumented information
	grfs	
		B: screen number
		B:1 = left, 2 = right,
		Q: scroll pause once done (ms)
		Q: scroll speed (ms)
		W: # of pixels to scroll each time
		W: 0 = continue scrolling after pause, 1 = scroll to scrollend and then stop, 2 = scroll to scrollend and then end animation (causing new update)
		W: width of total scroll area in pixels
			
	grfd
		W: screen number
		W: width of scrollable area	in pixels
		
ANIC flags
ANIM_TRANSITION   0x01 # A transition animation has finished
ANIM_SCROLL_ONCE  0x02 # A scrollonce has finished
ANIM_SCREEN_1     0x04 # For scrollonce only, screen 1 was scrolling
ANIM_SCREEN_2     0x08 # For scrollonce only, screen 2 was scrolling
*/

/****************************************************************************************
 * 
 */
void display_init(char *welcome) {
	char *item = config_alloc_get(NVS_TYPE_STR, "display_config");

	if (item && *item) {
		char * drivername=strstr(item,"driver");
		if (!drivername  || (drivername && strcasestr(drivername,"SSD1306"))) {
			handle = &SSD1306_handle;
			if (handle->init(item, welcome)) {
				slimp_handler_chain = slimp_handler;
				slimp_handler = display_handler;
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
	
	chained_notify = server_notify;
	server_notify = server_attach;

	if (item) free(item);
}

/****************************************************************************************
 * 
 */
static void server_attach(in_addr_t ip, u16_t hport, u16_t cport) {
	char msg[32];
	sprintf(msg, "%s:%hu", inet_ntoa(ip), hport);
	handle->print_message(msg);
	if (chained_notify) (*chained_notify)(ip, hport, cport);
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

