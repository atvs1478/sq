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
 
// button type (pressed = LOW or HIGH)
#define BUTTON_LOW 		0
#define BUTTON_HIGH		1

typedef enum { BUTTON_PRESSED, BUTTON_RELEASED } button_event_e; 
typedef enum { BUTTON_NORMAL, BUTTON_SHIFTED } button_press_e; 
typedef void (*button_handler)(void *id, button_event_e event, button_press_e mode, bool long_press);

/* 
set long_press to 0 for no long-press
set shifter_gpio to -1 for no shift
NOTE: shifter buttons *must* be created before shiftee
*/

void button_create(void *id, int gpio, int type, bool pull, button_handler handler, int long_press, int shifter_gpio);