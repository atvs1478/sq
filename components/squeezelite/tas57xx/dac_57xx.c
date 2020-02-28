/* 
 *  Squeezelite for esp32
 *
 *  (c) Sebastien 2019
 *      Philippe G. 2019, philippe_44@outlook.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
 
#include "squeezelite.h" 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "adac.h"

// this is the only hard-wired thing
#define VOLUME_GPIO	14	

#define TAS575x 0x98
#define TAS578x	0x90

static bool init(int i2c_port_num, int i2s_num, i2s_config_t *config);
static void deinit(void);
static void speaker(bool active);
static void headset(bool active);
static void volume(unsigned left, unsigned right);
static void power(adac_power_e mode);

struct adac_s dac_tas57xx = { init, deinit, power, speaker, headset, volume };

struct tas57xx_cmd_s {
	uint8_t reg;
	uint8_t value;
};

static const struct tas57xx_cmd_s tas57xx_init_sequence[] = {
    { 0x00, 0x00 },		// select page 0
    { 0x02, 0x10 },		// standby
    { 0x0d, 0x10 },		// use SCK for PLL
	{ 0x25, 0x08 },		// ignore SCK halt 
	{ 0x08, 0x10 },		// Mute control enable (from TAS5780)
	{ 0x54, 0x02 },		// Mute output control (from TAS5780)
	{ 0x02, 0x00 },		// restart
	{ 0xff, 0xff }		// end of table
};

// matching orders
typedef enum { TAS57_ACTIVE = 0, TAS57_STANDBY, TAS57_DOWN, TAS57_ANALOGUE_OFF, TAS57_ANALOGUE_ON, TAS57_VOLUME } dac_cmd_e;

static const struct tas57xx_cmd_s tas57xx_cmd[] = {
	{ 0x02, 0x00 },	// TAS57_ACTIVE
	{ 0x02, 0x10 },	// TAS57_STANDBY
	{ 0x02, 0x01 },	// TAS57_DOWN
	{ 0x56, 0x10 },	// TAS57_ANALOGUE_OFF
	{ 0x56, 0x00 },	// TAS57_ANALOGUE_ON
};

static log_level loglevel = lINFO;
static u8_t tas57_addr;
static int i2c_port;

static void dac_cmd(dac_cmd_e cmd, ...);
static int tas57_detect(void);

/****************************************************************************************
 * init
 */
static bool init(int i2c_port_num, int i2s_num, i2s_config_t *i2s_config)	{	 
	i2c_port = i2c_port_num;
		
	// configure i2c
	i2c_config_t i2c_config = {
			.mode = I2C_MODE_MASTER,
			.sda_io_num = 27,
			.sda_pullup_en = GPIO_PULLUP_ENABLE,
			.scl_io_num = 26,
			.scl_pullup_en = GPIO_PULLUP_ENABLE,
			.master.clk_speed = 100000,
		};
		
	i2c_param_config(i2c_port, &i2c_config);
	i2c_driver_install(i2c_port, I2C_MODE_MASTER, false, false, false);
		
	// find which TAS we are using (if any)
	tas57_addr = tas57_detect();
	
	if (!tas57_addr) {
		LOG_WARN("No TAS57xx detected");
		i2c_driver_delete(i2c_port);
		return 0;
	}
	
	LOG_INFO("TAS57xx DAC using I2C sda:%u, scl:%u", i2c_config.sda_io_num, i2c_config.scl_io_num);
	
	i2c_cmd_handle_t i2c_cmd = i2c_cmd_link_create();
	
	for (int i = 0; tas57xx_init_sequence[i].reg != 0xff; i++) {
		i2c_master_start(i2c_cmd);
		i2c_master_write_byte(i2c_cmd, tas57_addr | I2C_MASTER_WRITE, I2C_MASTER_NACK);
		i2c_master_write_byte(i2c_cmd, tas57xx_init_sequence[i].reg, I2C_MASTER_NACK);
		i2c_master_write_byte(i2c_cmd, tas57xx_init_sequence[i].value, I2C_MASTER_NACK);

		LOG_DEBUG("i2c write %x at %u", tas57xx_init_sequence[i].reg, tas57xx_init_sequence[i].value);
	}

	i2c_master_stop(i2c_cmd);	
	esp_err_t res = i2c_master_cmd_begin(i2c_port, i2c_cmd, 500 / portTICK_RATE_MS);
    i2c_cmd_link_delete(i2c_cmd);

	// configure I2S pins & install driver	
	i2s_pin_config_t i2s_pin_config = (i2s_pin_config_t) { 	.bck_io_num = CONFIG_I2S_BCK_IO, .ws_io_num = CONFIG_I2S_WS_IO, 
														.data_out_num = CONFIG_I2S_DO_IO, .data_in_num = CONFIG_I2S_DI_IO,
								};
	res |= i2s_driver_install(i2s_num, i2s_config, 0, NULL);
	res |= i2s_set_pin(i2s_num, &i2s_pin_config);
	LOG_INFO("DAC using I2S bck:%d, ws:%d, do:%d", i2s_pin_config.bck_io_num, i2s_pin_config.ws_io_num, i2s_pin_config.data_out_num);
	
	if (res == ESP_OK) {
		// init volume & mute
		gpio_pad_select_gpio(VOLUME_GPIO);
		gpio_set_direction(VOLUME_GPIO, GPIO_MODE_OUTPUT);
		gpio_set_level(VOLUME_GPIO, 0);
		return true;
	} else {
		LOG_ERROR("could not intialize TAS57xx %d", res);
		return false;
	}	
}	

/****************************************************************************************
 * init
 */
static void deinit(void)	{	 
	i2c_driver_delete(i2c_port);
}

/****************************************************************************************
 * change volume
 */
static void volume(unsigned left, unsigned right) {
	LOG_INFO("TAS57xx volume (L:%u R:%u)", left, right);
	gpio_set_level(VOLUME_GPIO, left || right);
} 

/****************************************************************************************
 * power
 */
static void power(adac_power_e mode) {
	switch(mode) {
	case ADAC_STANDBY:
		dac_cmd(TAS57_STANDBY);
		break;
	case ADAC_ON:
		dac_cmd(TAS57_ACTIVE);
		break;		
	case ADAC_OFF:
		dac_cmd(TAS57_DOWN);
		break;				
	default:
		LOG_WARN("unknown DAC command");
		break;
	}
}

/****************************************************************************************
 * speaker
 */
static void speaker(bool active) {
	if (active) dac_cmd(TAS57_ANALOGUE_ON);
	else dac_cmd(TAS57_ANALOGUE_OFF);
} 

/****************************************************************************************
 * headset
 */
static void headset(bool active) {
} 
 
/****************************************************************************************
 * DAC specific commands
 */
void dac_cmd(dac_cmd_e cmd, ...) {
	va_list args;
	esp_err_t ret = ESP_OK;
	
	va_start(args, cmd);
	i2c_cmd_handle_t i2c_cmd = i2c_cmd_link_create();

	switch(cmd) {
	case TAS57_VOLUME:
		LOG_ERROR("DAC volume not handled yet");
		break;
	default:
		i2c_master_start(i2c_cmd);
		i2c_master_write_byte(i2c_cmd, tas57_addr | I2C_MASTER_WRITE, I2C_MASTER_NACK);
		i2c_master_write_byte(i2c_cmd, tas57xx_cmd[cmd].reg, I2C_MASTER_NACK);
		i2c_master_write_byte(i2c_cmd, tas57xx_cmd[cmd].value, I2C_MASTER_NACK);
		i2c_master_stop(i2c_cmd);	
		ret	= i2c_master_cmd_begin(i2c_port, i2c_cmd, 50 / portTICK_RATE_MS);
	}
	
    i2c_cmd_link_delete(i2c_cmd);
	
	if (ret != ESP_OK) {
		LOG_ERROR("could not intialize TAS57xx %d", ret);
	}

	va_end(args);
}

/****************************************************************************************
 * TAS57 detection
 */
static int tas57_detect(void) {
	u8_t data, addr[] = {TAS578x, TAS575x};
	int ret;
	
	for (int i = 0; i < sizeof(addr); i++) {
		i2c_cmd_handle_t i2c_cmd = i2c_cmd_link_create();

		i2c_master_start(i2c_cmd);
		i2c_master_write_byte(i2c_cmd, addr[i] | I2C_MASTER_WRITE, I2C_MASTER_NACK);
		i2c_master_write_byte(i2c_cmd, 00, I2C_MASTER_NACK);
		
		i2c_master_start(i2c_cmd);			
		i2c_master_write_byte(i2c_cmd, addr[i] | I2C_MASTER_READ, I2C_MASTER_NACK);
		i2c_master_read_byte(i2c_cmd, &data, I2C_MASTER_NACK);
		
		i2c_master_stop(i2c_cmd);	
		ret	= i2c_master_cmd_begin(i2c_port, i2c_cmd, 50 / portTICK_RATE_MS);
		i2c_cmd_link_delete(i2c_cmd);	
		
		if (ret == ESP_OK) {
			LOG_INFO("Detected TAS @0x%x", addr[i]);
			return addr[i];
		}	
	}	
	
	return 0;
}

