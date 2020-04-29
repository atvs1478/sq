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

#include "gds.h"

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
extern struct GDS_Device *display;
  
enum displayer_cmd_e 	{ DISPLAYER_SHUTDOWN, DISPLAYER_ACTIVATE, DISPLAYER_SUSPEND, DISPLAYER_TIMER_PAUSE, DISPLAYER_TIMER_RUN };
enum displayer_time_e 	{ DISPLAYER_ELAPSED, DISPLAYER_REMAINING };

enum display_bus_cmd_e { DISPLAY_BUS_TAKE, DISPLAY_BUS_GIVE };
bool (*display_bus)(void *from, enum display_bus_cmd_e cmd);
char * display_conf_get_driver_name(char * driver);
bool display_is_valid_driver(char * driver);

void displayer_scroll(char *string, int speed, int pause);
void displayer_control(enum displayer_cmd_e cmd, ...);
void displayer_metadata(char *artist, char *album, char *title);
void displayer_timer(enum displayer_time_e mode, int elapsed, int duration);
