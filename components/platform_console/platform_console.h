/* Console example — declarations of command registration functions.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include "esp_console.h"
#include "cJSON.h"
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t cmd_to_json(const esp_console_cmd_t *cmd);
cJSON * get_cmd_list();
#ifdef __cplusplus
}
#endif
