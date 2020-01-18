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
//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"
#include "buttons.h"
#include "config.h"
#include "audio_controls.h"

typedef esp_err_t (actrls_config_map_handler) (const cJSON * member, actrls_config_t *cur_config,uint32_t offset);
typedef struct {
	char * member;
	uint32_t offset;
	actrls_config_map_handler * handler;
} actrls_config_map_t;

static esp_err_t actrls_process_member(const cJSON * member, actrls_config_t *cur_config);
static esp_err_t actrls_process_button(const cJSON * button, actrls_config_t *cur_config);
static esp_err_t actrls_process_int (const cJSON * member, actrls_config_t *cur_config, uint32_t offset);
static esp_err_t actrls_process_type (const cJSON * member, actrls_config_t *cur_config, uint32_t offset);
static esp_err_t actrls_process_bool (const cJSON * member, actrls_config_t *cur_config, uint32_t offset);
static esp_err_t actrls_process_action (const cJSON * member, actrls_config_t *cur_config, uint32_t offset);

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
cJSON * control_profiles = NULL;
static actrls_t default_controls, current_controls;
static actrls_hook_t *default_hook, *current_hook;

static void control_handler(void *client, button_event_e event, button_press_e press, bool long_press) {
	actrls_config_t *key = (actrls_config_t*) client;
	actrls_action_detail_t  action_detail;

	switch(press) {
	case BUTTON_NORMAL:
		if (long_press) action_detail = key->longpress[event == BUTTON_PRESSED ? 0 : 1];
		else action_detail = key->normal[event == BUTTON_PRESSED ? 0 : 1];
		break;
	case BUTTON_SHIFTED:
		if (long_press) action_detail = key->longshifted[event == BUTTON_PRESSED ? 0 : 1];
		else action_detail = key->shifted[event == BUTTON_PRESSED ? 0 : 1];
		break;
	default:
		action_detail.action = ACTRLS_NONE;
		break;
	}
	
	ESP_LOGD(TAG, "control gpio:%u press:%u long:%u event:%u action:%u", key->gpio, press, long_press, event,action_detail.action);

	// stop here if control hook served the request
	if (current_hook && (*current_hook)(key->gpio, action_detail.action, event, press, long_press)) return;
	
	// otherwise process using configuration
	if (action_detail.action == ACTRLS_REMAP) {
		// remap requested
		ESP_LOGD(TAG, "remapping buttons to profile %s",action_detail.name);
		cJSON * profile_obj = cJSON_GetObjectItem(control_profiles,action_detail.name);
		if (profile_obj) {
			actrls_config_t *cur_config  = (actrls_config_t *) cJSON_GetStringValue(profile_obj);
			if (cur_config) {
				ESP_LOGD(TAG,"Remapping all the buttons that are found in the new profile");
				while (cur_config->gpio != -1) {
					ESP_LOGD(TAG,"Remapping button with gpio %u", cur_config->gpio);
					button_remap((void*) cur_config, cur_config->gpio, control_handler, cur_config->long_press, cur_config->shifter_gpio);
					cur_config++;
				}
			} else {
				ESP_LOGE(TAG,"Profile %s exists, but is empty. Cannot remap buttons",action_detail.name);
			}
		} else {
			ESP_LOGE(TAG,"Invalid profile name %s. Cannot remap buttons",action_detail.name);
		}	
	} else if (action_detail.action != ACTRLS_NONE) {
		ESP_LOGD(TAG, "calling action %u", action_detail.action);
		if (current_controls[action_detail.action]) (*current_controls[action_detail.action])();
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

/****************************************************************************************
 * 
 */
static actrls_action_e actrls_parse_action_json(const char * name){
	actrls_action_e action = ACTRLS_NONE;
	
	if(!strcasecmp("ACTRLS_NONE",name)) return ACTRLS_NONE;
	for(int i=0;i<ACTRLS_MAX && actrls_action_s[i][0]!='\0' ;i++){
		if(!strcmp(actrls_action_s[i], name)){
			return (actrls_action_e) i;
		}
	}
	// Action name wasn't recognized.
	// Check if this is a profile name that has a match in nvs
	ESP_LOGD(TAG,"unknown action %s, trying to find matching profile ", name);
	cJSON * existing = cJSON_GetObjectItem(control_profiles, name);

	if (!existing) {
		ESP_LOGD(TAG,"Loading new audio control profile with name: %s", name);
		if (actrls_init_json(name, false) == ESP_OK) {
			action = ACTRLS_REMAP;
		}
	} else {
		ESP_LOGD(TAG,"Existing profile %s was referenced", name);
		action = ACTRLS_REMAP;
	}

	return action;
}

/****************************************************************************************
 * 
 */
static esp_err_t actrls_process_int (const cJSON * member, actrls_config_t *cur_config,uint32_t offset){
	esp_err_t err = ESP_OK;
	ESP_LOGD(TAG,"Processing int member");
	int *value = (int*)((char*) cur_config + offset);
	*value = member->valueint;
	return err;
}

/****************************************************************************************
 * 
 */
static esp_err_t actrls_process_type (const cJSON * member, actrls_config_t *cur_config, uint32_t offset){
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

/****************************************************************************************
 * 
 */
static esp_err_t actrls_process_bool (const cJSON * member, actrls_config_t *cur_config, uint32_t offset){
	esp_err_t err = ESP_OK;
	if (!member) {
		ESP_LOGE(TAG,"Null json member pointer!");
		err = ESP_FAIL;
	} else {
		ESP_LOGD(TAG,"Processing bool member ");
		if (cJSON_IsBool(member)) {
			bool*value = (bool*)((char*) cur_config + offset);
			*value = cJSON_IsTrue(member);
		} else {
			ESP_LOGE(TAG,"Member %s is not a boolean", member->string?member->string:"unknown");
			err = ESP_FAIL;
		}
	}

	return err;
}

/****************************************************************************************
 * 
 */
static esp_err_t actrls_process_action (const cJSON * member, actrls_config_t *cur_config, uint32_t offset){
	esp_err_t err = ESP_OK;
	cJSON * button_action= cJSON_GetObjectItemCaseSensitive(member, "pressed");
	actrls_action_detail_t*value = (actrls_action_detail_t*)((char *)cur_config + offset);
	if (button_action != NULL) {
		value[0].action = actrls_parse_action_json( button_action->valuestring);
		if(value[0].action == ACTRLS_REMAP){
			value[0].name = strdup(button_action->valuestring);
		}
	} 
	button_action = cJSON_GetObjectItemCaseSensitive(member, "released");
	if (button_action != NULL) {
		value[1].action = actrls_parse_action_json( button_action->valuestring);
		if (value[1].action == ACTRLS_REMAP){
			value[1].name = strdup(button_action->valuestring);
		}
	}

	return err;
}

/****************************************************************************************
 * 
 */
static esp_err_t actrls_process_member(const cJSON * member, actrls_config_t *cur_config) {
	esp_err_t err = ESP_OK;
	const actrls_config_map_t * h=actrls_config_map;

	char * str = cJSON_Print(member);
	while (h->handler && strcmp(member->string, h->member)) { h++; }

	if (h->handler) {
		ESP_LOGD(TAG,"found handler for member %s, json value %s", h->member,str?str:"");
		err = h->handler(member, cur_config, h->offset);
	} else {
		err = ESP_FAIL;
		ESP_LOGE(TAG, "Unknown json structure member : %s", str?str:"");
	}

	if (str) free(str);
	return err;
}

/****************************************************************************************
 * 
 */
static esp_err_t actrls_process_button(const cJSON * button, actrls_config_t *cur_config) {
	esp_err_t err= ESP_OK;
	const cJSON *member;

	cJSON_ArrayForEach(member, button)
	{
		ESP_LOGD(TAG,"Processing member %s. ", member->string);
		esp_err_t loc_err = actrls_process_member(member, cur_config);
		err = (err == ESP_OK) ? loc_err : err;
	}
	return err;

}

/****************************************************************************************
 * 
 */
static actrls_config_t * actrls_init_alloc_structure(const cJSON *buttons, const char * name){
	int member_count = 0;
	const cJSON *button;
	actrls_config_t * json_config=NULL;

	// Check if the main profiles array was created
	if(!control_profiles){
		control_profiles = cJSON_CreateObject();
	}

	ESP_LOGD(TAG,"Counting the number of buttons definition");
	cJSON_ArrayForEach(button, buttons)	{
		member_count++;
	}

	ESP_LOGD(TAG, "config contains %u button definitions",	member_count);
	if (member_count != 0) {
		json_config = calloc(sizeof(actrls_config_t) * (member_count + 1), 1);
		if (json_config){
			json_config[member_count].gpio = -1;
		} else {	
			ESP_LOGE(TAG,"Unable to allocate memory to hold configuration for %u buttons ",member_count);
		}
	} else {
		ESP_LOGE(TAG,"No button found in configuration structure");
	}

	// we're cheating here; we're going to store the control profile
	// pointer as a string reference;  this will prevent cJSON
	// from trying to free the structure if we ever want to free the object
	cJSON * new_profile = cJSON_CreateStringReference((const char *)json_config);
	cJSON_AddItemToObject(control_profiles, name, new_profile);

	return json_config;
}

/****************************************************************************************
 * 
 */
static void actrls_defaults(actrls_config_t *config) {
	config->type = BUTTON_LOW;
	config->pull = false;
	config->debounce = 0;
	config->long_press = 0;
	config->shifter_gpio = -1;
	config->normal[0].action = config->normal[1].action = ACTRLS_NONE;
	config->longpress[0].action = config->longpress[1].action = ACTRLS_NONE;
	config->shifted[0].action = config->shifted[1].action = ACTRLS_NONE;
	config->longshifted[0].action = config->longshifted[1].action = ACTRLS_NONE;
}


/****************************************************************************************
 * 
 */
esp_err_t actrls_init_json(const char *profile_name, bool create) {
	esp_err_t err = ESP_OK;
	actrls_config_t *cur_config = NULL;
	actrls_config_t *config_root = NULL;
	const cJSON *button;

	char *config = config_alloc_get_default(NVS_TYPE_STR, profile_name, NULL, 0);
	if(!config) return ESP_FAIL;

	ESP_LOGD(TAG,"Parsing JSON structure %s", config);
	cJSON *buttons = cJSON_Parse(config);
	if (buttons == NULL) {
		ESP_LOGE(TAG,"JSON Parsing failed for %s", config);
		err = ESP_FAIL;
	} else {
		ESP_LOGD(TAG,"Json parsing completed");
		if (cJSON_IsArray(buttons)) {
			ESP_LOGD(TAG,"configuration is an array as expected");
			cur_config =config_root= actrls_init_alloc_structure(buttons, profile_name);
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
				actrls_defaults(cur_config);
				esp_err_t loc_err = actrls_process_button(button, cur_config);
				err = (err == ESP_OK) ? loc_err : err;
				if (loc_err == ESP_OK) {
					if (create) button_create((void*) cur_config, cur_config->gpio,cur_config->type, cur_config->pull,cur_config->debounce,
									control_handler, cur_config->long_press, cur_config->shifter_gpio);
				} else {
					ESP_LOGE(TAG,"Error parsing button structure.  Button will not be registered.");
				}

				cur_config++;
			}
		} else {
			ESP_LOGE(TAG,"Invalid configuration; array is expected and none received in %s ", config);
		}
		cJSON_Delete(buttons);
	}
	// Now update the global json_config object.  If we are recursively processing menu structures,
	// the last init that completes will assigh the first json config object found, which will match
	// the default config from nvs.
	json_config = config_root;
	return err;
}

/****************************************************************************************
 *
 */
void actrls_set_default(const actrls_t controls, actrls_hook_t *hook) {
	memcpy(default_controls, controls, sizeof(actrls_t));
	memcpy(current_controls, default_controls, sizeof(actrls_t));
	default_hook = current_hook = hook;
}

/****************************************************************************************
 * 
 */
void actrls_set(const actrls_t controls, actrls_hook_t *hook) {
	memcpy(current_controls, controls, sizeof(actrls_t));
	current_hook = hook;
}

/****************************************************************************************
 * 
 */
void actrls_unset(void) {
	memcpy(current_controls, default_controls, sizeof(actrls_t));
	current_hook = default_hook;
}
