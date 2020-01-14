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

#ifdef CONFIG_SQUEEZEAMP
#define JACK_GPIO		34
#define SPKFAULT_GPIO	2			// this requires a pull-up, so can't be >34
#endif

#define MONITOR_TIMER	(10*1000)

static const char *TAG = "monitor";

static TimerHandle_t monitor_timer;

void (*jack_handler_svc)(bool inserted);
bool jack_inserted_svc(void);

void (*spkfault_handler_svc)(bool inserted);
bool spkfault_svc(void);

/****************************************************************************************
 * 
 */
static void monitor_callback(TimerHandle_t xTimer) {
	ESP_LOGI(TAG, "Heap internal:%zu (min:%zu) external:%zu (min:%zu)", 
			heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
			heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
			heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
			heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
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
#ifdef JACK_GPIO
	return !gpio_get_level(JACK_GPIO);
#else
	return false;
#endif
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
#ifdef SPKFAULT_GPIO
	return !gpio_get_level(SPKFAULT_GPIO);
#else
	return false;
#endif
}

#include "driver/rtc_io.h" 
/****************************************************************************************
 * 
 */
void monitor_svc_init(void) {
	ESP_LOGI(TAG, "Initializing monitoring");

#ifdef JACK_GPIO
	gpio_pad_select_gpio(JACK_GPIO);
	gpio_set_direction(JACK_GPIO, GPIO_MODE_INPUT);

	// re-use button management for jack handler, it's a GPIO after all
	button_create(NULL, JACK_GPIO, BUTTON_LOW, false, 250, jack_handler_default, 0, -1);
#endif

#ifdef SPKFAULT_GPIO
	gpio_pad_select_gpio(SPKFAULT_GPIO);
	gpio_set_direction(SPKFAULT_GPIO, GPIO_MODE_INPUT);
	gpio_set_pull_mode(SPKFAULT_GPIO, GPIO_PULLUP_ONLY); 
	
	// re-use button management for speaker fault handler, it's a GPIO after all
	button_create(NULL, SPKFAULT_GPIO, BUTTON_LOW, true, 0, spkfault_handler_default, 0, -1);
#endif

	monitor_timer = xTimerCreate("monitor", MONITOR_TIMER / portTICK_RATE_MS, pdTRUE, NULL, monitor_callback);
	xTimerStart(monitor_timer, portMAX_DELAY);
}
