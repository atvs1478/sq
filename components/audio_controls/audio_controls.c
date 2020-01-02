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
#include <unistd.h>
#include "esp_system.h"
#include "esp_log.h"
#include "buttons.h"
#include "audio_controls.h"

static const char * TAG = "audio_controls";

typedef struct {
	void (*volume_up)(void);
	void (*volume_down)(void);
	void (*play_toggle)(void);
	void (*play)(void);
	void (*pause)(void);
	void (*stop)(void);
} audio_control_t;

static audio_control_t default_control, current_control;

/****************************************************************************************
 * DOWN button
 */
static void down(button_event_e event, button_press_e press, bool long_press) {
	if (press == BUTTON_NORMAL) {
		if (!long_press) ESP_LOGI(TAG, "volume DOWN %u", event);
		else ESP_LOGI(TAG, "volume DOWN long %u", event);
	} else {
		if (!long_press) ESP_LOGI(TAG, "volume DOWN shifted %u", event);
		else ESP_LOGI(TAG, "volume DOWN long shifted %u", event);
	}
	//if (event == BUTTON_PRESSED) (*current_control.volume_down)();
}

/****************************************************************************************
 * UP button
 */
static void up(button_event_e event, button_press_e press, bool long_press) {
	if (press == BUTTON_NORMAL) {
		if (!long_press) ESP_LOGI(TAG, "volume UP %u", event);
		else ESP_LOGI(TAG, "volume UP long %u", event);
	} else {
		if (!long_press) ESP_LOGI(TAG, "volume UP shifted %u", event);
		else ESP_LOGI(TAG, "volume UP long shifted %u", event);
	}
	//if (event == BUTTON_PRESSED) (*current_control.volume_up)();
}

/****************************************************************************************
 * 
 */
void audio_controls_init(void) {
	/*
	button_create(18, BUTTON_LOW, true, up, 0, -1);
	button_create(19, BUTTON_LOW, true, down, 0, -1);
	button_create(21, BUTTON_LOW, true, play, 0, -1);
	*/
	button_create(4, BUTTON_LOW, true, up, 2000, -1);
	button_create(5, BUTTON_LOW, true, down, 3000, 4);
}

/****************************************************************************************
 * 
 */
void default_audio_control(audio_control_t *control) {
	default_control = current_control = *control;
}

/****************************************************************************************
 * 
 */
void register_audio_control(audio_control_t *control) {
	current_control = *control;
}

/****************************************************************************************
 * 
 */
void deregister_audio_control(void) {
	current_control = default_control;	
}
