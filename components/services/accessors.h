/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include "esp_system.h"
#include "driver/i2c.h"

esp_err_t 				config_i2c_set(const i2c_config_t * config, int port);
const i2c_config_t * 	config_i2c_get(int * i2c_port);
void 					parse_set_GPIO(void (*cb)(int gpio, char *value));
