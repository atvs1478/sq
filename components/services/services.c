/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "battery.h"
#include "led.h"
#include "monitor.h"

extern void battery_svc_init(void);
extern void monitor_svc_init(void);
extern void led_svc_init(void);

static const char TAG[] = "services";

#ifdef CONFIG_SQUEEZEAMP
#define LED_GREEN_GPIO 	12
#define LED_RED_GPIO	13
#endif

/****************************************************************************************
 * 
 */
void services_init(void) {
	gpio_install_isr_service(0);

	ESP_LOGD(TAG,"Configuring LEDs");
	led_svc_init();
#ifdef CONFIG_SQUEEZEAMP
	led_config(LED_GREEN, LED_GREEN_GPIO, 0);
	led_config(LED_RED, LED_RED_GPIO, 0);
#endif

	battery_svc_init();
	monitor_svc_init();
}
