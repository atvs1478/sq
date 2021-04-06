/* 
 *  Squeezelite for esp32
 *
 *  (c) Wizmo 2021
 * 		Sebastien 2019
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

static const char TAG[] = "WM8978";

static void deinit(void) { }
static void speaker(bool active);
static void headset(bool active);
static bool volume(unsigned left, unsigned right) { return false; }
static void power(adac_power_e mode);
static bool init(char *config, int i2c_port_num, i2s_config_t *i2s_config);

static bool i2c_json_execute(char *set);
static esp_err_t i2c_write_reg(uint8_t reg, uint16_t val);
static uint16_t i2c_read_reg(uint8_t reg);

const struct adac_s dac_wm8978 = { "WM8978", init, deinit, power, speaker, headset, volume };
static int i2c_port, i2c_addr;
static cJSON *i2c_json;
// default dac_controlset
static char* default_controlset = "{\"init\": [ {\"reg\":0,\"val\":0},{\"reg\":4,\"val\":16},{\"reg\":6,\"val\":0},{\"reg\":10,\"val\":8},{\"reg\":43,\"val\":16},{\"reg\":49,\"val\":102} ], \"poweron\": [ {\"reg\":1,\"val\":11},{\"reg\":2,\"val\":384},{\"reg\":3,\"val\":111} ], \"poweroff\": [ {\"reg\":1,\"val\":0},{\"reg\":2,\"val\":0},{\"reg\":3,\"val\":0} ]}";
/* Note: Device supports headset and speaker volume/mute, but jack
//   insertion is conrolled internaly and not connected gpio on t-audio to monitor.
//   (currently jack_handler mutes headset at all times) 
//  "headseton": [ {"reg":52,"val":57},{"reg":53,"val":319} ], "headsetoff": [  {"reg":52,"val":121},{"reg":53,"val":383} ], "speakeron": [ {"reg":54,"val":57},{"reg":55,"val":319} ], "speakeroff": [ {"reg":54,"val":121},{"reg":55,"val":383} ]}"; 
*/

// initiation table for non-readbale 9-bit i2c registers
static uint16_t WM8978_REGVAL_TBL[58] =
	{
		0X0000, 0X0000, 0X0000, 0X0000, 0X0050, 0X0000, 0X0140, 0X0000,
		0X0000, 0X0000, 0X0000, 0X00FF, 0X00FF, 0X0000, 0X0100, 0X00FF,
		0X00FF, 0X0000, 0X012C, 0X002C, 0X002C, 0X002C, 0X002C, 0X0000,
		0X0032, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000,
		0X0038, 0X000B, 0X0032, 0X0000, 0X0008, 0X000C, 0X0093, 0X00E9,
		0X0000, 0X0000, 0X0000, 0X0000, 0X0003, 0X0010, 0X0010, 0X0100,
		0X0100, 0X0002, 0X0001, 0X0001, 0X0039, 0X0039, 0X0039, 0X0039,
		0X0001, 0X0001};


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

	p = config_alloc_get_str("dac_controlset", CONFIG_DAC_CONTROLSET, default_controlset);
	i2c_json = cJSON_Parse(p);
	
	if (!i2c_addr || !i2c_json || i2c_config.sda_io_num == -1 || i2c_config.scl_io_num == -1) {
		if (p) free(p);
		ESP_LOGW(TAG, "No i2c controlset found");
		return true;
	}	
	
	ESP_LOGI(TAG, "WM8978 uses IC2 @%d with sda:%d, scl:%d", i2c_addr, i2c_config.sda_io_num, i2c_config.scl_io_num);
	
	// we have an I2C configured	
	i2c_param_config(i2c_port, &i2c_config);
	i2c_driver_install(i2c_port, I2C_MODE_MASTER, false, false, false);
		
	if (!i2c_json_execute("init")) {	
		ESP_LOGE(TAG, "could not intialize DAC");
		return false;
	}	

	// Configure system clk to GPIO0 for DAC MCLK input
    ESP_LOGI(TAG, "Configuring MCLK on pin:%d", 0);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
   	REG_WRITE(PIN_CTRL, 0xFFFFFFF0);
	
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
 * speaker
 */
static void speaker(bool active) {
	if (active) i2c_json_execute("speakeron");
	else i2c_json_execute("speakeroff");
} 

/****************************************************************************************
 * headset
 */
static void headset(bool active) {
	if (active) i2c_json_execute("headseton");
	else i2c_json_execute("headsetoff");
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
			uint16_t data = i2c_read_reg(reg->valueint);
			data |= (uint16_t) val->valueint;
			i2c_write_reg(reg->valueint, data);
		} else if (!strcasecmp(mode->valuestring, "and")) {
			uint16_t data = i2c_read_reg(reg->valueint);
			data &= (uint16_t) val->valueint;
			i2c_write_reg(reg->valueint, data);
        }
	}
	
	return true;
}	

/****************************************************************************************
 * 
 */
static esp_err_t i2c_write_reg(uint8_t reg, uint16_t val) {
	esp_err_t ret;
	uint8_t msb;
	uint8_t lsb;
    msb = (reg << 1) | ((val >> 8) & 0X01);
    lsb = val & 0XFF;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
	
	i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, I2C_MASTER_NACK);
	i2c_master_write_byte(cmd, msb, I2C_MASTER_NACK);
	i2c_master_write_byte(cmd, lsb, I2C_MASTER_NACK);
	
	i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_port, cmd, 100 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
	
	if (ret != ESP_OK) {
		ESP_LOGW(TAG, "I2C write failed");
	} else {
		// update local register
		WM8978_REGVAL_TBL[reg] = val;
        ESP_LOGD(TAG, "I2C Write OK %d=%d (bytes %d %d)", reg, val, msb, lsb);
    }
    
	return ret;
}

/****************************************************************************************
 *  Return local register value
 */
static uint16_t i2c_read_reg(uint8_t reg) {
	return WM8978_REGVAL_TBL[reg];
}


