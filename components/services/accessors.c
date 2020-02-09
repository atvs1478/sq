/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "config.h"
#include "accessors.h"
#include "globdefs.h"

static const char *TAG = "services";

#define min(a,b) (((a) < (b)) ? (a) : (b))

/****************************************************************************************
 * 
 */
esp_err_t config_i2c_set(const i2c_config_t * config, int port){
	int buffer_size=255;
	char * config_buffer=calloc(buffer_size,1);
	if(config_buffer)  {
		snprintf(config_buffer,buffer_size,"scl=%u sda=%u speed=%u port=%u",config->scl_io_num,config->sda_io_num,config->master.clk_speed,port);
		ESP_LOGI(TAG,"Updating i2c configuration to %s",config_buffer);
		config_set_value(NVS_TYPE_STR, "i2c_config", config_buffer);
		free(config_buffer);
	}
	return ESP_OK;
}

/****************************************************************************************
 * 
 */
const i2c_config_t * config_i2c_get(int * i2c_port) {
	char *nvs_item, *p;
	static i2c_config_t i2c = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = -1,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_io_num = -1,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 400000,
	};

	nvs_item = config_alloc_get(NVS_TYPE_STR, "i2c_config");
	if (nvs_item) {
		if ((p = strcasestr(nvs_item, "scl")) != NULL) i2c.scl_io_num = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(nvs_item, "sda")) != NULL) i2c.sda_io_num = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(nvs_item, "speed")) != NULL) i2c.master.clk_speed = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(nvs_item, "port")) != NULL) i2c_system_port = atoi(strchr(p, '=') + 1);
		free(nvs_item);
	}
	if(i2c_port) *i2c_port=i2c_system_port;
	return &i2c;
}

/****************************************************************************************
 * 
 */
const spi_bus_config_t * config_spi_get(spi_host_device_t * spi_host) {
	char *nvs_item, *p;
	static spi_bus_config_t spi = {
		.mosi_io_num = -1,
        .sclk_io_num = -1,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1
    };

	nvs_item = config_alloc_get(NVS_TYPE_STR, "spi_config");
	if (nvs_item) {
		if ((p = strcasestr(nvs_item, "data")) != NULL) spi.mosi_io_num = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(nvs_item, "clk")) != NULL) spi.sclk_io_num = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(nvs_item, "d/c")) != NULL) spi_system_dc_gpio = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(nvs_item, "host")) != NULL) spi_system_host = atoi(strchr(p, '=') + 1);
		free(nvs_item);
	}
	if(spi_host) *spi_host = spi_system_host;
	return &spi;
}

/****************************************************************************************
 * 
 */
void parse_set_GPIO(void (*cb)(int gpio, char *value)) {
	char *nvs_item, *p, type[4];
	int gpio;
	
	if ((nvs_item = config_alloc_get(NVS_TYPE_STR, "set_GPIO")) == NULL) return;
	
	p = nvs_item;
	
	do {
		if (sscanf(p, "%d=%3[^,]", &gpio, type) > 0) cb(gpio, type);
		p = strchr(p, ',');
	} while (p++);
	
	free(nvs_item);
}	