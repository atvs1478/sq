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
//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"
#include "buttons.h"
#include "audio_controls.h"


typedef esp_err_t (actrls_config_map_handler) (const cJSON * member, actrls_config_t *cur_config,uint32_t offset);
typedef struct {
	char * member;
	uint32_t offset;
	actrls_config_map_handler * handler;
} actrls_config_map_t;

esp_err_t actrls_process_member(const cJSON * member, actrls_config_t *cur_config);
esp_err_t actrls_process_button(const cJSON * button, actrls_config_t *cur_config);
esp_err_t actrls_process_int (const cJSON * member, actrls_config_t *cur_config, uint32_t offset);
esp_err_t actrls_process_type (const cJSON * member, actrls_config_t *cur_config, uint32_t offset);
esp_err_t actrls_process_bool (const cJSON * member, actrls_config_t *cur_config, uint32_t offset);
esp_err_t actrls_process_action (const cJSON * member, actrls_config_t *cur_config, uint32_t offset);

static const actrls_config_map_t actrls_config_map[] =
		{
			{"gpio", offsetof(actrls_config_t,gpio), actrls_process_int},
			{"debounce", offsetof(actrls_config_t,debounce), actrls_process_int},
			{"type", offsetof(actrls_config_t,type), actrls_process_type},
			{"pull", offsetof(actrls_config_t,pull), actrls_process_bool},
			{"long_press", offsetof(actrls_config_t,long_press),actrls_process_int},
			{"shifter_gpio", offsetof(actrls_config_t,shifter_gpio), actrls_process_int},
			{"normal", offsetof(actrls_config_t,normal), actrls_process_action},
			{"shifted", offsetof(actrls_config_t,shifted), actrls_process_action},
			{"longpress", offsetof(actrls_config_t,longpress), actrls_process_action},
			{"longshifted", offsetof(actrls_config_t,longshifted), actrls_process_action},
			{"", 0, NULL}
		};

// BEWARE: the actions below need to stay aligned with the corresponding enum to properly support json parsing
#define EP(x) [x] = #x  /* ENUM PRINT */
static const char * actrls_action_s[ ] = { EP(ACTRLS_VOLUP),EP(ACTRLS_VOLDOWN),EP(ACTRLS_TOGGLE),EP(ACTRLS_PLAY),
									EP(ACTRLS_PAUSE),EP(ACTRLS_STOP),EP(ACTRLS_REW),EP(ACTRLS_FWD),EP(ACTRLS_PREV),EP(ACTRLS_NEXT),
									EP(BCTRLS_PUSH), EP(BCTRLS_UP),EP(BCTRLS_DOWN),EP(BCTRLS_LEFT),EP(BCTRLS_RIGHT), ""} ;
									
static const char * TAG = "audio controls";
static actrls_config_t *json_config;
static actrls_t default_controls, current_controls;

static void control_handler(void *id, button_event_e event, button_press_e press, bool long_press) {
	actrls_config_t *key = (actrls_config_t*) id;
	actrls_action_e action;

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
	
	ESP_LOGD(TAG, "control gpio:%u press:%u long:%u event:%u action:%u", key->gpio, press, long_press, event, action);

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
		button_create((void*) (config + i), config[i].gpio, config[i].type, config[i].pull, config[i].debounce, control_handler, config[i].long_press, config[i].shifter_gpio);
	}
	return ESP_OK;
}

actrls_action_e actrls_parse_action_json(const char * name){

	for(int i=0;i<ACTRLS_MAX && actrls_action_s[i][0]!='\0' ;i++){
		if(!strcmp(actrls_action_s[i], name)){
			return (actrls_action_e) i;
		}
	}
	return ACTRLS_NONE;
}

esp_err_t actrls_process_int (const cJSON * member, actrls_config_t *cur_config,uint32_t offset){
	esp_err_t err = ESP_OK;
	ESP_LOGD(TAG,"Processing int member");
	int *value = (int*)((char*) cur_config + offset);
	*value = member->valueint;
	return err;
}
esp_err_t actrls_process_type (const cJSON * member, actrls_config_t *cur_config, uint32_t offset){
	esp_err_t err = ESP_OK;
	ESP_LOGD(TAG,"Processing type member");
	int *value = (int *)((char*) cur_config + offset);
	if (member->type == cJSON_String) {
		*value =
				!strcmp(member->valuestring,
						"BUTTON_LOW") ?
						BUTTON_LOW : BUTTON_HIGH;
	} else {
		ESP_LOGE(TAG,
				"Button type value expected string value of BUTTON_LOW or BUTTON_HIGH, none found");
		err=ESP_FAIL;
	}
	return err;
}

esp_err_t actrls_process_bool (const cJSON * member, actrls_config_t *cur_config, uint32_t offset){
	esp_err_t err = ESP_OK;
	if(!member) {
		ESP_LOGE(TAG,"Null json member pointer!");
		err = ESP_FAIL;
	}
	else {
		ESP_LOGD(TAG,"Processing bool member ");
		if(cJSON_IsBool(member)){
			bool*value = (bool*)((char*) cur_config + offset);
			*value = cJSON_IsTrue(member);
		}
		else {
			ESP_LOGE(TAG,"Member %s is not a boolean", member->string?member->string:"unknown");
			err = ESP_FAIL;
		}
	}

	return err;
}
esp_err_t actrls_process_action (const cJSON * member, actrls_config_t *cur_config, uint32_t offset){
	esp_err_t err = ESP_OK;
	cJSON * button_action= cJSON_GetObjectItemCaseSensitive(member, "pressed");
	actrls_action_e*value = (actrls_action_e*)((char *)cur_config + offset);
	if (button_action != NULL) {
		value[0] = actrls_parse_action_json( button_action->valuestring);
	}
	else{
		ESP_LOGW(TAG,"Action pressed not found in json structure");
		err = ESP_FAIL;
	}
	button_action = cJSON_GetObjectItemCaseSensitive(member, "released");
	if (button_action != NULL) {
		value[1] = actrls_parse_action_json( button_action->valuestring);
	}
	else{
		ESP_LOGW(TAG,"Action released not found in json structure");
		err = ESP_FAIL;
	}

	return err;
}


esp_err_t actrls_process_member(const cJSON * member, actrls_config_t *cur_config) {
	esp_err_t err = ESP_FAIL;
	const actrls_config_map_t * h=actrls_config_map;

	char * str = cJSON_Print(member);
	while (h->handler && strcmp(member->string, h->member)) { h++; }

	if (h->handler) {
		ESP_LOGD(TAG,"found handler for member %s, json value %s", h->member,str?str:"");
		err = h->handler(member, cur_config, h->offset);
	} else {
		ESP_LOGE(TAG, "Unknown json structure member : %s", str?str:"");
	}

	if(str) free(str);
	return err;
}

esp_err_t actrls_process_button(const cJSON * button, actrls_config_t *cur_config) {
	esp_err_t err= ESP_FAIL;
	const cJSON *member;

	cJSON_ArrayForEach(member, button)
	{
		ESP_LOGD(TAG,"Processing member %s. ", member->string);
		esp_err_t loc_err = actrls_process_member(member, cur_config);
		err = (err == ESP_OK) ? loc_err : err;
	}
	return err;

}

actrls_config_t * actrls_init_alloc_structure(const cJSON *buttons){
	int member_count = 0;
	const cJSON *button;
	actrls_config_t * json_config=NULL;
	ESP_LOGD(TAG,"Counting the number of buttons definition");
	cJSON_ArrayForEach(button, buttons)	{
		member_count++;
	}
	ESP_LOGD(TAG, "config contains %u button definitions",
			member_count);
	if (member_count != 0) {
		json_config = malloc(sizeof(actrls_config_t) * member_count);
		if(json_config){
			memset(json_config, 0x00, sizeof(actrls_config_t) * member_count);
		}
		else {
			ESP_LOGE(TAG,"Unable to allocate memory to hold configuration for %u buttons ",member_count);
		}

	}
	else {
		ESP_LOGE(TAG,"No button found in configuration structure");
	}

	return json_config;
}

/****************************************************************************************
 * 
 */
esp_err_t actrls_init_json(const char *config) {
	esp_err_t err = ESP_OK;
	actrls_config_t *cur_config;
	const cJSON *button;
	ESP_LOGD(TAG,"Parsing JSON structure ");
	cJSON *buttons = cJSON_Parse(config);
	if (buttons == NULL) {
		ESP_LOGE(TAG,"JSON Parsing failed for %s", config);
		err = ESP_FAIL;
	}
	else {
		ESP_LOGD(TAG,"Json parsing completed");
		if (cJSON_IsArray(buttons)) {
			ESP_LOGD(TAG,"configuration is an array as expected");
			cur_config = json_config = actrls_init_alloc_structure(buttons);
			if(!cur_config) {
				ESP_LOGE(TAG,"Config buffer was empty. ");
				cJSON_Delete(buttons);
				return ESP_FAIL;
			}
			ESP_LOGD(TAG,"Processing button definitions. ");
			cJSON_ArrayForEach(button, buttons){
				char * str = cJSON_Print(button);
				ESP_LOGD(TAG,"Processing %s. ", str?str:"");
				if(str){
					free(str);
				}
				esp_err_t loc_err = actrls_process_button(button, cur_config);
				err = (err == ESP_OK) ? loc_err : err;
				if (loc_err == ESP_OK) {
					button_create((void*) cur_config, cur_config->gpio,cur_config->type, cur_config->pull,cur_config->debounce,
									control_handler, cur_config->long_press, cur_config->shifter_gpio);
				}
				cur_config++;
			}
		}
		else {
			ESP_LOGE(TAG,"Invalid configuration; array is expected and none received in %s ", config);
		}
		cJSON_Delete(buttons);
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
