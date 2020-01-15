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

struct display_handle_s {
	bool (*init)(char *config, char* welcome);
	void (*print_message)(char *msg);
	void (*vfdc_handler)(u8_t *data, int len);
	void (*grfe_handler)(u8_t *data, int len);
	void (*grfb_handler)(u8_t *data, int len);
	void (*visu_handler)(u8_t *data, int len);
}; 

extern struct display_handle_s SSD1306_handle;



