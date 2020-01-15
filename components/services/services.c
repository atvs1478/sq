/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include <driver/i2c.h>
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

/****************************************************************************************
 * 
 */
void services_init(void) {
	int scl = -1, sda = -1, i2c_speed = 250000;
	char *nvs_item, *p;
	
	gpio_install_isr_service(0);
	
	nvs_item = config_alloc_get(NVS_TYPE_STR, "i2c_config");
	
	if (nvs_item) {
		if ((p = strcasestr(nvs_item, "scl")) != NULL) scl = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(nvs_item, "sda")) != NULL) sda = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(nvs_item, "speed")) != NULL) i2c_speed = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(nvs_item, "port")) != NULL) i2c_system_port = atoi(strchr(p, '=') + 1);
	}
	
#ifdef CONFIG_SQUEEZEAMP
	if (i2c_system_port == 0) {
		i2c_system_port = 1;
		ESP_LOGE(TAG, "can't use i2c port 0 on SqueezeAMP");
	}
#endif

	ESP_LOGI(TAG,"Configuring I2C sda:%d scl:%d port:%u speed:%u", sda, scl, i2c_system_port, i2c_speed);

	if (sda != -1 && scl != -1) {
		i2c_config_t i2c = { 0 };
		
		i2c.mode = I2C_MODE_MASTER;
		i2c.sda_io_num = sda;
		i2c.sda_pullup_en = GPIO_PULLUP_ENABLE;
		i2c.scl_io_num = scl;
		i2c.scl_pullup_en = GPIO_PULLUP_ENABLE;
		i2c.master.clk_speed = i2c_speed;
		
		i2c_param_config(i2c_system_port, &i2c);
		i2c_driver_install(i2c_system_port, i2c.mode, 0, 0, 0 );
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
