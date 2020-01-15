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
#include "driver/adc.h"
#include "battery.h"

#define BATTERY_TIMER	(10*1000)

static const char *TAG = "battery";

static struct {
	float sum, avg;
	int count;
	TimerHandle_t timer;
} battery;

/****************************************************************************************
 * 
 */
#ifdef CONFIG_SQUEEZEAMP
static void battery_callback(TimerHandle_t xTimer) {

	battery.sum += adc1_get_raw(ADC1_CHANNEL_7) / 4095. * (10+174)/10. * 1.1;
	if (++battery.count == 30) {
		battery.avg = battery.sum / battery.count;
		battery.sum = battery.count = 0;
		ESP_LOGI(TAG, "Voltage %.2fV", battery.avg);
	}	

}
#endif

/****************************************************************************************
 * 
 */
void battery_svc_init(void) {
#ifdef CONFIG_SQUEEZEAMP			
	ESP_LOGI(TAG, "Initializing battery");

	adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_0);

	battery.timer = xTimerCreate("battery", BATTERY_TIMER / portTICK_RATE_MS, pdTRUE, NULL, battery_callback);
	xTimerStart(battery.timer, portMAX_DELAY);
#endif				
}
