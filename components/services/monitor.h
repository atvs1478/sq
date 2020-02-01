/* 
 *  Squeezelite for esp32
 *
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

extern void (*jack_handler_svc)(bool inserted);
extern bool jack_inserted_svc(void);

extern void (*spkfault_handler_svc)(bool inserted);
extern bool spkfault_svc(void);

extern int battery_value_svc(void);

