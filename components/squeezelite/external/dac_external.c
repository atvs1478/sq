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
#include "driver/i2c.h"
#include "esp_log.h"
#include "cJSON.h"
#include "platform_config.h"
#include "adac.h"

static const char TAG[] = "DAC external";

static void speaker(bool active) { }
static void headset(bool active) { } 
static bool volume(unsigned left, unsigned right) { return false; }
static void power(adac_power_e mode);
static bool init(char *config, int i2c_port_num, i2s_config_t *i2s_config);

static bool i2c_json_execute(char *set);

const struct adac_s dac_external = { "i2s", init, adac_deinit, power, speaker, headset, volume };
static cJSON *i2c_json;
static int i2c_addr;

/****************************************************************************************
 * init
 */
static bool init(char *config, int i2c_port_num, i2s_config_t *i2s_config) {	 
	char *p;	
	
	i2c_addr = adac_init(config, i2c_port_num);
	if (!i2c_addr) return false;
	
	ESP_LOGI(TAG, "DAC on I2C @%d", i2c_addr);
	
	p = config_alloc_get_str("dac_controlset", CONFIG_DAC_CONTROLSET, NULL);
	i2c_json = cJSON_Parse(p);
	
	if (!i2c_json) {
		if (p) free(p);
		ESP_LOGW(TAG, "no i2c controlset found");
		return true;
	}	
		
	if (!i2c_json_execute("init")) {	
		ESP_LOGE(TAG, "could not intialize DAC");
		return false;
	}	
	
	return true;
}	

/****************************************************************************************
 * power
 */
static void power(adac_power_e mode) {
	if (mode == ADAC_STANDBY || mode == ADAC_OFF) i2c_json_execute("poweroff");
	else i2c_json_execute("poweron");
}

/****************************************************************************************
 * 
 */
bool i2c_json_execute(char *set) {
	cJSON *json_set = cJSON_GetObjectItemCaseSensitive(i2c_json, set);
	cJSON *item;

	if (!json_set) return true;
	
	cJSON_ArrayForEach(item, json_set)
	{
		cJSON *reg = cJSON_GetObjectItemCaseSensitive(item, "reg");
		cJSON *val = cJSON_GetObjectItemCaseSensitive(item, "val");
		cJSON *mode = cJSON_GetObjectItemCaseSensitive(item, "mode");

		if (!reg || !val) continue;

		if (!mode) {
			adac_write_byte(i2c_addr, reg->valueint, val->valueint);
		} else if (!strcasecmp(mode->valuestring, "or")) {
			uint8_t data = adac_read_byte(i2c_addr,reg->valueint);
			data |= (uint8_t) val->valueint;
			adac_write_byte(i2c_addr, reg->valueint, data);
		} else if (!strcasecmp(mode->valuestring, "and")) {
			uint8_t data = adac_read_byte(i2c_addr, reg->valueint);
			data &= (uint8_t) val->valueint;
			adac_write_byte(i2c_addr, reg->valueint, data);
        }
	}
	
	return true;
}	
