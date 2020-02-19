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

/* 
 The displayer is not thread-safe and the caller must ensure use its own 
 mutexes if it wants something better. Especially, text() line() and draw()
 are not protected against each other.
 In text mode (text/line) when using DISPLAY_SUSPEND, the displayer will 
 refreshed line 2 one last time before suspending itself. As a result if it
 is in a long sleep (scrolling pause), the refresh will happen after wakeup. 
 So it can conflict with other display direct writes that have been made during
 sleep. Note that if DISPLAY_SHUTDOWN has been called meanwhile, it (almost) 
 never happens
 The display_bus() shall be subscribed by other displayers so that at least
 when this one (the main) wants to take control over display, it can signal
 that to others
*/ 
  
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
					  
enum displayer_cmd_e 	{ DISPLAYER_SHUTDOWN, DISPLAYER_ACTIVATE, DISPLAYER_SUSPEND, DISPLAYER_TIMER_PAUSE, DISPLAYER_TIMER_RUN };
enum displayer_time_e 	{ DISPLAYER_ELAPSED, DISPLAYER_REMAINING };

// don't change anything there w/o changing all drivers init code
extern struct display_s {
	int width, height;
	bool dirty;
	bool (*init)(char *config, char *welcome);
	void (*clear)(bool full, ...);
	bool (*set_font)(int num, enum display_font_e font, int space);
	void (*on)(bool state);
	void (*brightness)(uint8_t level);
	void (*text)(enum display_font_e font, enum display_pos_e pos, int attribute, char *msg, ...);
	bool (*line)(int num, int x, int attribute, char *text);
	int (*stretch)(int num, char *string, int max);
	void (*update)(void);
	void (*draw_raw)(int x1, int y1, int x2, int y2, bool by_column, bool MSb, uint8_t *data);
	void (*draw_cbr)(uint8_t *data, int width, int height);		// width and height is the # of rows/columns in data, as opposed to display height (0 = display width, 0 = display height) 
	void (*draw_line)(int x1, int y1, int x2, int y2);
	void (*draw_box)( int x1, int y1, int x2, int y2, bool fill);
} *display;

enum display_bus_cmd_e { DISPLAY_BUS_TAKE, DISPLAY_BUS_GIVE };
bool (*display_bus)(void *from, enum display_bus_cmd_e cmd);

void displayer_scroll(char *string, int speed);
void displayer_control(enum displayer_cmd_e cmd, ...);
void displayer_metadata(char *artist, char *album, char *title);
void displayer_timer(enum displayer_time_e mode, int elapsed, int duration);
