/* 
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
 
#pragma once

#include "buttons.h"

// BEWARE: this is the index of the array of action below
typedef enum { 	ACTRLS_NONE = -1, ACTRLS_VOLUP, ACTRLS_VOLDOWN, ACTRLS_TOGGLE, ACTRLS_PLAY, 
				ACTRLS_PAUSE, ACTRLS_STOP, ACTRLS_REW, ACTRLS_FWD, ACTRLS_PREV, ACTRLS_NEXT, 
				BCTRLS_PUSH, BCTRLS_UP, BCTRLS_DOWN, BCTRLS_LEFT, BCTRLS_RIGHT,
				ACTRLS_MAX 
		} actrls_action_e;
typedef enum {
	ACTRLS_MAP_INT, ACTRLS_MAP_BOOL, ACTRLS_MAP_ACTION,ACTRLS_MAP_TYPE,ACTRLS_MAP_END
} actrls_action_map_element_type_e;



typedef void (*actrls_handler)(void);
typedef actrls_handler actrls_t[ACTRLS_MAX - ACTRLS_NONE - 1];
typedef struct {
	int gpio;
	int type;
	bool pull;
	int	debounce;
	int long_press;
	int shifter_gpio;
	actrls_action_e normal[2], longpress[2], shifted[2], longshifted[2];	// [0] keypressed, [1] keyreleased
} actrls_config_t;



typedef struct {
	char * member;
	uint32_t offset;
	actrls_action_map_element_type_e type;
} actrls_config_map_t;

static const actrls_config_map_t actrls_config_map[] =
		{
			{"gpio", offsetof(actrls_config_t,gpio), ACTRLS_MAP_INT},
			{"debounce", offsetof(actrls_config_t,debounce), ACTRLS_MAP_INT},
			{"type", offsetof(actrls_config_t,type),ACTRLS_MAP_TYPE},
			{"pull", offsetof(actrls_config_t,pull),ACTRLS_MAP_BOOL},
			{"long_press", offsetof(actrls_config_t,long_press),ACTRLS_MAP_INT},
			{"shifter_gpio", offsetof(actrls_config_t,shifter_gpio),ACTRLS_MAP_INT},
			{"normal", offsetof(actrls_config_t,normal), ACTRLS_MAP_ACTION},
			{"longpress", offsetof(actrls_config_t,longpress), ACTRLS_MAP_ACTION},
			{"longshifted", offsetof(actrls_config_t,longshifted), ACTRLS_MAP_ACTION},
			{"", 0,ACTRLS_MAP_END}
		};


esp_err_t actrls_init(int n, const actrls_config_t *config);
esp_err_t actrls_init_json(const char * config);
void actrls_set_default(const actrls_t controls);
void actrls_set(const actrls_t controls);
void actrls_unset(void);
