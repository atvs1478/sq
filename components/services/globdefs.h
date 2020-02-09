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
#define SPI_SYSTEM_HOST	SPI2_HOST

extern int i2c_system_port;
extern int spi_system_host;
extern int spi_system_dc_gpio;
extern bool gpio36_39_used;

#ifdef CONFIG_SQUEEZEAMP
#define SPKFAULT_GPIO			2			// this requires a pull-up, so can't be >34
#define ADAC dac_tas57xx
#elif defined(CONFIG_A1S)
#define ADAC dac_a1s
#else
#define ADAC dac_external
#endif
