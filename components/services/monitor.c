/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_log.h"
#include "monitor.h"
#include "driver/gpio.h"
#include "buttons.h"
#include "led.h"
#include "globdefs.h"
#include "config.h"
#include "accessors.h"

#define MONITOR_TIMER	(10*1000)

static const char *TAG = "monitor";

static TimerHandle_t monitor_timer;

static struct {
	int gpio;
	int active;
} 	jack = { CONFIG_JACK_GPIO, 0 },
	spkfault = { CONFIG_SPKFAULT_GPIO, 0 };

void (*jack_handler_svc)(bool inserted);
bool jack_inserted_svc(void);

void (*spkfault_handler_svc)(bool inserted);
bool spkfault_svc(void);

/****************************************************************************************
 * 
 */
static void task_stats( void ) {
	static struct {
		TaskStatus_t *tasks;
		uint32_t total, n;
	} current, previous;
	
	current.n = uxTaskGetNumberOfTasks();
	current.tasks = malloc( current.n * sizeof( TaskStatus_t ) );
	current.n = uxTaskGetSystemState( current.tasks, current.n, &current.total );
	
	static EXT_RAM_ATTR char scratch[128+1];
	*scratch = '\0';

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
	uint32_t elapsed = current.total - previous.total;
    
	for(int i = 0, n = 0; i < current.n; i++ ) {
		for (int j = 0; j < previous.n; j++) {
			if (current.tasks[i].xTaskNumber == previous.tasks[j].xTaskNumber) {
				n += sprintf(scratch + n, "%16s %2u%% s:%5u)", current.tasks[i].pcTaskName, 
																		   100 * (current.tasks[i].ulRunTimeCounter - previous.tasks[j].ulRunTimeCounter) / elapsed, 
																		   current.tasks[i].usStackHighWaterMark);
				if (i % 3 == 2 || i == current.n - 1) {
					ESP_LOGI(TAG, "%s", scratch);
					n = 0;
				}	
				break;
			}
		}	
	}	
#else
	for (int i = 0, n = 0; i < current.n; i ++) {
		n += sprintf(scratch + n, "%16s s:%5u\t", current.tasks[i].pcTaskName, current.tasks[i].usStackHighWaterMark);
		if (i % 3 == 2 || i == current.n - 1) {
			ESP_LOGI(TAG, "%s", scratch);
			n = 0;
		}	
	}
#endif	
	
	if (previous.tasks) free(previous.tasks);
	previous = current;
}
 
/****************************************************************************************
 * 
 */
static void monitor_callback(TimerHandle_t xTimer) {
	ESP_LOGI(TAG, "Heap internal:%zu (min:%zu) external:%zu (min:%zu)", 
			heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
			heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
			heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
			heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
			
	task_stats();
}

/****************************************************************************************
 * 
 */
static void jack_handler_default(void *id, button_event_e event, button_press_e mode, bool long_press) {
	ESP_LOGD(TAG, "Jack %s", event == BUTTON_PRESSED ? "inserted" : "removed");
	if (jack_handler_svc) (*jack_handler_svc)(event == BUTTON_PRESSED);
}

/****************************************************************************************
 * 
 */
bool jack_inserted_svc (void) {
	return button_is_pressed(jack.gpio, NULL);
}

/****************************************************************************************
 * 
 */
static void spkfault_handler_default(void *id, button_event_e event, button_press_e mode, bool long_press) {
	ESP_LOGD(TAG, "Speaker status %s", event == BUTTON_PRESSED ? "faulty" : "normal");
	if (event == BUTTON_PRESSED) led_on(LED_RED);
	else led_off(LED_RED);
	if (spkfault_handler_svc) (*spkfault_handler_svc)(event == BUTTON_PRESSED);
}

/****************************************************************************************
 * 
 */
bool spkfault_svc (void) {
	return button_is_pressed(spkfault.gpio, NULL);
}

/****************************************************************************************
 * 
 */
void set_jack_gpio(int gpio, char *value) {
	if (strcasestr(value, "jack")) {
		char *p;
		jack.gpio = gpio;	
		if ((p = strchr(value, ':')) != NULL) jack.active = atoi(p + 1);
	}	
}

/****************************************************************************************
 * 
 */
void set_spkfault_gpio(int gpio, char *value) {
	if (strcasestr(value, "spkfault")) {
		char *p;
		spkfault.gpio = gpio;	
		if ((p = strchr(value, ':')) != NULL) spkfault.active = atoi(p + 1);
	}	
}

/****************************************************************************************
 * 
 */
void monitor_svc_init(void) {
	ESP_LOGI(TAG, "Initializing monitoring");
	
#ifdef CONFIG_JACK_GPIO_LEVEL
	jack.active = CONFIG_JACK_GPIO_LEVEL;
#endif

#ifndef CONFIG_JACK_LOCKED
	parse_set_GPIO(set_jack_gpio);
#endif	

	// re-use button management for jack handler, it's a GPIO after all
	if (jack.gpio != -1) {
		ESP_LOGI(TAG,"Adding jack (%s) detection GPIO %d", jack.active ? "high" : "low", jack.gpio);					 
		button_create(NULL, jack.gpio, jack.active ? BUTTON_HIGH : BUTTON_LOW, false, 250, jack_handler_default, 0, -1);
	}	
	
#ifdef CONFIG_SPKFAULT_GPIO_LEVEL
	spkfault.active = CONFIG_SPKFAULT_GPIO_LEVEL;
#endif	

#ifndef CONFIG_SPKFAULT_LOCKED
	parse_set_GPIO(set_spkfault_gpio);
#endif

	// re-use button management for speaker fault handler, it's a GPIO after all
	if (spkfault.gpio != -1) {
		ESP_LOGI(TAG,"Adding speaker fault (%s) detection GPIO %d", spkfault.active ? "high" : "low", spkfault.gpio);					 
		button_create(NULL, spkfault.gpio, spkfault.active ? BUTTON_HIGH : BUTTON_LOW, false, 0, spkfault_handler_default, 0, -1);
	}	

	// do we want stats
	char *p = config_alloc_get_default(NVS_TYPE_STR, "stats", "n", 0);
	if (p && (*p == '1' || *p == 'Y' || *p == 'y')) {
		monitor_timer = xTimerCreate("monitor", MONITOR_TIMER / portTICK_RATE_MS, pdTRUE, NULL, monitor_callback);
		xTimerStart(monitor_timer, portMAX_DELAY);
	}	
	free(p);
	
	ESP_LOGI(TAG, "Heap internal:%zu (min:%zu) external:%zu (min:%zu)", 
			heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
			heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
			heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
			heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
}
