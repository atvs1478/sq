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

static void deinit(void) { }
static void speaker(bool active) { }
static void headset(bool active) { } 
static bool volume(unsigned left, unsigned right) { return false; }
static void power(adac_power_e mode);
static bool init(char *config, int i2c_port_num, i2s_config_t *i2s_config);

static bool i2c_json_execute(char *set);
static esp_err_t i2c_write_reg(uint8_t reg, uint8_t val);
static uint8_t i2c_read_reg(uint8_t reg);

const struct adac_s dac_external = { "i2s", init, deinit, power, speaker, headset, volume };
static int i2c_port, i2c_addr;
static cJSON *i2c_json;

/****************************************************************************************
 * init
 */
static bool init(char *config, int i2c_port_num, i2s_config_t *i2s_config) {	 
	char *p;	
	i2c_port = i2c_port_num;
	
	// configure i2c
	i2c_config_t i2c_config = {
			.mode = I2C_MODE_MASTER,
			.sda_io_num = -1,
			.sda_pullup_en = GPIO_PULLUP_ENABLE,
			.scl_io_num = -1,
			.scl_pullup_en = GPIO_PULLUP_ENABLE,
			.master.clk_speed = 250000,
		};

	if ((p = strcasestr(config, "i2c")) != NULL) i2c_addr = atoi(strchr(p, '=') + 1);
	if ((p = strcasestr(config, "sda")) != NULL) i2c_config.sda_io_num = atoi(strchr(p, '=') + 1);
	if ((p = strcasestr(config, "scl")) != NULL) i2c_config.scl_io_num = atoi(strchr(p, '=') + 1);

	p = config_alloc_get_str("dac_controlset", CONFIG_DAC_CONTROLSET, NULL);
	i2c_json = cJSON_Parse(p);
	
	if (!i2c_addr || !i2c_json || i2c_config.sda_io_num == -1 || i2c_config.scl_io_num == -1) {
		if (p) free(p);
		ESP_LOGW(TAG, "No i2c controlset found");
		return true;
	}	
	
	ESP_LOGI(TAG, "DAC uses I2C @%d with sda:%d, scl:%d", i2c_addr, i2c_config.sda_io_num, i2c_config.scl_io_num);
	
	// we have an I2C configured	
	i2c_param_config(i2c_port, &i2c_config);
	i2c_driver_install(i2c_port, I2C_MODE_MASTER, false, false, false);
		
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
			i2c_write_reg(reg->valueint, val->valueint);
		} else if (!strcasecmp(mode->valuestring, "or")) {
			uint8_t data = i2c_read_reg(reg->valueint);
			data |= (uint8_t) val->valueint;
			i2c_write_reg(reg->valueint, data);
		} else if (!strcasecmp(mode->valuestring, "and")) {
			uint8_t data = i2c_read_reg(reg->valueint);
			data &= (uint8_t) val->valueint;
			i2c_write_reg(reg->valueint, data);
        }
	}
	
	return true;
}	

/****************************************************************************************
 * 
 */
static esp_err_t i2c_write_reg(uint8_t reg, uint8_t val) {
	esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
	
	i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, I2C_MASTER_NACK);
	i2c_master_write_byte(cmd, reg, I2C_MASTER_NACK);
	i2c_master_write_byte(cmd, val, I2C_MASTER_NACK);
	
	i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_port, cmd, 100 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
	
	if (ret != ESP_OK) {
		ESP_LOGW(TAG, "I2C write failed");
	}
	
    return ret;
}

/****************************************************************************************
 * 
 */
static uint8_t i2c_read_reg(uint8_t reg) {
	esp_err_t ret;
	uint8_t data = 0;
	
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    
	i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, I2C_MASTER_NACK);
	i2c_master_write_byte(cmd, reg, I2C_MASTER_NACK);

	i2c_master_start(cmd);			
	i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_READ, I2C_MASTER_NACK);
	i2c_master_read_byte(cmd, &data, I2C_MASTER_NACK);
	
    i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(i2c_port, cmd, 100 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	
	if (ret != ESP_OK) {
		ESP_LOGW(TAG, "I2C read failed");
	}
	
	return data;
}


