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
typedef enum { BUTTON_NORMAL, BUTTON_LONG, BUTTON_SHIFTED } button_press_e; 
typedef void (*button_handler)(button_event_e event, button_press_e mode);

/* 
a button might have variable functions
	- "long", <long_handler>, <duration_in_ms>
	- "shift", <shift_handler>, <shifter_gpio>

button_create(2, BUTTON_LOW, true, handler, NULL);
button_create(5, BUTTON_HIGH, true, handler, "long", long_handler, 2000, NULL);
button_create(6, BUTTON_LOW, true, handler, "shift", shift_handler, 5, NULL);
button_create(6, BUTTON_HIGH, true, handler, "long", long_handler, 2000, "shift", shift_handler, 5);

NOTE: shifter buttons *must* be created before shiftee
*/

void button_create(int gpio, int type, bool pull, button_handler handler, ...);