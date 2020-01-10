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
#include "cJSON.h"
#include "buttons.h"
#include "audio_controls.h"
// BEWARE: the actions below need to stay aligned with the corresponding enum to properly support json parsing
static const char * actrls_action_s[ ] = { "ACTRLS_VOLUP","ACTRLS_VOLDOWN","ACTRLS_TOGGLE","ACTRLS_PLAY",
									"ACTRLS_PAUSE","ACTRLS_STOP","ACTRLS_REW","ACTRLS_FWD","ACTRLS_PREV","ACTRLS_NEXT",
									"BCTRLS_PUSH", "BCTRLS_UP","BCTRLS_DOWN","BCTRLS_LEFT","BCTRLS_RIGHT", ""} ;


static const char * TAG = "audio controls";
static actrls_config_t *json_config;
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
esp_err_t actrls_init(int n, const actrls_config_t *config) {
	for (int i = 0; i < n; i++) {
		button_create((void*) (config + i), config[i].gpio, config[i].type, config[i].pull, control_handler, config[i].long_press, config[i].shifter_gpio);
	}
	return ESP_OK;
}

actrls_action_e actrls_parse_action_json(const char * name){

	for(int i=0;actrls_action_s[i][0]!='\0' && i<ACTRLS_MAX;i++){
		if(!strcmp(actrls_action_s[i], name)){
			return (actrls_action_e) i;
		}
	}
	return ACTRLS_NONE;
}
esp_err_t actrls_parse_config_map(const cJSON * member, actrls_config_t *cur_config){
	const actrls_config_map_t * map=NULL;
	esp_err_t err=ESP_OK;
	char *config_pointer = (char*) cur_config;
	cJSON *button_action;

	char * string = cJSON_Print(member);
	ESP_LOGD(TAG, "Processing structure member json : %s", string);
	free(string);

	for(int i = 0;!map && actrls_config_map[i].type!=ACTRLS_MAP_END ;i++){
		if(!strcmp(member->string, actrls_config_map[i].member)){
			map = &actrls_config_map[i];
		}
	}

	if(!map){
		ESP_LOGE(TAG,"Unknown structure member [%s]", member->string?member->string:"");
		return ESP_FAIL;
	}

	ESP_LOGD(TAG,
			"Found map type %d at structure offset %u",
			map->type, map->offset);
	void *value = (config_pointer + map->offset);
	switch (map->type) {
	case ACTRLS_MAP_TYPE:
		if (member->child != NULL) {
			*(int*) value =
					!strcmp(member->child->string,
							"BUTTON_LOW") ?
							BUTTON_LOW : BUTTON_HIGH;
		} else {
			ESP_LOGE(TAG,
					"Button type value expected string value of BUTTON_LOW or BUTTON_HIGH, none found");
		}
		break;
	case ACTRLS_MAP_INT:
		*(int*) value = member->valueint;
		break;
	case ACTRLS_MAP_BOOL:
		*(bool*) value = cJSON_IsTrue(member);
		break;
	case ACTRLS_MAP_ACTION:
		button_action= cJSON_GetObjectItemCaseSensitive(
				member, "pressed");
		if (button_action != NULL) {
			((actrls_action_e*) value)[0] =
					actrls_parse_action_json(
							button_action->string);
		}
		button_action = cJSON_GetObjectItemCaseSensitive(
				member, "released");
		if (button_action != NULL) {
			((actrls_action_e*) value)[1] =
					actrls_parse_action_json(
							button_action->string);
		}
		break;

	default:
		break;
	}


	if (err == ESP_OK) {
		button_create((void*) cur_config, cur_config->gpio,
				cur_config->type, cur_config->pull,
				control_handler, cur_config->long_press,
				cur_config->shifter_gpio);
	}

	return err;
}


/****************************************************************************************
 * 
 */
esp_err_t actrls_init_json(const char *config) {
	esp_err_t err = ESP_OK;

	actrls_config_t *cur_config;

	cJSON *buttons = cJSON_Parse(config);
	if (buttons != NULL) {
		if (cJSON_IsArray(buttons)) {
			int member_count = 0;
			const cJSON *button;
			const cJSON *member;
			cJSON_ArrayForEach(button, buttons)
			{
				member_count++;
			}
			ESP_LOGD(TAG, "config contains %u button definitions",
					member_count);
			if (member_count == 0) {
				return ESP_FAIL;
			}
			json_config = malloc(sizeof(actrls_config_t) * member_count);
			memset(json_config, 0x00, sizeof(actrls_config_t) * member_count);
			cur_config = json_config;

			cJSON_ArrayForEach(button, buttons)
			{
				esp_err_t loc_err = ESP_OK;
				cJSON_ArrayForEach(member, button)
				{
					actrls_parse_config_map(member, cur_config);
					err = err == ESP_OK ? loc_err : err;
				}
				cur_config++;
			}
		}

		cJSON_Delete(buttons);
	} else {
		err = ESP_FAIL;
	}

	return err;
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
