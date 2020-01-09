/* 
 *  audio control callbacks
 *
 *  (c) Sebastien 2019
 *      Philippe G. 2019, philippe_44@outlook.com
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
 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "esp_system.h"
#include "esp_log.h"
#include "buttons.h"
#include "audio_controls.h"

static const char * TAG = "audio controls";

static actrls_t default_controls, current_controls;

static void control_handler(void *id, button_event_e event, button_press_e press, bool long_press) {
	actrls_config_t *key = (actrls_config_t*) id;
	actrls_action_e action;

	ESP_LOGD(TAG, "control gpio:%u press:%u long:%u event:%u", key->gpio, press, long_press, event);
	
	switch(press) {
	case BUTTON_NORMAL:
		if (long_press) action = key->longpress[event == BUTTON_PRESSED ? 0 : 1];
		else action = key->normal[event == BUTTON_PRESSED ? 0 : 1];
		break;
	case BUTTON_SHIFTED:
		if (long_press) action = key->longshifted[event == BUTTON_PRESSED ? 0 : 1];
		else action = key->shifted[event == BUTTON_PRESSED ? 0 : 1];
		break;
	default:
		action = ACTRLS_NONE;
		break;
	}

	if (action != ACTRLS_NONE) {
		ESP_LOGD(TAG, " calling action %u", action);
		if (current_controls[action]) (*current_controls[action])();
	}
}

/*
void up(void *id, button_event_e event, button_press_e press, bool longpress) {
	if (press == BUTTON_NORMAL) {
		if (longpress) ESP_LOGI(TAG, "up long %u", event);
		else ESP_LOGI(TAG, "up %u", event);
	} else if (press == BUTTON_SHIFTED) {
		if (longpress) ESP_LOGI(TAG, "up shifted long %u", event);
		else ESP_LOGI(TAG, "up shifted %u", event);
	} else {
		ESP_LOGI(TAG, "don't know what we are doing here %u", event);
	}
}

void down(void *id, button_event_e event, button_press_e press, bool longpress) {
	if (press == BUTTON_NORMAL) {
		if (longpress) ESP_LOGI(TAG, "down long %u", event);
		else ESP_LOGI(TAG, "down %u", event);
	} else if (press == BUTTON_SHIFTED) {
		if (longpress) ESP_LOGI(TAG, "down shifted long %u", event);
		else ESP_LOGI(TAG, "down shifted %u", event);
	} else {
		ESP_LOGI(TAG, "don't know what we are doing here %u", event);
	}
}
*/

/****************************************************************************************
 * 
 */
void actrls_init(int n, const actrls_config_t *config) {
	for (int i = 0; i < n; i++) {
		button_create((void*) (config + i), config[i].gpio, config[i].type, config[i].pull, config[i].debounce, control_handler, config[i].long_press, config[i].shifter_gpio);
	}
}

/****************************************************************************************
 * 
 */
void actrls_set_default(const actrls_t controls) {
	memcpy(default_controls, controls, sizeof(actrls_t));
	memcpy(current_controls, default_controls, sizeof(actrls_t));
}

/****************************************************************************************
 * 
 */
void actrls_set(const actrls_t controls) {
	memcpy(current_controls, controls, sizeof(actrls_t));
}

/****************************************************************************************
 * 
 */
void actrls_unset(void) {
	memcpy(current_controls, default_controls, sizeof(actrls_t));
}
