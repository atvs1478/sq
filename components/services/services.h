/*
 *  audio control callbacks
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

#ifndef COMPONENTS_SERVICES_SERVICES_H_
#define COMPONENTS_SERVICES_SERVICES_H_
#include <driver/i2c.h>
#include "esp_system.h"

const i2c_config_t * services_get_i2c_config(int * i2c_port);
esp_err_t services_store_i2c_config(const i2c_config_t * config, int i2c_system_port);
void services_init(void);
void display_init(char *welcome);



#endif /* COMPONENTS_SERVICES_SERVICES_H_ */
