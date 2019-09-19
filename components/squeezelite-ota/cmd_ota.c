/* Console example — various system commands

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "../squeezelite-ota/cmd_ota.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_spi_flash.h"
#include "driver/rtc_io.h"
#include "driver/uart.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/rtc_cntl_reg.h"
#include "esp32/rom/uart.h"
#include "sdkconfig.h"

static const char * TAG = "platform_esp32";

/* 'heap' command prints minumum heap size */
static int perform_ota_update(int argc, char **argv)
{

    return 0;
}

static void register_ota_cmd()
{
    const esp_console_cmd_t cmd = {
        .command = "ota_update",
        .help = "Updates the application binary from the provided URL",
        .hint = NULL,
        .func = &perform_ota_update,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}



