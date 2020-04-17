/* cmd_i2ctools.c

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <stdio.h>
#include "cmd_i2ctools.h"
#include "argtable3/argtable3.h"
#include "driver/i2c.h"
#include "platform_console.h"
#include "esp_log.h"
#include "string.h"
#include "stdio.h"
#include "platform_config.h"
#include "accessors.h"
#include "trace.h"
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define WRITE_BIT I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ    /*!< I2C master read */
#define ACK_CHECK_EN 0x1            /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                 /*!< I2C ack value */
#define NACK_VAL 0x1                /*!< I2C nack value */

static const char *TAG = "cmd_i2ctools";

static gpio_num_t i2c_gpio_sda = 19;
static gpio_num_t i2c_gpio_scl = 18;
static uint32_t i2c_frequency = 100000;
#ifdef CONFIG_I2C_LOCKED
static i2c_port_t i2c_port = I2C_NUM_1;
#else
static i2c_port_t i2c_port = I2C_NUM_0;
#endif

static struct {
    struct arg_int *chip_address;
    struct arg_int *register_address;
    struct arg_int *data_length;
    struct arg_end *end;
} i2cget_args;

static struct {
    struct arg_int *chip_address;
    struct arg_int *register_address;
    struct arg_int *data;
    struct arg_end *end;
} i2cset_args;

static struct {
    struct arg_int *chip_address;
    struct arg_int *size;
    struct arg_end *end;
} i2cdump_args;

static struct {
	struct arg_lit *load;
    struct arg_int *port;
    struct arg_int *freq;
    struct arg_int *sda;
    struct arg_int *scl;
    struct arg_end *end;
} i2cconfig_args;


static struct {
    struct arg_int *port;
    struct arg_end *end;
} i2cstop_args;

static struct {
    struct arg_int *port;
    struct arg_end *end;
} i2ccheck_args;

static struct {
	struct arg_lit *clear;
	struct arg_lit *hflip;
	struct arg_lit *vflip;
	struct arg_lit *rotate;
	struct arg_int *address;
	struct arg_int *width;
	struct arg_int *height;
	struct arg_str *name;
	struct arg_str *driver;
	struct arg_end *end;
} i2cdisp_args;


bool is_i2c_started(i2c_port_t port){
	esp_err_t ret = ESP_OK;
	ESP_LOGD(TAG,"Determining if i2c is started on port %u", port);
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ret = i2c_master_start(cmd);
    if(ret == ESP_OK){
    	ret = i2c_master_write_byte(cmd,WRITE_BIT, ACK_CHECK_EN);
    }
    if(ret == ESP_OK){
    	ret = i2c_master_stop(cmd);
    }
    if(ret == ESP_OK){
    	ret = i2c_master_cmd_begin(port, cmd, 50 / portTICK_RATE_MS);
    }
    i2c_cmd_link_delete(cmd);
    ESP_LOGD(TAG,"i2c is %s. %s",ret!=ESP_ERR_INVALID_STATE?"started":"not started", esp_err_to_name(ret));
    return (ret!=ESP_ERR_INVALID_STATE);
}


typedef struct {
	uint8_t address;
	char * description;
} i2c_db_t;


// the list was taken from https://i2cdevices.org/addresses
// on 2020-01-16
static const i2c_db_t i2c_db[] = {
		{ .address = 0x00, .description = "Unknown"},
		{ .address = 0x01, .description = "Unknown"},
		{ .address = 0x02, .description = "Unknown"},
		{ .address = 0x03, .description = "Unknown"},
		{ .address = 0x04, .description = "Unknown"},
		{ .address = 0x05, .description = "Unknown"},
		{ .address = 0x06, .description = "Unknown"},
		{ .address = 0x07, .description = "Unknown"},
		{ .address = 0x0c, .description = "AK8975"},
		{ .address = 0x0d, .description = "AK8975"},
		{ .address = 0x0e, .description = "MAG3110 AK8975 IST-8310"},
		{ .address = 0x0f, .description = "AK8975"},
		{ .address = 0x10, .description = "VEML7700 VML6075"},
		{ .address = 0x11, .description = "Si4713 SAA5246 SAA5243P/K SAA5243P/L SAA5243P/E SAA5243P/H"},
		{ .address = 0x13, .description = "VCNL40x0"},
		{ .address = 0x18, .description = "MCP9808 LIS3DH LSM303"},
		{ .address = 0x19, .description = "MCP9808 LIS3DH LSM303"},
		{ .address = 0x1a, .description = "MCP9808"},
		{ .address = 0x1b, .description = "MCP9808"},
		{ .address = 0x1c, .description = "MCP9808 MMA845x FXOS8700"},
		{ .address = 0x1d, .description = "MCP9808 MMA845x ADXL345 FXOS8700"},
		{ .address = 0x1e, .description = "MCP9808 FXOS8700 HMC5883 LSM303 LSM303"},
		{ .address = 0x1f, .description = "MCP9808 FXOS8700"},
		{ .address = 0x20, .description = "FXAS21002 MCP23008 MCP23017 Chirp!"},
		{ .address = 0x21, .description = "FXAS21002 MCP23008 MCP23017 SAA4700"},
		{ .address = 0x22, .description = "MCP23008 MCP23017 PCA1070"},
		{ .address = 0x23, .description = "MCP23008 MCP23017 SAA4700"},
		{ .address = 0x24, .description = "MCP23008 MCP23017 PCD3311C PCD3312C"},
		{ .address = 0x25, .description = "MCP23008 MCP23017 PCD3311C PCD3312C"},
		{ .address = 0x26, .description = "MCP23008 MCP23017"},
		{ .address = 0x27, .description = "MCP23008 MCP23017"},
		{ .address = 0x28, .description = "BNO055 CAP1188"},
		{ .address = 0x29, .description = "BNO055 CAP1188 TCS34725 TSL2591 VL53L0x VL6180X"},
		{ .address = 0x2a, .description = "CAP1188"},
		{ .address = 0x2b, .description = "CAP1188"},
		{ .address = 0x2c, .description = "CAP1188 AD5248 AD5251 AD5252 CAT5171"},
		{ .address = 0x2d, .description = "CAP1188 AD5248 AD5251 AD5252 CAT5171"},
		{ .address = 0x2e, .description = "AD5248 AD5251 AD5252"},
		{ .address = 0x2f, .description = "AD5248 AD5243 AD5251 AD5252"},
		{ .address = 0x30, .description = "SAA2502"},
		{ .address = 0x31, .description = "SAA2502"},
		{ .address = 0x38, .description = "FT6x06 VEML6070 BMA150 SAA1064"},
		{ .address = 0x39, .description = "TSL2561 APDS-9960 VEML6070 SAA1064"},
		{ .address = 0x3a, .description = "PCF8577C SAA1064"},
		{ .address = 0x3b, .description = "SAA1064 PCF8569"},
		{ .address = 0x3c, .description = "SSD1305 SSD1306 PCF8578 PCF8569 SH1106"},
		{ .address = 0x3d, .description = "SSD1305 SSD1306 PCF8578 SH1106"},
		{ .address = 0x40, .description = "HTU21D-F TMP007 PCA9685 NE5751 TDA8421 INA260 TEA6320 TEA6330 TMP006 TEA6300 Si7021 INA219 TDA9860"},
		{ .address = 0x41, .description = "TMP007 PCA9685 STMPE811 TDA8424 NE5751 TDA8421 INA260 STMPE610 TDA8425 TMP006 INA219 TDA9860 TDA8426"},
		{ .address = 0x42, .description = "HDC1008 TMP007 TMP006 PCA9685 INA219 TDA8415 TDA8417 INA260"},
		{ .address = 0x43, .description = "HDC1008 TMP007 TMP006 PCA9685 INA219 INA260"},
		{ .address = 0x44, .description = "TMP007 TMP006 PCA9685 INA219 STMPE610 SHT31 ISL29125 STMPE811 TDA4688 TDA4672 TDA4780 TDA4670 TDA8442 TDA4687 TDA4671 TDA4680 INA260"},
		{ .address = 0x45, .description = "TMP007 TMP006 PCA9685 INA219 SHT31 TDA8376 INA260"},
		{ .address = 0x46, .description = "TMP007 TMP006 PCA9685 INA219 TDA9150 TDA8370 INA260"},
		{ .address = 0x47, .description = "TMP007 TMP006 PCA9685 INA219 INA260"},
		{ .address = 0x48, .description = "PCA9685 INA219 PN532 TMP102 INA260 ADS1115"},
		{ .address = 0x49, .description = "TSL2561 PCA9685 INA219 TMP102 INA260 ADS1115 AS7262"},
		{ .address = 0x4a, .description = "PCA9685 INA219 TMP102 ADS1115 MAX44009 INA260"},
		{ .address = 0x4b, .description = "PCA9685 INA219 TMP102 ADS1115 MAX44009 INA260"},
		{ .address = 0x4c, .description = "PCA9685 INA219 INA260"},
		{ .address = 0x4d, .description = "PCA9685 INA219 INA260"},
		{ .address = 0x4e, .description = "PCA9685 INA219 INA260"},
		{ .address = 0x4f, .description = "PCA9685 INA219 INA260"},
		{ .address = 0x50, .description = "PCA9685 MB85RC"},
		{ .address = 0x51, .description = "PCA9685 MB85RC"},
		{ .address = 0x52, .description = "PCA9685 MB85RC Nunchuck controller APDS-9250"},
		{ .address = 0x53, .description = "ADXL345 PCA9685 MB85RC"},
		{ .address = 0x54, .description = "PCA9685 MB85RC"},
		{ .address = 0x55, .description = "PCA9685 MB85RC"},
		{ .address = 0x56, .description = "PCA9685 MB85RC"},
		{ .address = 0x57, .description = "PCA9685 MB85RC MAX3010x"},
		{ .address = 0x58, .description = "PCA9685 TPA2016 SGP30"},
		{ .address = 0x59, .description = "PCA9685"},
		{ .address = 0x5a, .description = "PCA9685 CCS811 MLX90614 DRV2605 MPR121"},
		{ .address = 0x5b, .description = "PCA9685 CCS811 MPR121"},
		{ .address = 0x5c, .description = "PCA9685 AM2315 MPR121"},
		{ .address = 0x5d, .description = "PCA9685 MPR121"},
		{ .address = 0x5e, .description = "PCA9685"},
		{ .address = 0x5f, .description = "PCA9685 HTS221"},
		{ .address = 0x60, .description = "PCA9685 MPL115A2 MPL3115A2 Si5351A Si1145 MCP4725A0 TEA5767 TSA5511 SAB3037 SAB3035 MCP4725A1"},
		{ .address = 0x61, .description = "PCA9685 Si5351A MCP4725A0 TEA6100 TSA5511 SAB3037 SAB3035 MCP4725A1"},
		{ .address = 0x62, .description = "PCA9685 MCP4725A1 TSA5511 SAB3037 SAB3035 UMA1014T"},
		{ .address = 0x63, .description = "Si4713 PCA9685 MCP4725A1 TSA5511 SAB3037 SAB3035 UMA1014T"},
		{ .address = 0x64, .description = "PCA9685 MCP4725A2 MCP4725A1"},
		{ .address = 0x65, .description = "PCA9685 MCP4725A2 MCP4725A1"},
		{ .address = 0x66, .description = "PCA9685 MCP4725A3 IS31FL3731 MCP4725A1"},
		{ .address = 0x67, .description = "PCA9685 MCP4725A3 MCP4725A1"},
		{ .address = 0x68, .description = "PCA9685 AMG8833 DS1307 PCF8523 DS3231 MPU-9250 ITG3200 PCF8573 MPU6050"},
		{ .address = 0x69, .description = "PCA9685 AMG8833 MPU-9250 ITG3200 PCF8573 SPS30 MPU6050"},
		{ .address = 0x6a, .description = "PCA9685 L3GD20H PCF8573"},
		{ .address = 0x6b, .description = "PCA9685 L3GD20H PCF8573"},
		{ .address = 0x6c, .description = "PCA9685"},
		{ .address = 0x6d, .description = "PCA9685"},
		{ .address = 0x6e, .description = "PCA9685"},
		{ .address = 0x6f, .description = "PCA9685"},
		{ .address = 0x70, .description = "PCA9685 TCA9548 HT16K33"},
		{ .address = 0x71, .description = "PCA9685 TCA9548 HT16K33"},
		{ .address = 0x72, .description = "PCA9685 TCA9548 HT16K33"},
		{ .address = 0x73, .description = "PCA9685 TCA9548 HT16K33"},
		{ .address = 0x74, .description = "PCA9685 TCA9548 HT16K33"},
		{ .address = 0x75, .description = "PCA9685 TCA9548 HT16K33"},
		{ .address = 0x76, .description = "PCA9685 TCA9548 HT16K33 BME280 BMP280 MS5607 MS5611 BME680"},
		{ .address = 0x77, .description = "PCA9685 TCA9548 HT16K33 IS31FL3731 BME280 BMP280 MS5607 BMP180 BMP085 BMA180 MS5611 BME680"},
		{ .address = 0x78, .description = "PCA9685"},
		{ .address = 0x79, .description = "PCA9685"},
		{ .address = 0x7a, .description = "PCA9685"},
		{ .address = 0x7b, .description = "PCA9685"},
		{ .address = 0x7c, .description = "PCA9685"},
		{ .address = 0x7d, .description = "PCA9685"},
		{ .address = 0x7e, .description = "PCA9685"},
		{ .address = 0x7f, .description = "PCA9685"},
		{ .address = 0, .description = NULL}
};
void i2c_load_configuration(){
	ESP_LOGD(TAG,"Loading configuration from nvs");
	const i2c_config_t * conf =  config_i2c_get((int *)&i2c_port);
	i2c_gpio_scl = conf->scl_io_num;
	i2c_gpio_sda = conf->sda_io_num;
	i2c_frequency = conf->master.clk_speed;
}

const char * i2c_get_description(uint8_t address){
	uint8_t i=0;
	while(i2c_db[i].description && i2c_db[i].address!=address) i++;
	return i2c_db[i].description?i2c_db[i].description:"Unlisted";
}

static esp_err_t i2c_get_port(int port, i2c_port_t *i2c_port)
{
    if (port >= I2C_NUM_MAX) {
        ESP_LOGE(TAG, "Wrong port number: %d", port);
        return ESP_FAIL;
    }
    switch (port) {
    case 0:
        *i2c_port = I2C_NUM_0;
        break;
    case 1:
        *i2c_port = I2C_NUM_1;
        break;
    default:
        *i2c_port = I2C_NUM_0;
        break;
    }
    return ESP_OK;
}
static esp_err_t i2c_master_driver_install(){
	esp_err_t err=ESP_OK;
	ESP_LOGD(TAG,"Installing i2c driver on port %u", i2c_port);
	if((err=i2c_driver_install(i2c_port, I2C_MODE_MASTER, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0))!=ESP_OK){
		ESP_LOGE(TAG,"Driver install failed! %s", esp_err_to_name(err));
	}
	return err;
}

static esp_err_t i2c_master_driver_initialize()
{
	esp_err_t err=ESP_OK;
	i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = i2c_gpio_sda,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = i2c_gpio_scl,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = i2c_frequency
    };
    ESP_LOGI(TAG,"Initializing i2c driver configuration.\n   mode = I2C_MODE_MASTER, \n   scl_pullup_en = GPIO_PULLUP_ENABLE, \n   i2c port = %u, \n   sda_io_num = %u, \n   sda_pullup_en = GPIO_PULLUP_ENABLE, \n   scl_io_num = %u, \n   scl_pullup_en = GPIO_PULLUP_ENABLE, \n   master.clk_speed = %u", i2c_port, i2c_gpio_sda,i2c_gpio_scl,i2c_frequency);
    if((err=i2c_param_config(i2c_port, &conf))!=ESP_OK){
    	ESP_LOGE(TAG,"i2c driver config load failed. %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t i2c_initialize_driver_from_config(){
	esp_err_t err = ESP_OK;
	ESP_LOGD(TAG,"Initializing driver from configuration.");
	i2c_load_configuration();
	if(is_i2c_started(i2c_port)){
		ESP_LOGW(TAG, "Stopping i2c driver on port %u", i2c_port);
		//  stop the current driver instance
		if((err=i2c_driver_delete(i2c_port))!=ESP_OK){
			ESP_LOGE(TAG,"i2c driver delete failed. %s", esp_err_to_name(err));
		}
	}
	if(err==ESP_OK){
		err = i2c_master_driver_initialize();
	}
	if(err == ESP_OK){
		err = i2c_master_driver_install();
	}
	return err;
}


static int do_i2c_stop(int argc, char **argv ){

    int nerrors = arg_parse(argc, argv, (void **)&i2cstop_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, i2cstop_args.end, argv[0]);
        return 0;
    }
    if (i2cstop_args.port->count && i2c_get_port(i2cstop_args.port->ival[0], &i2c_port) != ESP_OK) {
        return 1;
    }
	ESP_LOGW(TAG,"Stopping i2c on port %u.",i2c_port);
	i2c_driver_delete(i2c_port);
	return 0;
}
static int do_i2c_check(int argc, char **argv ){

	i2c_port_t port=0;
    int nerrors = arg_parse(argc, argv, (void **)&i2ccheck_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, i2ccheck_args.end, argv[0]);
        return 0;
    }
    port=i2c_port;

    if (i2ccheck_args.port->count && i2c_get_port(i2ccheck_args.port->ival[0], &port) != ESP_OK) {
        return 1;
    }
    bool started=is_i2c_started(port);
	ESP_LOGI(TAG,"i2c is %s on port %u.", started?"started":"not started",port );
	return 0;
}
static int do_i2c_show_display(int argc, char **argv){
	char * config_string = (char * )config_alloc_get(NVS_TYPE_STR, "display_config") ;
	if(config_string){
		ESP_LOGI(TAG,"Display configuration string is : \n"
					"display_config = \"%s\"",config_string);
		free(config_string);
	}
	else {
		ESP_LOGW(TAG,"No display configuration found in nvs config display_config");
	}
	char * nvs_item = config_alloc_get(NVS_TYPE_STR, "i2c_config");
	if (nvs_item) {
		ESP_LOGI(TAG,"I2C configuration is: %s", nvs_item);
		free(nvs_item);
	}
	return 0;
}

static int do_i2c_set_display(int argc, char **argv)
{
	int width=0, height=0, address=60;
	char * name = NULL;
	char * driver= NULL;
	char config_string[200]={};
    int nerrors = arg_parse(argc, argv, (void **)&i2cdisp_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, i2cdisp_args.end, argv[0]);
        return 0;
    }

	/* Check "--clear" option */
	if (i2cdisp_args.clear->count) {
		ESP_LOGW(TAG,"Clearing display config");
		config_set_value(NVS_TYPE_STR, "display_config", "");
		return 0;
	}




	/* Check "--address" option */
	if (i2cdisp_args.address->count) {
		address=i2cdisp_args.address->ival[0];
	}

	/* Check "--width" option */
	if (i2cdisp_args.width->count) {
		width=i2cdisp_args.width->ival[0];
	}
	else {
		ESP_LOGE(TAG,"Missing parameter: --width");
		nerrors ++;
	}

	/* Check "--height" option */
	if (i2cdisp_args.height->count) {
		height=i2cdisp_args.height->ival[0];
	}
	else {
		ESP_LOGE(TAG,"Missing parameter: --height");
		nerrors ++;
	}
	/* Check "--name" option */
	if (i2cdisp_args.name->count) {
		name=strdup(i2cdisp_args.name->sval[0]);
	}

	/* Check "--driver" option */
	if (i2cdisp_args.driver->count) {
		driver=strdup(i2cdisp_args.driver->sval[0]);
	}

	if(!name) name = strdup("I2C");
	if(!driver) driver = strdup("SSD1306");

	bool rotate = i2cdisp_args.rotate->count>0;

	snprintf(config_string, sizeof(config_string),"%s:width=%i,height=%i,address=%i,driver=%s%s%s",
				name,width,height,address,driver,rotate || i2cdisp_args.hflip->count?",HFlip":"",rotate || i2cdisp_args.vflip->count?",VFlip":"" );
	free(name);
	free(driver);

	if(nerrors!=0){
		return 0;
	}

	ESP_LOGI(TAG,"Updating display configuration string configuration to :\n"
			"display_config = \"%s\"",config_string );

	return config_set_value(NVS_TYPE_STR, "display_config", config_string)!=ESP_OK;
}

static int do_i2cconfig_cmd(int argc, char **argv)
{
	esp_err_t err=ESP_OK;
	int res=0;
    int nerrors = arg_parse(argc, argv, (void **)&i2cconfig_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, i2cconfig_args.end, argv[0]);
        return 0;
    }
    /* Check "--load" option */
    if (i2cconfig_args.load->count) {
    	ESP_LOGW(TAG,"Loading i2c config");
    	i2c_load_configuration();
    }
    else {

		/* Check "--port" option */
		if (i2cconfig_args.port->count) {
			if (i2c_get_port(i2cconfig_args.port->ival[0], &i2c_port) != ESP_OK) {
				return 1;
			}
		}
		/* Check "--freq" option */
		if (i2cconfig_args.freq->count) {
			i2c_frequency = i2cconfig_args.freq->ival[0];
		}
		if (i2cconfig_args.sda->count){
			/* Check "--sda" option */
			i2c_gpio_sda = i2cconfig_args.sda->ival[0];
		}
		else {
			ESP_LOGE(TAG,"Missing --sda option.");
			res=1;
		}

		if (i2cconfig_args.scl->count){
			/* Check "--sda" option */
			i2c_gpio_scl = i2cconfig_args.scl->ival[0];
		}
		else {
			ESP_LOGE(TAG,"Missing --scl option.");
			res=1;
		}
    }

#ifdef CONFIG_SQUEEZEAMP
	if (i2c_port == I2C_NUM_0) {
		i2c_port = I2C_NUM_1;
		ESP_LOGE(TAG, "can't use i2c port 0 on SqueezeAMP. Changing to port 1.");
	}
#endif
	if(!res){
		ESP_LOGI(TAG, "Uninstall i2c driver from port %u if needed",i2c_port);
		if(is_i2c_started(i2c_port)){
			if((err=i2c_driver_delete(i2c_port))!=ESP_OK){
				ESP_LOGE(TAG,"i2c driver delete failed. %s", esp_err_to_name(err));
				res = 1;
			}
		}
	}
	if(!res){
		ESP_LOGI(TAG,"Initializing driver with config scl=%u sda=%u speed=%u port=%u",i2c_gpio_scl,i2c_gpio_sda,i2c_frequency,i2c_port);
		if((err=i2c_master_driver_initialize())==ESP_OK){
			ESP_LOGI(TAG,"Initalize success.");
			// now start the i2c driver
			ESP_LOGI(TAG,"Starting the i2c driver.");
			if((err=i2c_master_driver_install())!=ESP_OK){
				 res=1;
			}
			else
			{
				ESP_LOGI(TAG,"i2c driver successfully started.");
			}
		}
		else {
			ESP_LOGE(TAG,"I2C initialization failed. %s", esp_err_to_name(err));
			res=1;
		}
	}
	if(!res && !i2cconfig_args.load->count){
			ESP_LOGI(TAG,"Storing i2c parameters.");
			i2c_config_t config={
				.mode = I2C_MODE_MASTER,
				.sda_io_num = i2c_gpio_sda,
				.sda_pullup_en = GPIO_PULLUP_ENABLE,
				.scl_io_num = i2c_gpio_scl,
				.scl_pullup_en = GPIO_PULLUP_ENABLE,
				.master.clk_speed = i2c_frequency
			};
			config_i2c_set(&config, i2c_port);
	}

    return res;
}

#define RUN_SHOW_ERROR(c)


static int do_i2cdump_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&i2cdump_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, i2cdump_args.end, argv[0]);
        return 0;
    }

    /* Check chip address: "-c" option */
    int chip_addr = i2cdump_args.chip_address->ival[0];
    /* Check read size: "-s" option */
    int size = 1;
    if (i2cdump_args.size->count) {
        size = i2cdump_args.size->ival[0];
    }
    if (size != 1 && size != 2 && size != 4) {
        ESP_LOGE(TAG, "Wrong read size. Only support 1,2,4");
        return 1;
    }
    esp_err_t ret = i2c_initialize_driver_from_config();
	if(ret!=ESP_OK) return 0;

    uint8_t data_addr;
    uint8_t data[4];
    int32_t block[16];
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f"
           "    0123456789abcdef\r\n");
    for (int i = 0; i < 128; i += 16) {
        printf("%02x: ", i);
        for (int j = 0; j < 16; j += size) {
            fflush(stdout);
            data_addr = i + j;
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, chip_addr << 1 | WRITE_BIT, ACK_CHECK_EN);
            i2c_master_write_byte(cmd, data_addr, ACK_CHECK_EN);
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, chip_addr << 1 | READ_BIT, ACK_CHECK_EN);
            if (size > 1) {
                i2c_master_read(cmd, data, size - 1, ACK_VAL);
            }
            i2c_master_read_byte(cmd, data + size - 1, NACK_VAL);
            i2c_master_stop(cmd);
            esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, 50 / portTICK_RATE_MS);
            i2c_cmd_link_delete(cmd);
            if (ret == ESP_OK) {
                for (int k = 0; k < size; k++) {
                    printf("%02x ", data[k]);
                    block[j + k] = data[k];
                }
            } else {
                for (int k = 0; k < size; k++) {
                    printf("XX ");
                    block[j + k] = -1;
                }
            }
        }
        printf("   ");
        for (int k = 0; k < 16; k++) {
            if (block[k] < 0) {
                printf("X");
            }
            if ((block[k] & 0xff) == 0x00 || (block[k] & 0xff) == 0xff) {
                printf(".");
            } else if ((block[k] & 0xff) < 32 || (block[k] & 0xff) >= 127) {
                printf("?");
            } else {
                printf("%c", block[k] & 0xff);
            }
        }
        printf("\r\n");
    }
    // Don't stop the driver;  our firmware may be using it for screen, etc
    //i2c_driver_delete(i2c_port);
    return 0;
}
static int do_i2cset_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&i2cset_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, i2cset_args.end, argv[0]);
        return 0;
    }

    /* Check chip address: "-c" option */
    int chip_addr = i2cset_args.chip_address->ival[0];
    /* Check register address: "-r" option */
    int data_addr = 0;
    if (i2cset_args.register_address->count) {
        data_addr = i2cset_args.register_address->ival[0];
    }
    /* Check data: "-d" option */
    int len = i2cset_args.data->count;

    i2c_master_driver_initialize();
	if(i2c_master_driver_install()!=ESP_OK){
		 return 1;
	}

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, chip_addr << 1 | WRITE_BIT, ACK_CHECK_EN);
    if (i2cset_args.register_address->count) {
        i2c_master_write_byte(cmd, data_addr, ACK_CHECK_EN);
    }
    for (int i = 0; i < len; i++) {
        i2c_master_write_byte(cmd, i2cset_args.data->ival[i], ACK_CHECK_EN);
    }
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Write OK");
    } else if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "Bus is busy");
    } else {
        ESP_LOGW(TAG, "Write Failed");
    }
    // Don't stop the driver;  our firmware may be using it for screen, etc
    //i2c_driver_delete(i2c_port);
    return 0;
}

static int do_i2cget_cmd(int argc, char **argv)
{
	esp_err_t err=ESP_OK;
    int nerrors = arg_parse(argc, argv, (void **)&i2cget_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, i2cget_args.end, argv[0]);
        return 0;
    }

    /* Check chip address: "-c" option */
    int chip_addr = i2cget_args.chip_address->ival[0];
    /* Check register address: "-r" option */
    int data_addr = -1;
    if (i2cget_args.register_address->count) {
        data_addr = i2cget_args.register_address->ival[0];
    }
    /* Check data length: "-l" option */
    int len = 1;
    if (i2cget_args.data_length->count) {
        len = i2cget_args.data_length->ival[0];
    }


    if((err=i2c_master_driver_initialize())!=ESP_OK){
      	ESP_LOGE(TAG,"Error initializing i2c driver. %s",esp_err_to_name(err));
      	return 1;
     }
	if((err=i2c_master_driver_install())!=ESP_OK){
		 return 1;
	}

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    uint8_t *data = malloc(len);
    if (data_addr != -1) {
        i2c_master_write_byte(cmd, chip_addr << 1 | WRITE_BIT, ACK_CHECK_EN);
        i2c_master_write_byte(cmd, data_addr, ACK_CHECK_EN);
        i2c_master_start(cmd);
    }
    i2c_master_write_byte(cmd, chip_addr << 1 | READ_BIT, ACK_CHECK_EN);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, data + len - 1, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_OK) {
        for (int i = 0; i < len; i++) {
            printf("0x%02x ", data[i]);
            if ((i + 1) % 16 == 0) {
                printf("\r\n");
            }
        }
        if (len % 16) {
            printf("\r\n");
        }
    } else if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "Bus is busy");
    } else {
        ESP_LOGW(TAG, "Read failed");
    }
    free(data);
    // Don't stop the driver;  our firmware may be using it for screen, etc
    //i2c_driver_delete(i2c_port);
    return 0;
}

static int do_i2cdetect_cmd(int argc, char **argv)
{
	uint8_t matches[128]={};
	int last_match=0;
	esp_err_t ret = i2c_initialize_driver_from_config();
	if(ret!=ESP_OK) return 0;
    uint8_t address;
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    for (int i = 0; i < 128 ; i += 16) {
        printf("%02x: ", i);
        for (int j = 0; j < 16 ; j++) {
            fflush(stdout);
            address = i + j;
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (address << 1) | WRITE_BIT, ACK_CHECK_EN);
            i2c_master_stop(cmd);
            ret = i2c_master_cmd_begin(i2c_port, cmd, 50 / portTICK_RATE_MS);
            i2c_cmd_link_delete(cmd);
            if (ret == ESP_OK) {
                printf("%02x ", address);
                matches[++last_match-1] = address;
            } else if (ret == ESP_ERR_TIMEOUT) {
                printf("UU ");
            } else {
                printf("-- ");
            }
        }

        printf("\r\n");

    }
    if(last_match) {
    	printf("\r\n------------------------------------------------------------------------------------"
    		   "\r\nDetected the following devices (names provided by https://i2cdevices.org/addresses).");

		for(int i=0;i<last_match;i++){
			//printf("%02x = %s\r\n", matches[i], i2c_get_description(matches[i]));
			printf("\r\n%u [%02xh]- %s", matches[i], matches[i], i2c_get_description(matches[i]));
		}
		printf("\r\n------------------------------------------------------------------------------------\r\n");
    }

    return 0;
}
static void register_i2c_set_display(){
	i2cdisp_args.address = arg_int0("a", "address", "<n>", "Set the device address, default 60");
	i2cdisp_args.width = arg_int0("w", "width", "<n>", "Set the display width");
	i2cdisp_args.height = arg_int0("h", "height", "<n>", "Set the display height");
	i2cdisp_args.name = arg_str0("t", "type", "<I2C|SPI>", "Set the display type. default I2C");
	i2cdisp_args.driver = arg_str0("d", "driver", "<string>", "Set the display driver name. Default SSD1306");
	i2cdisp_args.clear = arg_litn(NULL, "clear", 0, 1, "clear configuration and return");
	i2cdisp_args.hflip = arg_litn(NULL, "hf", 0, 1, "Flip picture horizontally");
	i2cdisp_args.vflip = arg_litn(NULL, "vf", 0, 1, "Flip picture vertically");
	i2cdisp_args.rotate = arg_litn("r", "rotate", 0, 1, "Rotate the picture 180 deg");
	i2cdisp_args.end = arg_end(8);
	const esp_console_cmd_t i2c_set_display= {
	 		.command = "setdisplay",
			.help="Sets the display options for the board",
			.hint = NULL,
			.func = &do_i2c_set_display,
			.argtable = &i2cdisp_args
	};

	const esp_console_cmd_t i2c_show_display= {
			.command = "getdisplay",
			.help="Shows display options and global i2c configuration",
			.hint = NULL,
			.func = &do_i2c_show_display,
			.argtable = NULL
	};
	cmd_to_json(&i2c_set_display);
	cmd_to_json(&i2c_show_display);
	ESP_ERROR_CHECK(esp_console_cmd_register(&i2c_set_display));
	ESP_ERROR_CHECK(esp_console_cmd_register(&i2c_show_display));
}
static void register_i2cdectect(void)
{
    const esp_console_cmd_t i2cdetect_cmd = {
        .command = "i2cdetect",
        .help = "Scan I2C bus for devices",
        .hint = NULL,
        .func = &do_i2cdetect_cmd,
        .argtable = NULL
    };
    cmd_to_json(&i2cdetect_cmd);
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cdetect_cmd));
}

static void register_i2cget(void)
{
    i2cget_args.chip_address = arg_int1("c", "chip", "<chip_addr>", "Specify the address of the chip on that bus");
    i2cget_args.register_address = arg_int0("r", "register", "<register_addr>", "Specify the address on that chip to read from");
    i2cget_args.data_length = arg_int0("l", "length", "<length>", "Specify the length to read from that data address");
    i2cget_args.end = arg_end(1);
    const esp_console_cmd_t i2cget_cmd = {
        .command = "i2cget",
        .help = "Read registers visible through the I2C bus",
        .hint = NULL,
        .func = &do_i2cget_cmd,
        .argtable = &i2cget_args
    };
    cmd_to_json(&i2cget_cmd);
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cget_cmd));
}


static void register_i2cset(void)
{
    i2cset_args.chip_address = arg_int1("c", "chip", "<chip_addr>", "Specify the address of the chip on that bus");
    i2cset_args.register_address = arg_int0("r", "register", "<register_addr>", "Specify the address on that chip to read from");
    i2cset_args.data = arg_intn(NULL, NULL, "<data>", 0, 256, "Specify the data to write to that data address");
    i2cset_args.end = arg_end(2);
    const esp_console_cmd_t i2cset_cmd = {
        .command = "i2cset",
        .help = "Set registers visible through the I2C bus",
        .hint = NULL,
        .func = &do_i2cset_cmd,
        .argtable = &i2cset_args
    };
    cmd_to_json(&i2cset_cmd);
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cset_cmd));
}



static void register_i2cdump(void)
{
    i2cdump_args.chip_address = arg_int1("c", "chip", "<chip_addr>", "Specify the address of the chip on that bus");
    i2cdump_args.size = arg_int0("s", "size", "<size>", "Specify the size of each read");
    i2cdump_args.end = arg_end(3);
    const esp_console_cmd_t i2cdump_cmd = {
        .command = "i2cdump",
        .help = "Examine registers visible through the I2C bus",
        .hint = NULL,
        .func = &do_i2cdump_cmd,
        .argtable = &i2cdump_args
    };
    cmd_to_json(&i2cdump_cmd);
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cdump_cmd));
}

static void register_i2ccheck(){
	i2ccheck_args.port = arg_int0("p", "port", "<0|1>", "Set the I2C bus port number");
	i2ccheck_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "i2ccheck",
        .help = "Check if the I2C bus is installed",
        .hint = NULL,
        .func = &do_i2c_check,
        .argtable = &i2ccheck_args
    };
    cmd_to_json(&cmd);
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}


static void register_i2cstop(){
	i2cstop_args.port = arg_int0("p", "port", "<0|1>", "Set the I2C bus port number");
	i2cstop_args.end = arg_end(2);

    const esp_console_cmd_t i2cconfig_cmd = {
        .command = "i2cstop",
        .help = "Stop the I2C bus",
        .hint = NULL,
        .func = &do_i2c_stop,
        .argtable = &i2cstop_args
    };
    cmd_to_json(&i2cconfig_cmd);
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cconfig_cmd));
}


static void register_i2cconfig(void)
{
    i2cconfig_args.port = arg_int0("p", "port", "<0|1>", "Set the I2C bus port number");
    i2cconfig_args.freq = arg_int0("f", "freq", "<Hz>", "Set the frequency(Hz) of I2C bus. e.g. 100000");
    i2cconfig_args.sda = arg_int0("d", "sda", "<gpio>", "Set the gpio for I2C SDA. e.g. 19");
    i2cconfig_args.scl = arg_int0("c", "scl", "<gpio>", "Set the gpio for I2C SCL. e.g. 18");
    i2cconfig_args.load = arg_litn("l", "load", 0, 1, "load existing configuration and return");
    i2cconfig_args.end = arg_end(4);
    const esp_console_cmd_t i2cconfig_cmd = {
        .command = "i2cconfig",
        .help = "Config I2C bus",
        .hint = NULL,
        .func = &do_i2cconfig_cmd,
        .argtable = &i2cconfig_args
    };
    cmd_to_json(&i2cconfig_cmd);
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cconfig_cmd));
}

void register_i2ctools(void)
{
    register_i2cconfig();
    register_i2cdectect();
    register_i2cget();
    register_i2cset();
    register_i2cdump();
    register_i2c_set_display();
    register_i2cstop();
    register_i2ccheck();
}
