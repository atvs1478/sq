/* 
 *  a crude button press/long-press/shift management based on GPIO
 *
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
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_task.h"
#include "driver/gpio.h"
#include "buttons.h"
#include "globdefs.h"

bool gpio36_39_used;

static const char * TAG = "buttons";

static int n_buttons = 0;

#define BUTTON_STACK_SIZE	4096
#define MAX_BUTTONS			16
#define DEBOUNCE			50

static EXT_RAM_ATTR struct button_s {
	void *client;
	int gpio;
	int debounce;
	button_handler handler;
	struct button_s *self, *shifter;
	int shifter_gpio;	// this one is just for post-creation						
	int	long_press;
	bool long_timer, shifted, shifting;
	int type, level;	
	TimerHandle_t timer;
} buttons[MAX_BUTTONS];

static xQueueHandle button_evt_queue = NULL;

static void buttons_task(void* arg);

/****************************************************************************************
 * GPIO low-level handler
 */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
	struct button_s *button = (struct button_s*) arg;
	BaseType_t woken = pdFALSE;

	if (xTimerGetPeriod(button->timer) > button->debounce / portTICK_RATE_MS) xTimerChangePeriodFromISR(button->timer, button->debounce / portTICK_RATE_MS, &woken); // does that restart the timer? 
	else xTimerResetFromISR(button->timer, &woken);
	// ESP_EARLY_LOGI(TAG, "INT gpio %u level %u", button->gpio, button->level);
}

/****************************************************************************************
 * Buttons debounce/longpress timer
 */
static void buttons_timer( TimerHandle_t xTimer ) {
	struct button_s *button = (struct button_s*) pvTimerGetTimerID (xTimer);

	button->level = gpio_get_level(button->gpio);
	if (button->shifter && button->shifter->type == button->shifter->level) button->shifter->shifting = true;

	if (button->long_press && !button->long_timer && button->level == button->type) {
		// detect a long press, so hold event generation
		ESP_LOGD(TAG, "setting long timer gpio:%u level:%u", button->gpio, button->level);
		xTimerChangePeriod(xTimer, button->long_press / portTICK_RATE_MS, 0);
		button->long_timer = true;
	} else {
		// send a button pressed/released event (content is copied in queue)
		ESP_LOGD(TAG, "sending event for gpio:%u level:%u", button->gpio, button->level);
		// queue will have a copy of button's context
		xQueueSend(button_evt_queue, button, 0);
		button->long_timer = false;
	}
}

/****************************************************************************************
 * Tasks that calls the appropriate functions when buttons are pressed
 */
static void buttons_task(void* arg) {
	ESP_LOGI(TAG, "starting button tasks");
	
    while (1) {
		struct button_s button;
		button_event_e event;
		button_press_e press;

        if (!xQueueReceive(button_evt_queue, &button, portMAX_DELAY)) continue;

		event = (button.level == button.type) ? BUTTON_PRESSED : BUTTON_RELEASED;		

		ESP_LOGD(TAG, "received event:%u from gpio:%u level:%u (timer %u shifting %u)", event, button.gpio, button.level, button.long_timer, button.shifting);

		// find if shifting is activated
		if (button.shifter && button.shifter->type == button.shifter->level) press = BUTTON_SHIFTED;
		else press = BUTTON_NORMAL;
	
		/* 
		long_timer will be set either because we truly have a long press 
		or we have a release before the long press timer elapsed, so two 
		events shall be sent
		*/
		if (button.long_timer) {
			if (event == BUTTON_RELEASED) {
				// early release of a long-press button, send press/release
				if (!button.shifting) {
					(*button.handler)(button.client, BUTTON_PRESSED, press, false);		
					(*button.handler)(button.client, BUTTON_RELEASED, press, false);		
				}
				// button is a copy, so need to go to real context
				button.self->shifting = false;
			} else if (!button.shifting) {
				// normal long press and not shifting so don't discard
				(*button.handler)(button.client, BUTTON_PRESSED, press, true);
			}  
		} else {
			// normal press/release of a button or release of a long-press button
			if (!button.shifting) (*button.handler)(button.client, event, press, button.long_press);
			// button is a copy, so need to go to real context
			button.self->shifting = false;
		}
    }
}	
	
/****************************************************************************************
 * dummy button handler
 */	
void dummy_handler(void *id, button_event_e event, button_press_e press) {
	ESP_LOGW(TAG, "should not be here");
}

/****************************************************************************************
 * Create buttons 
 */
void button_create(void *client, int gpio, int type, bool pull, int debounce, button_handler handler, int long_press, int shifter_gpio) { 
	static DRAM_ATTR StaticTask_t xTaskBuffer __attribute__ ((aligned (4)));
	static EXT_RAM_ATTR StackType_t xStack[BUTTON_STACK_SIZE] __attribute__ ((aligned (4)));

	if (n_buttons >= MAX_BUTTONS) return;

	ESP_LOGI(TAG, "Creating button using GPIO %u, type %u, pull-up/down %u, long press %u shifter %u", gpio, type, pull, long_press, shifter_gpio);

	if (!n_buttons) {
		button_evt_queue = xQueueCreate(10, sizeof(struct button_s));
		xTaskCreateStatic( (TaskFunction_t) buttons_task, "buttons_thread", BUTTON_STACK_SIZE, NULL, ESP_TASK_PRIO_MIN + 1, xStack, &xTaskBuffer);
	}
	
	// just in case this structure is allocated in a future release
	memset(buttons + n_buttons, 0, sizeof(struct button_s));

	// set mandatory parameters
	buttons[n_buttons].client = client;
 	buttons[n_buttons].gpio = gpio;
 	buttons[n_buttons].debounce = debounce ? debounce: DEBOUNCE;
	buttons[n_buttons].handler = handler;
	buttons[n_buttons].long_press = long_press;
	buttons[n_buttons].level = -1;
	buttons[n_buttons].shifter_gpio = shifter_gpio;
	buttons[n_buttons].type = type;
	buttons[n_buttons].timer = xTimerCreate("buttonTimer", buttons[n_buttons].debounce / portTICK_RATE_MS, pdFALSE, (void *) &buttons[n_buttons], buttons_timer);
	buttons[n_buttons].self = buttons + n_buttons;

	for (int i = 0; i < n_buttons; i++) {
		// first try to find our shifter
		if (buttons[i].gpio == shifter_gpio) {
			buttons[n_buttons].shifter = buttons + i;
			// a shifter must have a long-press handler
			if (!buttons[i].long_press) buttons[i].long_press = -1;
		}
		// then try to see if we are a non-assigned shifter
		if (buttons[i].shifter_gpio == gpio) {
			buttons[i].shifter = buttons + n_buttons;
			ESP_LOGI(TAG, "post-assigned shifter gpio %u", buttons[i].gpio);			
		}	
	}

	gpio_pad_select_gpio(gpio);
	gpio_set_direction(gpio, GPIO_MODE_INPUT);

	// we need any edge detection
	gpio_set_intr_type(gpio, GPIO_INTR_ANYEDGE);

	// do we need pullup or pulldown
	if (pull) {
		if (GPIO_IS_VALID_OUTPUT_GPIO(gpio)) {
			if (type == BUTTON_LOW) gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY);
			else gpio_set_pull_mode(gpio, GPIO_PULLDOWN_ONLY);
		} else {	
			ESP_LOGW(TAG, "cannot set pull up/down for gpio %u", gpio);
		}
	}
	
	// nasty ESP32 bug: fire-up constantly INT on GPIO 36/39 if ADC1, AMP, Hall which WiFi when PS is activated
	if (gpio == 36 || gpio == 39) gpio36_39_used = true;

	gpio_isr_handler_add(gpio, gpio_isr_handler, (void*) &buttons[n_buttons]);
	gpio_intr_enable(gpio);

	n_buttons++;
}	

/****************************************************************************************
 * Get stored id
 */
void *button_get_client(int gpio) {
	 for (int i = 0; i < n_buttons; i++) {
		 if (buttons[i].gpio == gpio) return buttons[i].client;
	 }
	 return NULL;
 }

/****************************************************************************************
 * Update buttons 
 */
void *button_remap(void *client, int gpio, button_handler handler, int long_press, int shifter_gpio) { 
	int i;
	struct button_s *button = NULL;
	void *prev_client;
	
	ESP_LOGI(TAG, "remapping GPIO %u, long press %u shifter %u", gpio, long_press, shifter_gpio);

	// find button
	for (i = 0; i < n_buttons; i++) {
		if (buttons[i].gpio == gpio) {
			button = buttons + i;
			break;
		}	
	}	
	
	// huh
	if (!button) return NULL;	
	
	prev_client = button->client;
	button->client = client;
 	button->handler = handler;
	button->long_press = long_press;
	button->shifter_gpio = shifter_gpio;

	// find our shifter	(if any)	
	for (i = 0; shifter_gpio != -1 && i < n_buttons; i++) {
		if (buttons[i].gpio == shifter_gpio) {
			button->shifter = buttons + i;
			// a shifter must have a long-press handler
			if (!buttons[i].long_press) buttons[i].long_press = -1;
			break;
		}
	}
	
	return prev_client;
}
