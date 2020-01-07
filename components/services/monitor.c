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

#define MONITOR_TIMER	(10*1000)

static const char TAG[] = "monitor";

static TimerHandle_t monitor_timer;

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
void monitor_svc_init(void) {
	ESP_LOGI(TAG, "Initializing monitoring");

	monitor_timer = xTimerCreate("monitor", MONITOR_TIMER / portTICK_RATE_MS, pdTRUE, NULL, monitor_callback);
	xTimerStart(monitor_timer, portMAX_DELAY);
}
