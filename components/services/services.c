/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "services.h"
#include "config.h"
#include "battery.h"
#include "led.h"
#include "monitor.h"
#include "globdefs.h"

extern void battery_svc_init(void);
extern void monitor_svc_init(void);
extern void led_svc_init(void);

int i2c_system_port = I2C_SYSTEM_PORT;

static const char *TAG = "services";

esp_err_t services_store_i2c_config(const i2c_config_t * config, int i2c_system_port){
	int buffer_size=255;
	char * config_buffer=malloc(buffer_size);
	memset(config_buffer,0x00,buffer_size);
	if(config_buffer)  {
		snprintf(config_buffer,buffer_size,"scl=%u sda=%u speed=%u port=%u",config->scl_io_num,config->sda_io_num,config->master.clk_speed,i2c_system_port);
		ESP_LOGI(TAG,"Updating i2c configuration to %s",config_buffer);
		config_set_value(NVS_TYPE_STR, "i2c_config", config_buffer);
		free(config_buffer);
	}
	return ESP_OK;

}
const i2c_config_t * services_get_i2c_config(int * i2c_port) {
	char *nvs_item, *p;
	static i2c_config_t i2c = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = -1,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_io_num = -1,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 400000
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
void services_init(void) {

	gpio_install_isr_service(0);
	const i2c_config_t * i2c_config = services_get_i2c_config(&i2c_system_port);
	
#ifdef CONFIG_SQUEEZEAMP
	if (i2c_system_port == 0) {
		i2c_system_port = 1;
		ESP_LOGE(TAG, "can't use i2c port 0 on SqueezeAMP");
	}
#endif

	ESP_LOGI(TAG,"Configuring I2C sda:%d scl:%d port:%u speed:%u", i2c_config->sda_io_num,i2c_config->scl_io_num, i2c_system_port, i2c_config->master.clk_speed);

	if (i2c_config->sda_io_num != -1 && i2c_config->scl_io_num != -1) {
		i2c_param_config(i2c_system_port, i2c_config);
		i2c_driver_install(i2c_system_port, i2c_config->mode, 0, 0, 0 );
	} else {
		ESP_LOGE(TAG, "can't initialize I2C");
	}

	ESP_LOGD(TAG,"Configuring LEDs");
	led_svc_init();
	led_config(LED_GREEN, LED_GREEN_GPIO, 0);
	led_config(LED_RED, LED_RED_GPIO, 0);

	battery_svc_init();
	monitor_svc_init();
}
