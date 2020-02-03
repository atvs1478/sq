/* 
 *  Squeezelite for esp32
 *
 *  (c) Sebastien 2019
 *      Philippe G. 2019, philippe_44@outlook.com
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
 
#include "adac.h"

static bool init(int i2c_port_num, int i2s_num, i2s_config_t *config) { return true; };
static void deinit(void) { };
static void speaker(bool active) { };
static void headset(bool active) { } ;
static void volume(unsigned left, unsigned right) { };
static void power(adac_power_e mode) { };

struct adac_s dac_null = { init, deinit, power, speaker, headset, volume };

