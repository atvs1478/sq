/* 
 *  Squeezelite for esp32
 *
 *  (c) Philippe G. 2020, philippe_44@outlook.com
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

void equalizer_open(u32_t sample_rate);
void equalizer_close(void);
void equalizer_update(s8_t *gain);
void equalizer_process(u8_t *buf, u32_t bytes, u32_t sample_rate);
