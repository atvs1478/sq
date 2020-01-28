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

#define DISPLAY_CLEAR 		0x01
#define	DISPLAY_ONLY_EOL	0x02
#define DISPLAY_UPDATE		0x04
#define DISPLAY_MONOSPACE	0x08

// these ones are for 'x' parameter of line() function
#define DISPLAY_LEFT 	0
#define DISPLAY_RIGHT	0xff00
#define	DISPLAY_CENTER  0xff01

enum display_pos_e 	{ DISPLAY_TOP_LEFT, DISPLAY_MIDDLE_LEFT, DISPLAY_BOTTOM_LEFT, DISPLAY_CENTERED };
enum display_font_e { DISPLAY_FONT_DEFAULT, 
					  DISPLAY_FONT_LINE_1, DISPLAY_FONT_LINE_2, DISPLAY_FONT_SEGMENT, 
					  DISPLAY_FONT_TINY, DISPLAY_FONT_SMALL,  DISPLAY_FONT_MEDIUM, DISPLAY_FONT_LARGE, DISPLAY_FONT_HUGE };
					  
enum displayer_cmd_e 	{ DISPLAYER_SHUTDOWN, DISPLAYER_ACTIVATE, DISPLAYER_DISABLE, DISPLAYER_TIMER_PAUSE, DISPLAYER_TIMER_RESUME };
enum displayer_time_e 	{ DISPLAYER_ELAPSED, DISPLAYER_REMAINING };

// don't change anything there w/o changing all drivers init code
extern struct display_s {
	int width, height;
	bool (*init)(char *config, char *welcome);
	void (*clear)(void);
	bool (*set_font)(int num, enum display_font_e font, int space);
	void (*on)(bool state);
	void (*brightness)(uint8_t level);
	void (*text)(enum display_font_e font, enum display_pos_e pos, int attribute, char *msg, ...);
	bool (*line)(int num, int x, int attribute, char *text);
	int (*stretch)(int num, char *string, int max);
	void (*update)(void);
	void (*draw)(int x1, int y1, int x2, int y2, bool by_column, uint8_t *data);
	void (*draw_cbr)(uint8_t *data, int height);		// height is the # of columns in data, as oppoosed to display height (0 = display height) 
} *display;

void displayer_scroll(char *string, int speed);
void displayer_control(enum displayer_cmd_e cmd, ...);
void displayer_metadata(char *artist, char *album, char *title);
void displayer_timer(enum displayer_time_e mode, int elapsed, int duration);