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
#include "tools.h"
#include "display.h"

// here we should include all possible drivers
extern struct display_s SSD1306_display;

struct display_s *display;

static const char *TAG = "display";

#define min(a,b) (((a) < (b)) ? (a) : (b))

#define DISPLAYER_STACK_SIZE 	2048
#define SCROLLABLE_SIZE			384
#define HEADER_SIZE				64
#define	DEFAULT_SLEEP			3600


static EXT_RAM_ATTR struct {
	TaskHandle_t task;
	SemaphoreHandle_t mutex;
	int pause, speed, by;
	enum { DISPLAYER_DISABLED, DISPLAYER_AUTO_DISABLE, DISPLAYER_ACTIVE } state;
	char header[HEADER_SIZE + 1];
	char string[SCROLLABLE_SIZE + 1];
	int offset, boundary;
	char *metadata_config;
	bool timer;
	uint32_t elapsed, duration;
	TickType_t tick;
} displayer;

static void displayer_task(void *args);

/****************************************************************************************
 * 
 */
void display_init(char *welcome) {
	bool init = false;
	char *item = config_alloc_get(NVS_TYPE_STR, "display_config");

	if (item && *item) {
		char * drivername=strstr(item,"driver");
		if (!drivername  || (drivername && strcasestr(drivername,"SSD1306"))) {
			display = &SSD1306_display;
			if (display->init(item, welcome)) {
				init = true;
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
	
	if (init) {
		static DRAM_ATTR StaticTask_t xTaskBuffer __attribute__ ((aligned (4)));
		static EXT_RAM_ATTR StackType_t xStack[DISPLAYER_STACK_SIZE] __attribute__ ((aligned (4)));

		// start the task that will handle scrolling & counting
		displayer.mutex = xSemaphoreCreateMutex();
		displayer.by = 2;
		displayer.pause = 3600;
		displayer.speed = 33;
		displayer.task = xTaskCreateStatic( (TaskFunction_t) displayer_task, "displayer_thread", DISPLAYER_STACK_SIZE, NULL, ESP_TASK_PRIO_MIN + 1, xStack, &xTaskBuffer);
		
		// set lines for "fixed" text mode
		display->set_font(1, DISPLAY_FONT_LINE_1, -3);
		display->set_font(2, DISPLAY_FONT_LINE_2, -3);
		
		displayer.metadata_config = config_alloc_get(NVS_TYPE_STR, "metadata_config");
	}
	
	if (item) free(item);
}

/****************************************************************************************
 * This is not really thread-safe as displayer_task might be in the middle of line drawing
 * but it won't crash (I think) and making it thread-safe would be complicated for a
 * feature which is secondary (the LMS version of scrolling is thread-safe)
 */
static void displayer_task(void *args) {
	int scroll_sleep = 0, timer_sleep;
	
	while (1) {
		// suspend ourselves if nothing to do
		if (displayer.state == DISPLAYER_DISABLED) {
			vTaskSuspend(NULL);
			display->clear();
			display->line(1, DISPLAY_LEFT, DISPLAY_UPDATE, displayer.header);
			scroll_sleep = 0;
		}	
		
		// we have been waken up before our requested time
		if (scroll_sleep <= 10) {
			// something to scroll (or we'll wake-up every pause ms ... no big deal)
			if (*displayer.string && displayer.state == DISPLAYER_ACTIVE) {
				display->line(2, -displayer.offset, DISPLAY_CLEAR | DISPLAY_UPDATE, displayer.string);
			
				xSemaphoreTake(displayer.mutex, portMAX_DELAY);
				scroll_sleep = displayer.offset ? displayer.speed : displayer.pause;
				displayer.offset = displayer.offset >= displayer.boundary ? 0 : (displayer.offset + min(displayer.by, displayer.boundary - displayer.offset));			
				xSemaphoreGive(displayer.mutex);
			} else if (displayer.state == DISPLAYER_AUTO_DISABLE) {
				display->line(2, DISPLAY_LEFT, DISPLAY_CLEAR | DISPLAY_UPDATE, displayer.string);
				xSemaphoreTake(displayer.mutex, portMAX_DELAY);
				displayer.state = DISPLAYER_DISABLED;
				xSemaphoreGive(displayer.mutex);
			} else scroll_sleep = DEFAULT_SLEEP;
		}	
		
		// handler timer
		if (displayer.timer && displayer.state == DISPLAYER_ACTIVE) {
			char counter[12];
			TickType_t tick = xTaskGetTickCount();
			uint32_t elapsed = (tick - displayer.tick) * portTICK_PERIOD_MS;
			
			if (elapsed >= 1000) {
				displayer.tick = tick;
				displayer.elapsed += elapsed / 1000;
				if (displayer.elapsed < 3600) sprintf(counter, "%2u:%02u", displayer.elapsed / 60, displayer.elapsed % 60);
				else sprintf(counter, "%2u:%2u:%02u", displayer.elapsed / 3600, (displayer.elapsed % 3600) / 60, displayer.elapsed % 60);
				display->line(1, DISPLAY_RIGHT, (DISPLAY_CLEAR | DISPLAY_ONLY_EOL) | DISPLAY_UPDATE, counter);
				timer_sleep = 1000;
			} else timer_sleep = 1000 - elapsed;	
		} else timer_sleep = DEFAULT_SLEEP;
		
		// don't bother sleeping if we are disactivated
		if (displayer.state == DISPLAYER_ACTIVE) {
			int sleep = min(scroll_sleep, timer_sleep);
			scroll_sleep -= sleep;
			vTaskDelay( sleep / portTICK_PERIOD_MS);
		}	
	}
}	

/****************************************************************************************
 * 
 */
void displayer_metadata(char *artist, char *album, char *title) {
	char *string = displayer.string, *p;
	int len = SCROLLABLE_SIZE;
	
	if (!displayer.metadata_config) {
		strncpy(displayer.string, title ? title : "", SCROLLABLE_SIZE);
		return;
	}
	
	xSemaphoreTake(displayer.mutex, portMAX_DELAY);
	
	// format metadata parameters and write them directly
	if ((p = strcasestr(displayer.metadata_config, "format")) != NULL) {
		char token[16], *q;
		int space = len;
		bool skip = false;
			
		displayer.string[0] = '\0';	
		p = strchr(displayer.metadata_config, '=');
			
		while (p++) {
			// find token and copy what's after when reaching last one
			if (sscanf(p, "%*[^%%]%%%[^%]%%", token) < 0) {
				q = strchr(p, ',');
				strncat(string, p, q ? min(q - p, space) : space);
				break;
			}

			// copy what's before token (be safe)
			if ((q = strchr(p, '%')) == NULL) break;
			
			// skip whatever is after a token if this token is empty
			if (!skip) {
				strncat(string, p, min(q - p, space));
				space = len - strlen(string);
			}	

			// then copy token's content
			if (strcasestr(q, "artist") && artist) strncat(string, p = artist, space);
			else if (strcasestr(q, "album") && album) strncat(string, p = album, space);
			else if (strcasestr(q, "title") && title) strncat(string, p = title, space);
			space = len - strlen(string);
				
			// flag to skip the data following an empty field
			if (*p) skip = false;
			else skip = true;

			// advance to next separator
			p = strchr(q + 1, '%');
		}
	} else {
		string = strdup(title ? title : "");
	}
	
	// get optional scroll speed
	if ((p = strcasestr(displayer.metadata_config, "speed")) != NULL) sscanf(p, "%*[^=]=%d", &displayer.speed);
	
	displayer.offset = 0;	
	utf8_decode(displayer.string);
	displayer.boundary = display->stretch(2, displayer.string, SCROLLABLE_SIZE);
		
	xSemaphoreGive(displayer.mutex);
}	


/****************************************************************************************
 *
 */
void displayer_scroll(char *string, int speed) {
	xSemaphoreTake(displayer.mutex, portMAX_DELAY);

	if (speed) displayer.speed = speed;
	displayer.offset = 0;	
	strncpy(displayer.string, string, SCROLLABLE_SIZE);
	displayer.string[SCROLLABLE_SIZE] = '\0';
	displayer.boundary = display->stretch(2, displayer.string, SCROLLABLE_SIZE);
		
	xSemaphoreGive(displayer.mutex);
}

/****************************************************************************************
 * 
 */
void displayer_timer(enum displayer_time_e mode, uint32_t elapsed, uint32_t duration) {
	xSemaphoreTake(displayer.mutex, portMAX_DELAY);

	displayer.elapsed = elapsed;	
	displayer.duration = duration;	
	displayer.timer = true;
	displayer.tick = xTaskGetTickCount();
		
	xSemaphoreGive(displayer.mutex);
}	

/****************************************************************************************
 * See above comment
 */
void displayer_control(enum displayer_cmd_e cmd, ...) {
	va_list args;
	
	va_start(args, cmd);
	xSemaphoreTake(displayer.mutex, portMAX_DELAY);
	
	displayer.offset = 0;	
	displayer.boundary = 0;
	
	switch(cmd) {
	case DISPLAYER_ACTIVATE: {	
		char *header = va_arg(args, char*);
		strncpy(displayer.header, header, HEADER_SIZE);
		displayer.header[HEADER_SIZE] = '\0';
		displayer.state = DISPLAYER_ACTIVE;
		displayer.timer = false;
		displayer.string[0] = '\0';
		vTaskResume(displayer.task);
		break;
	}	
	case DISPLAYER_DISABLE:		
		displayer.state = DISPLAYER_AUTO_DISABLE;
		break;		
	case DISPLAYER_SHUTDOWN:
		displayer.state = DISPLAYER_DISABLED;
		vTaskSuspend(displayer.task);
		break;
	case DISPLAYER_TIMER_RESUME:
		if (!displayer.timer) {
			displayer.timer = true;		
			displayer.tick = xTaskGetTickCount();		
		}	
		break;
	case DISPLAYER_TIMER_PAUSE:
		displayer.timer = false;
		break;
	default:
		break;
	}	
	
	xSemaphoreGive(displayer.mutex);
	va_end(args);
}