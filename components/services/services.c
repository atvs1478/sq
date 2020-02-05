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
#include "accessors.h"

extern void battery_svc_init(void);
extern void monitor_svc_init(void);
extern void led_svc_init(void);

int i2c_system_port = I2C_SYSTEM_PORT;

static const char *TAG = "services";

/****************************************************************************************
 * 
 */
void set_power_gpio(int gpio, char *value) {
	bool parsed = true;
	
	if (!strcasecmp(value, "vcc") ) {
		gpio_pad_select_gpio(gpio);
		gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
		gpio_set_level(gpio, 1);
	} else if (!strcasecmp(value, "gnd")) {
		gpio_pad_select_gpio(gpio);
		gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
		gpio_set_level(gpio, 0);
	} else parsed = false	;
	
	if (parsed) ESP_LOGI(TAG, "set GPIO %u to %s", gpio, value);
 }	

/****************************************************************************************
 * 
 */
void services_init(void) {
	char *nvs_item;
	
	gpio_install_isr_service(0);
	
#ifdef CONFIG_SQUEEZEAMP
	if (i2c_system_port == 0) {
		i2c_system_port = 1;
		ESP_LOGE(TAG, "can't use i2c port 0 on SqueezeAMP");
	}
#endif

	// set potential power GPIO
	parse_set_GPIO(set_power_gpio);

	const i2c_config_t * i2c_config = config_i2c_get(&i2c_system_port);

	ESP_LOGI(TAG,"Configuring I2C sda:%d scl:%d port:%u speed:%u", i2c_config->sda_io_num, i2c_config->scl_io_num, i2c_system_port, i2c_config->master.clk_speed);

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