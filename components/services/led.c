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
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "led.h"
#include "accessors.h"

#define MAX_LED	8
#define BLOCKTIME	10	// up to portMAX_DELAY

static const char *TAG = "led";

static struct led_s {
	gpio_num_t gpio;
	bool on;
	int onstate;
	int ontime, offtime;
	int pushedon, pushedoff;
	bool pushed;
	TimerHandle_t timer;
} leds[MAX_LED];

static struct {
	int gpio;
	int active;
} green = { CONFIG_LED_GREEN_GPIO, 0 },
  red = { CONFIG_LED_RED_GPIO, 0 };

/****************************************************************************************
 * 
 */
static void vCallbackFunction( TimerHandle_t xTimer ) {
	struct led_s *led = (struct led_s*) pvTimerGetTimerID (xTimer);
	
	if (!led->timer) return;
	
	led->on = !led->on;
	ESP_LOGD(TAG,"led vCallbackFunction setting gpio %d level", led->gpio);
	gpio_set_level(led->gpio, led->on ? led->onstate : !led->onstate);
	
	// was just on for a while
	if (!led->on && led->offtime == -1) return;
	
	// regular blinking
	xTimerChangePeriod(xTimer, (led->on ? led->ontime : led->offtime) / portTICK_RATE_MS, BLOCKTIME);
}

/****************************************************************************************
 * 
 */
bool led_blink_core(int idx, int ontime, int offtime, bool pushed) {
	if (!leds[idx].gpio || leds[idx].gpio<0 ) return false;
	
	ESP_LOGD(TAG,"led_blink_core");
	if (leds[idx].timer) {
		// normal requests waits if a pop is pending
		if (!pushed && leds[idx].pushed) {
			leds[idx].pushedon = ontime; 
			leds[idx].pushedoff = offtime; 
			return true;
		}
		xTimerStop(leds[idx].timer, BLOCKTIME);
	}
	
	// save current state if not already pushed
	if (!leds[idx].pushed) {
		leds[idx].pushedon = leds[idx].ontime;
		leds[idx].pushedoff = leds[idx].offtime;	
		leds[idx].pushed = pushed;
	}	
	
	// then set new one
	leds[idx].ontime = ontime;
	leds[idx].offtime = offtime;	
			
	if (ontime == 0) {
		ESP_LOGD(TAG,"led %d, setting reverse level", idx);
		gpio_set_level(leds[idx].gpio, !leds[idx].onstate);
	} else if (offtime == 0) {
		ESP_LOGD(TAG,"led %d, setting level", idx);
		gpio_set_level(leds[idx].gpio, leds[idx].onstate);
	} else {
		if (!leds[idx].timer) {
			ESP_LOGD(TAG,"led %d, Creating timer", idx);
			leds[idx].timer = xTimerCreate("ledTimer", ontime / portTICK_RATE_MS, pdFALSE, (void *)&leds[idx], vCallbackFunction);
		}
        leds[idx].on = true;
        ESP_LOGD(TAG,"led %d, Setting gpio %d", idx, leds[idx].gpio);
		gpio_set_level(leds[idx].gpio, leds[idx].onstate);
		ESP_LOGD(TAG,"led %d, Starting timer.", idx);
		if (xTimerStart(leds[idx].timer, BLOCKTIME) == pdFAIL) return false;
	}
	ESP_LOGD(TAG,"led %d, led_blink_core_done", idx);
	return true;
} 

/****************************************************************************************
 * 
 */
bool led_unpush(int idx) {
	if (!leds[idx].gpio || leds[idx].gpio<0) return false;
	
	led_blink_core(idx, leds[idx].pushedon, leds[idx].pushedoff, true);
	leds[idx].pushed = false;
	
	return true;
}	

/****************************************************************************************
 * 
 */
bool led_config(int idx, gpio_num_t gpio, int onstate) {
	if(gpio<0){
		ESP_LOGW(TAG,"LED GPIO not configured");
		return false;
	}
	ESP_LOGD(TAG,"Index %d, GPIO %d, on state %s", idx, gpio, onstate>0?"On":"Off");
	if (idx >= MAX_LED) return false;
	leds[idx].gpio = gpio;
	leds[idx].onstate = onstate;
	ESP_LOGD(TAG,"Index %d, GPIO %d, on state %s. Selecting GPIO pad", idx, gpio, onstate>0?"On":"Off");
	gpio_pad_select_gpio(gpio);
	ESP_LOGD(TAG,"Index %d, GPIO %d, on state %s. Setting direction to OUTPUT", idx, gpio, onstate>0?"On":"Off");
	gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
	ESP_LOGD(TAG,"Index %d, GPIO %d, on state %s. Setting State to %d", idx, gpio, onstate>0?"On":"Off", onstate);
	gpio_set_level(gpio, !onstate);
	ESP_LOGD(TAG,"Done configuring the led");
	return true;
}

/****************************************************************************************
 * 
 */
bool led_unconfig(int idx) {
	if (idx >= MAX_LED) return false;	
	
	if (leds[idx].timer) xTimerDelete(leds[idx].timer, BLOCKTIME);
	leds[idx].timer = NULL;
	
	return true;
}

/****************************************************************************************
 * 
 */
void set_led_gpio(int gpio, char *value) {
	char *p;
	
	if (strcasestr(value, "green")) {
		green.gpio = gpio;
		if ((p = strchr(value, ':')) != NULL) green.active = atoi(p + 1);
	} else 	if (strcasestr(value, "red")) {
		red.active = gpio;
		if ((p = strchr(value, ':')) != NULL) red.active = atoi(p + 1);
	}	
}

void led_svc_init(void) {
	
#ifdef CONFIG_LED_GREEN_GPIO_LEVEL
	green.active = CONFIG_LED_GREEN_GPIO_LEVEL;
#endif
#ifdef CONFIG_LED_RED_GPIO_LEVEL
	red.active = CONFIG_LED_RED_GPIO_LEVEL;
#endif

#ifndef CONFIG_LED_LOCKED
	parse_set_GPIO(set_led_gpio);
#endif
	ESP_LOGI(TAG,"Configuring LEDs green:%d (active:%d), red:%d (active:%d)", green.gpio, green.active, red.gpio, red.active);

	led_config(LED_GREEN, green.gpio, green.active);
	led_config(LED_RED, red.gpio, red.active);
}
