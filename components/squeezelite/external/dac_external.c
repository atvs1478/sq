/* 
 *  Squeezelite for esp32
 *
 *  (c) Sebastien 2019
 *      Philippe G. 2019, philippe_44@outlook.com
 *
 *  This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 *
 */
 
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2s.h>
#include "esp_log.h"
#include "platform_config.h"
#include "adac.h"

static bool init(int i2c_port_num, int i2s_num, i2s_config_t *config);
static void deinit(void) { };
static void speaker(bool active) { };
static void headset(bool active) { } ;
static void volume(unsigned left, unsigned right) { };
static void power(adac_power_e mode) { };

struct adac_s dac_external = { init, deinit, power, speaker, headset, volume };

static char TAG[] = "DAC external";

static bool init(int i2c_port_num, int i2s_num, i2s_config_t *config) { 
	i2s_pin_config_t i2s_pin_config = (i2s_pin_config_t) { 	.bck_io_num = CONFIG_I2S_BCK_IO, .ws_io_num = CONFIG_I2S_WS_IO, 
															.data_out_num = CONFIG_I2S_DO_IO, .data_in_num = CONFIG_I2S_DI_IO };
	char *nvs_item = config_alloc_get(NVS_TYPE_STR, "dac_config");
	
	if (nvs_item) {
		char *p;
		if ((p = strcasestr(nvs_item, "bck")) != NULL) i2s_pin_config.bck_io_num = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(nvs_item, "ws")) != NULL) i2s_pin_config.ws_io_num = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(nvs_item, "do")) != NULL) i2s_pin_config.data_out_num = atoi(strchr(p, '=') + 1);
		free(nvs_item);
	} 
	
	if (i2s_pin_config.bck_io_num != -1 && i2s_pin_config.ws_io_num != -1 && i2s_pin_config.data_out_num != -1) {
		i2s_driver_install(i2s_num, config, 0, NULL);
		i2s_set_pin(i2s_num, &i2s_pin_config);

		ESP_LOGI(TAG, "External DAC using I2S bck:%u, ws:%u, do:%u", i2s_pin_config.bck_io_num, i2s_pin_config.ws_io_num, i2s_pin_config.data_out_num);

		return true;
	} else {
		ESP_LOGI(TAG, "Cannot initialize I2S for DAC bck:%d ws:%d do:%d", i2s_pin_config.bck_io_num, 
																		   i2s_pin_config.ws_io_num, 
																		   i2s_pin_config.data_out_num);
		return false;
	}
}

