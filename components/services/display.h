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

#define DISPLAY_CLEAR 	0x01
#define DISPLAY_UPDATE	0x02

enum display_pos_e { DISPLAY_TOP_LEFT, DISPLAY_MIDDLE_LEFT, DISPLAY_BOTTOM_LEFT, DISPLAY_CENTER };

extern struct display_s {
	bool (*init)(char *config, char *welcome);
	void (*on)(bool state);
	void (*brightness)(u8_t level);
	void (*text)(enum display_pos_e pos, int attribute, char *msg);
	void (*update)(void);
	void (*v_draw)(u8_t *data);
} *display;