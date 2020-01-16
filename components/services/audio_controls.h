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

// BEWARE: this is the index of the array of action below (change actrls_action_s as well!)
typedef enum { 	ACTRLS_NONE = -1, ACTRLS_VOLUP, ACTRLS_VOLDOWN, ACTRLS_TOGGLE, ACTRLS_PLAY, 
				ACTRLS_PAUSE, ACTRLS_STOP, ACTRLS_REW, ACTRLS_FWD, ACTRLS_PREV, ACTRLS_NEXT, 
				BCTRLS_PUSH, BCTRLS_UP, BCTRLS_DOWN, BCTRLS_LEFT, BCTRLS_RIGHT,
				ACTRLS_MAX 
		} actrls_action_e;

typedef void (*actrls_handler)(void);
typedef actrls_handler actrls_t[ACTRLS_MAX - ACTRLS_NONE - 1];

// BEWARE any change to struct below must be mapped to actrls_config_map
typedef struct actrl_config_s {
	int gpio;
	int type;
	bool pull;
	int	debounce;
	int long_press;
	int shifter_gpio;
	union {
		actrls_action_e action;
		struct actrl_config_s *config;	
	} normal[2], longpress[2], shifted[2], longshifted[2];	// [0] keypressed, [1] keyreleased
} actrls_config_t;

esp_err_t actrls_init(int n, const actrls_config_t *config);
esp_err_t actrls_init_json(const char * config);
void actrls_set_default(const actrls_t controls);
void actrls_set(const actrls_t controls);
void actrls_unset(void);
