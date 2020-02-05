/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <string.h>
#include <esp_log.h>
#include <esp_types.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c.h>
#include "adac.h"

//#include "audio_hal.h"
#include "ac101.h"

const static char TAG[] = "AC101";

#define AC_ASSERT(a, format, b, ...) \
    if ((a) != 0) { \
        ESP_LOGE(TAG, format, ##__VA_ARGS__); \
        return b;\
    }
	
static bool init(int i2c_port_num, int i2s_num, i2s_config_t *config);
static void deinit(void);
static void speaker(bool active);
static void headset(bool active);
static void volume(unsigned left, unsigned right);
static void power(adac_power_e mode);

struct adac_s dac_a1s = { init, deinit, power, speaker, headset, volume };

static esp_err_t i2c_write_reg(uint8_t reg, uint16_t val);
static uint16_t i2c_read_reg(uint8_t reg);
static esp_err_t ac101_start(ac_module_t mode);
static esp_err_t ac101_stop(void);
static esp_err_t ac101_set_earph_volume(uint8_t volume);
static esp_err_t ac101_set_spk_volume(uint8_t volume);
	
//static void pa_power(bool enable);

static int i2c_port;

/****************************************************************************************
 * init
 */
static bool init(int i2c_port_num, int i2s_num, i2s_config_t *i2s_config) {	 
	esp_err_t res;
	
	ESP_LOGI(TAG, "Initializing AC101");
	i2c_port = i2c_port_num;

	// configure i2c
	i2c_config_t i2c_config = {
			.mode = I2C_MODE_MASTER,
			.sda_io_num = 33,
			.sda_pullup_en = GPIO_PULLUP_ENABLE,
			.scl_io_num = 32,
			.scl_pullup_en = GPIO_PULLUP_ENABLE,
			.master.clk_speed = 100000,
		};
		
	i2c_param_config(i2c_port, &i2c_config);
	i2c_driver_install(i2c_port, I2C_MODE_MASTER, false, false, false);
	ESP_LOGI(TAG, "DAC using I2C sda:%u, scl:%u", i2c_config.sda_io_num, i2c_config.scl_io_num);
	
	res = i2c_write_reg(CHIP_AUDIO_RS, 0x123);
	
	//huh?
	//vTaskDelay(1000 / portTICK_PERIOD_MS); 
	if (ESP_OK != res) {
		ESP_LOGE(TAG, "reset failed!");
		return false;
	} 
	
	i2c_write_reg(SPKOUT_CTRL, 0xe880);

	// Enable the PLL from 256*44.1KHz MCLK source
	i2c_write_reg(PLL_CTRL1, 0x014f);
	//res |= i2c_write_reg(PLL_CTRL2, 0x83c0);
	i2c_write_reg(PLL_CTRL2, 0x8600);

	//Clocking system
	i2c_write_reg(SYSCLK_CTRL, 0x8b08);
	i2c_write_reg(MOD_CLK_ENA, 0x800c);
	i2c_write_reg(MOD_RST_CTRL, 0x800c);
	i2c_write_reg(I2S_SR_CTRL, 0x7000);			//sample rate
	 
	//AIF config
	i2c_write_reg(I2S1LCK_CTRL, 0x8850);			//BCLK/LRCK
	i2c_write_reg(I2S1_SDOUT_CTRL, 0xc000);		//
	i2c_write_reg(I2S1_SDIN_CTRL, 0xc000);
	i2c_write_reg(I2S1_MXR_SRC, 0x2200);			//

	i2c_write_reg(ADC_SRCBST_CTRL, 0xccc4);
	i2c_write_reg(ADC_SRC, 0x2020);
	i2c_write_reg(ADC_DIG_CTRL, 0x8000);
	i2c_write_reg(ADC_APC_CTRL, 0xbbc3);

	//Path Configuration
	i2c_write_reg(DAC_MXR_SRC, 0xcc00);
	i2c_write_reg(DAC_DIG_CTRL, 0x8000);
	i2c_write_reg(OMIXER_SR, 0x0081);
	i2c_write_reg(OMIXER_DACA_CTRL, 0xf080);//}

	//* Enable Speaker output
	i2c_write_reg(0x58, 0xeabd);

    //ac101_pa_power(true);

	uint16_t regval;

	// configure I2S		
	regval = i2c_read_reg(I2S1LCK_CTRL);
	regval &= 0xffc3;
	regval |= (AC_MODE_SLAVE << 15);
	regval |= (BIT_LENGTH_16_BITS << 4);
	regval |= (AC_MODE_SLAVE << 2);
	res |= i2c_write_reg(I2S1LCK_CTRL, regval);
	res |= i2c_write_reg(I2S_SR_CTRL, SAMPLE_RATE_44100);
			
	// configure I2S pins & install driver	
	i2s_pin_config_t i2s_pin_config = (i2s_pin_config_t) { 	.bck_io_num = 27, .ws_io_num = 26, 
															.data_out_num = 25, .data_in_num = 35 //Not used 
								};
	i2s_driver_install(i2s_num, i2s_config, 0, NULL);
	i2s_set_pin(i2s_num, &i2s_pin_config);

	ESP_LOGI(TAG, "DAC using I2S bck:%u, ws:%u, do:%u", i2s_pin_config.bck_io_num, i2s_pin_config.ws_io_num, i2s_pin_config.data_out_num);

	return true;
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
	// nothing at that point, volume is handled by backend
} 

/****************************************************************************************
 * power
 */
static void power(adac_power_e mode) {
	esp_err_t ret = ESP_OK;
	
	switch(mode) {
	case ADAC_STANDBY:
	case ADAC_OFF:
		ret = ac101_stop();
		break;
	case ADAC_ON:
		ret = ac101_start(AC_MODULE_DAC);
		break;		
	default:
		ESP_LOGW(TAG, "unknown power command");
		break;
	}
	
	if (ret != ESP_OK) ESP_LOGW(TAG, "can't start AC101 %d", ret);
}

/****************************************************************************************
 * speaker
 */
static void speaker(bool active) {
	if (active) i2c_write_reg(SPKOUT_CTRL, 0xeabd);
	else i2c_write_reg(SPKOUT_CTRL, 0xe880);		//disable speaker
} 

/****************************************************************************************
 * headset
 */
static void headset(bool active) {
	if (active) {
		i2c_write_reg(OMIXER_DACA_CTRL, 0xff80);
    	i2c_write_reg(HPOUT_CTRL, 0xc3c1);
    	i2c_write_reg(HPOUT_CTRL, 0xcb00);
		// huh?
    	vTaskDelay(100 / portTICK_PERIOD_MS);
		i2c_write_reg(HPOUT_CTRL, 0xfbc0);
	} else {
		i2c_write_reg(HPOUT_CTRL, 0x01);			//disable earphone
	}	
} 	

/****************************************************************************************
 * 
 */
static esp_err_t i2c_write_reg(uint8_t reg, uint16_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    esp_err_t ret =0;
	uint8_t send_buff[4];
	send_buff[0] = (AC101_ADDR << 1);
	send_buff[1] = reg;
	send_buff[2] = (val>>8) & 0xff;
	send_buff[3] = val & 0xff;
    ret |= i2c_master_start(cmd);
    ret |= i2c_master_write(cmd, send_buff, 4, ACK_CHECK_EN);
    ret |= i2c_master_stop(cmd);
    ret |= i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

/****************************************************************************************
 * 
 */
static uint16_t i2c_read_reg(uint8_t reg) {
	uint8_t data[2] = { 0 };
	
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( AC101_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( AC101_ADDR << 1 ) | READ_BIT, ACK_CHECK_EN);		//check or not
    i2c_master_read(cmd, data, 2, ACK_VAL);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

	return (data[0] << 8) + data[1];;
}

/****************************************************************************************
 * 
 */
void set_codec_clk(ac_adda_fs_i2s1_t rate) {
	i2c_write_reg(I2S_SR_CTRL, rate);
}

/****************************************************************************************
 * 
 */
static int ac101_get_spk_volume(void) {
    int res;
    res = i2c_read_reg(SPKOUT_CTRL);
    res &= 0x1f;
    return res*2;
}

/****************************************************************************************
 * 
 */
static esp_err_t ac101_set_spk_volume(uint8_t volume) {
	uint16_t res;
	esp_err_t ret;
	volume = volume/2;
	res = i2c_read_reg(SPKOUT_CTRL);
	res &= (~0x1f);
	volume &= 0x1f;
	res |= volume;
	ret = i2c_write_reg(SPKOUT_CTRL,res);
	return ret;
}

/****************************************************************************************
 * 
 */
static int ac101_get_earph_volume(void) {
    int res;
    res = i2c_read_reg(HPOUT_CTRL);
    return (res>>4)&0x3f;
}

/****************************************************************************************
 * 
 */
static esp_err_t ac101_set_earph_volume(uint8_t volume) {
	uint16_t res,tmp;
	esp_err_t ret;
	res = i2c_read_reg(HPOUT_CTRL);
	tmp = ~(0x3f<<4);
	res &= tmp;
	volume &= 0x3f;
	res |= (volume << 4);
	ret = i2c_write_reg(HPOUT_CTRL,res);
	return ret;
}

/****************************************************************************************
 * 
 */
static esp_err_t ac101_set_output_mixer_gain(ac_output_mixer_gain_t gain,ac_output_mixer_source_t source)
{
	uint16_t regval,temp,clrbit;
	esp_err_t ret;
	regval = i2c_read_reg(OMIXER_BST1_CTRL);
	switch(source){
	case SRC_MIC1:
		temp = (gain&0x7) << 6;
		clrbit = ~(0x7<<6);
		break;
	case SRC_MIC2:
		temp = (gain&0x7) << 3;
		clrbit = ~(0x7<<3);
		break;
	case SRC_LINEIN:
		temp = (gain&0x7);
		clrbit = ~0x7;
		break;
	default:
		return -1;
	}
	regval &= clrbit;
	regval |= temp;
	ret = i2c_write_reg(OMIXER_BST1_CTRL,regval);
	return ret;
}

/****************************************************************************************
 * 
 */
static esp_err_t ac101_start(ac_module_t mode) {
	esp_err_t res = 0;
	
    if (mode == AC_MODULE_LINE) {
		res |= i2c_write_reg(0x51, 0x0408);
		res |= i2c_write_reg(0x40, 0x8000);
		res |= i2c_write_reg(0x50, 0x3bc0);
    }
    if (mode == AC_MODULE_ADC || mode == AC_MODULE_ADC_DAC || mode == AC_MODULE_LINE) {
		//I2S1_SDOUT_CTRL
		//res |= i2c_write_reg(PLL_CTRL2, 0x8120);
    	res |= i2c_write_reg(0x04, 0x800c);
    	res |= i2c_write_reg(0x05, 0x800c);
		//res |= i2c_write_reg(0x06, 0x3000);
    }
    if (mode == AC_MODULE_DAC || mode == AC_MODULE_ADC_DAC || mode == AC_MODULE_LINE) {
    	//* Enable Headphone output   注意使用耳机时，最后开以下寄存器
		res |= i2c_write_reg(OMIXER_DACA_CTRL, 0xff80);
    	res |= i2c_write_reg(HPOUT_CTRL, 0xc3c1);
    	res |= i2c_write_reg(HPOUT_CTRL, 0xcb00);
		// huh?
    	vTaskDelay(100 / portTICK_PERIOD_MS);
		res |= i2c_write_reg(HPOUT_CTRL, 0xfbc0);

    	//* Enable Speaker output
		res |= i2c_write_reg(SPKOUT_CTRL, 0xeabd);
		// huh?
		vTaskDelay(10 / portTICK_PERIOD_MS);
		ac101_set_earph_volume(255);
		ac101_set_spk_volume(255);
    }

    return res;
}

/****************************************************************************************
 * 
 */
esp_err_t ac101_stop(void) {
	esp_err_t res = 0;
	res |= i2c_write_reg(HPOUT_CTRL, 0x01);			//disable earphone
	res |= i2c_write_reg(SPKOUT_CTRL, 0xe880);		//disable speaker
	return res;
}

/****************************************************************************************
 * 
 */
esp_err_t ac101_deinit(void) {
	return	i2c_write_reg(CHIP_AUDIO_RS, 0x123);		//soft reset
}


/****************************************************************************************
 * Don't know when this one is supposed to be called
 */
esp_err_t AC101_i2s_config_clock(ac_i2s_clock_t *cfg) {
	esp_err_t res = 0;
	uint16_t regval=0;
	regval = i2c_read_reg(I2S1LCK_CTRL);
	regval &= 0xe03f;
	regval |= (cfg->bclk_div << 9);
	regval |= (cfg->lclk_div << 6);
	res = i2c_write_reg(I2S1LCK_CTRL, regval);
	return res;
}

/****************************************************************************************
 * 
 */
esp_err_t ac101_get_voice_volume(int* volume) {
	*volume = ac101_get_earph_volume();
	return 0;
}

/*
void ac101_pa_power(bool enable) {
    gpio_config_t  io_conf;
    memset(&io_conf, 0, sizeof(io_conf));
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT(GPIO_PA_EN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    if (enable) {
        gpio_set_level(GPIO_PA_EN, 1);
    } else {
        gpio_set_level(GPIO_PA_EN, 0);
    }
}
*/