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
 * Volume DOWN command
 */
static void volume_down(button_event_e event, button_press_e press) {
	ESP_LOGI(TAG, "volume down %u", event);
	//if (event == BUTTON_PRESSED) (*current_control.volume_down)();
}

static void volume_down_special(button_event_e event, button_press_e press) {
	if (press == BUTTON_LONG) ESP_LOGI(TAG, "volume down long %u", event);
	else ESP_LOGI(TAG, "volume down shifted %u", event);
	//if (event == BUTTON_PRESSED) (*current_control.volume_down)();
}

/****************************************************************************************
 * Volume UP commands
 */
static void volume_up(button_event_e event, button_press_e press) {
	ESP_LOGI(TAG, "volume up %u", event);
	//if (event == BUTTON_PRESSED) (*current_control.volume_up)();
}

static void volume_up_long(button_event_e event, button_press_e press) {
	if (press == BUTTON_LONG) ESP_LOGI(TAG, "volume up long %u", event);
	else ESP_LOGI(TAG, "volume up shifted %u", event);
	//if (event == BUTTON_PRESSED) (*current_control.volume_down)();
}

/****************************************************************************************
 * 
 */
void audio_controls_init(void) {
	/*
	button_create(18, BUTTON_LOW, true, volume_up, NULL);
	button_create(19, BUTTON_LOW, true, volume_down, "long", volume_down_long, 3000, NULL);
	button_create(21, BUTTON_LOW, true, volume_up, NULL);
	*/
	button_create(4, BUTTON_LOW, true, volume_up, NULL);
	button_create(5, BUTTON_LOW, true, volume_down, "long", volume_down_special, 3000, "shift", volume_down_special, 4, NULL);
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
