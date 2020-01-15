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

#define I2C_SYSTEM_PORT	1
extern int i2c_system_port;

#ifdef CONFIG_SQUEEZEAMP
#define JACK_GPIO		34
#define SPKFAULT_GPIO	2			// this requires a pull-up, so can't be >34
#define LED_GREEN_GPIO 	12
#define LED_RED_GPIO	13
#endif



