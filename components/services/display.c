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

#define min(a,b) (((a) < (b)) ? (a) : (b))

#define DISPLAYER_STACK_SIZE 	2048
#define SCROLLABLE_SIZE			384


static EXT_RAM_ATTR struct {
	TaskHandle_t task;
	SemaphoreHandle_t mutex;
	int pause, speed, by;
	enum { DISPLAYER_DISABLED, DISPLAYER_IDLE, DISPLAYER_ACTIVE } state;
	char string[SCROLLABLE_SIZE + 1];
	int offset, boundary;
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
		display->set_font(1, DISPLAY_FONT_TINY, 0);
		display->set_font(2, DISPLAY_FONT_LARGE, 0);
	}
	
	if (item) free(item);
}

/****************************************************************************************
 * 
 */
static void displayer_task(void *args) {
	while (1) {
		int scroll_pause = displayer.pause;
		
		// suspend ourselves if nothing to do
		if (displayer.state == DISPLAYER_DISABLED) {
			vTaskSuspend(NULL);
			display->clear();
		}	
		
		// something to scroll (or we'll wake-up every pause ms ... no big deal)
		if (*displayer.string && displayer.state == DISPLAYER_ACTIVE) {
			display->line(2, DISPLAY_LEFT, -displayer.offset, DISPLAY_CLEAR | DISPLAY_UPDATE, displayer.string);
			
			xSemaphoreTake(displayer.mutex, portMAX_DELAY);
			scroll_pause = displayer.offset ? displayer.speed : displayer.pause;
			displayer.offset = displayer.offset >= displayer.boundary ? 0 : (displayer.offset + min(displayer.by, displayer.boundary - displayer.offset));			
			xSemaphoreGive(displayer.mutex);
		} else if (displayer.state == DISPLAYER_IDLE) {
			display->line(2, DISPLAY_LEFT, 0, DISPLAY_CLEAR | DISPLAY_UPDATE, displayer.string);
			scroll_pause = displayer.pause;
		}
		
		vTaskDelay(scroll_pause / portTICK_PERIOD_MS);
	}
}	


/****************************************************************************************
 * This is not really thread-safe as displayer_task might be in the middle of line drawing
 * but it won't crash (I think) and making it thread-safe would be complicated for a
 * feature which is secondary (the LMS version of scrolling is thread-safe)
 */
void displayer_scroll(char *string, int speed) {
	xSemaphoreTake(displayer.mutex, portMAX_DELAY);

	if (speed) displayer.speed = speed;
	displayer.offset = 0;	
	displayer.state = DISPLAYER_ACTIVE;
	strncpy(displayer.string, string, SCROLLABLE_SIZE);
	displayer.string[SCROLLABLE_SIZE] = '\0';
	displayer.boundary = display->stretch(2, displayer.string, SCROLLABLE_SIZE);
		
	xSemaphoreGive(displayer.mutex);
}

/****************************************************************************************
 * See above comment
 */
void displayer_control(enum displayer_cmd_e cmd) {
	xSemaphoreTake(displayer.mutex, portMAX_DELAY);
	
	displayer.offset = 0;	
	displayer.boundary = 0;
	
	switch(cmd) {
	case DISPLAYER_SHUTDOWN:
		displayer.state = DISPLAYER_DISABLED;
		vTaskSuspend(displayer.task);
		break;
	case DISPLAYER_ACTIVATE:	
		displayer.state = DISPLAYER_ACTIVE;
		displayer.string[0] = '\0';
		vTaskResume(displayer.task);
		break;
	case DISPLAYER_SUSPEND:		
		displayer.state = DISPLAYER_IDLE;
		break;
	default:
		break;
	}	
	
	xSemaphoreGive(displayer.mutex);
}