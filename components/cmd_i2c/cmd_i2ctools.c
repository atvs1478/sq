/* cmd_i2ctools.c

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "cmd_i2ctools.h"
#include "argtable3/argtable3.h"
#include "driver/i2c.h"
#include "esp_console.h"
#include "esp_log.h"
#include "string.h"
#include "stdio.h"
#include "config.h"

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
#ifdef CONFIG_SQUEEZEAMP
static i2c_port_t i2c_port = I2C_NUM_1;
#else
static i2c_port_t i2c_port = I2C_NUM_0;
#endif

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


	if((err=i2c_driver_install(i2c_port, I2C_MODE_MASTER, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0))!=ESP_OK){
		ESP_LOGW(TAG,"i2c driver was already installed.  Deleting it.");
		i2c_driver_delete(i2c_port);
		ESP_LOGW(TAG,"Installing i2c driver on port %u",i2c_port);
		if((err=i2c_driver_install(i2c_port, I2C_MODE_MASTER, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0))!=ESP_OK){
			ESP_LOGE(TAG,"Driver install failed. %s", esp_err_to_name(err));
		}
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
    ESP_LOGI(TAG,"Initializing i2c driver.\n   mode = I2C_MODE_MASTER, \n   scl_pullup_en = GPIO_PULLUP_ENABLE, \n   i2c port = %u, \n   sda_io_num = %u, \n   sda_pullup_en = GPIO_PULLUP_ENABLE, \n   scl_io_num = %u, \n   scl_pullup_en = GPIO_PULLUP_ENABLE, \n   master.clk_speed = %u", i2c_port, i2c_gpio_sda,i2c_gpio_scl,i2c_frequency);
    if((err=i2c_param_config(i2c_port, &conf))!=ESP_OK){
    	ESP_LOGE(TAG,"i2c driver initialized failed. %s", esp_err_to_name(err));
    }
    return err;
}

static struct {
	struct arg_lit *load;
    struct arg_int *port;
    struct arg_int *freq;
    struct arg_int *sda;
    struct arg_int *scl;
    struct arg_end *end;
} i2cconfig_args;

static struct {
	struct arg_lit *clear;
	struct arg_int *address;
	struct arg_int *width;
	struct arg_int *height;
	struct arg_str *name;
	struct arg_str *driver;
	struct arg_end *end;
} i2cdisp_args;

static struct {
	struct arg_end *end;
} i2cdisp_show_args;



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
	int width=0, height=0, address=120;
	char * name = strdup("I2S");
	char * driver= strdup("SSD136");
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
		free(name);
		name=strdup(i2cdisp_args.name->sval[0]);
	}

	/* Check "--driver" option */
	if (i2cdisp_args.driver->count) {
		free(driver);
		driver=strdup(i2cdisp_args.driver->sval[0]);
	}
	snprintf(config_string, sizeof(config_string),"%s:width=%i,height=%i,address=%i,driver=%s",name,width,height,address,driver );
	free(name);
	free(driver);

	if(nerrors!=0){
		return 0;
	}

	ESP_LOGI(TAG,"Updating display configuration string configuration to :\n"
			"display_config = \"%s\"",config_string );

	return config_set_value(NVS_TYPE_STR, "display_config", config_string)!=ESP_OK;
}

static void register_i2c_set_display(){
	i2cdisp_args.address = arg_int0(NULL, "address", "<n>", "Set the I2C bus port number (decimal format, default 120)");
	i2cdisp_args.width = arg_int0("w", "width", "<n>", "Set the display width");
	i2cdisp_args.height = arg_int0("h", "height", "<n>", "Set the display height");
	i2cdisp_args.name = arg_str0("n", "name", "<string>", "Set the display type. Default is I2S");
	i2cdisp_args.driver = arg_str0("d", "driver", "<string>", "Set the display driver name");
	i2cdisp_args.clear = arg_litn("c", "clear", 0, 1, "clear configuration and return");
	i2cdisp_args.end = arg_end(2);
	i2cdisp_show_args.end = arg_end(1);
	const esp_console_cmd_t i2c_set_display= {
	 		.command = "setdisplay",
			.help="Sets the display options for the board",
			.hint = NULL,
			.func = &do_i2c_set_display,
			.argtable = &i2cdisp_args
	};
	const esp_console_cmd_t i2c_show_display= {
			.command = "show_display",
			.help="Shows the board display options and i2c configuration",
			.hint = NULL,
			.func = &do_i2c_show_display,
			.argtable = &i2cdisp_show_args
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&i2c_set_display));
	ESP_ERROR_CHECK(esp_console_cmd_register(&i2c_show_display));
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
    	ESP_LOGW(TAG,"Clearing display config");
    	char * nvs_item = config_alloc_get(NVS_TYPE_STR, "i2c_config");
    	if (nvs_item) {
    		char * p=NULL;
    		ESP_LOGI(TAG,"Loading I2C configuration : %s", nvs_item);
    		if ((p = strcasestr(nvs_item, "scl")) != NULL) i2c_gpio_scl = atoi(strchr(p, '=') + 1);
    		if ((p = strcasestr(nvs_item, "sda")) != NULL) i2c_gpio_sda = atoi(strchr(p, '=') + 1);
    		if ((p = strcasestr(nvs_item, "speed")) != NULL) i2c_frequency = atoi(strchr(p, '=') + 1);
    		if ((p = strcasestr(nvs_item, "port")) != NULL) i2c_port = atoi(strchr(p, '=') + 1);

    		free(nvs_item);
    	}

    }

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


#ifdef CONFIG_SQUEEZEAMP
	if (i2c_port == I2C_NUM_0) {
		i2c_port = I2C_NUM_1;
		ESP_LOGE(TAG, "can't use i2c port 0 on SqueezeAMP. Changing to port 1.");
	}
#endif
	if(!res){
		int buffer_size=255;
		char * config_buffer=malloc(buffer_size);
		memset(config_buffer,0x00,buffer_size);
		snprintf(config_buffer,buffer_size,"scl=%u sda=%u speed=%u port=%u",i2c_gpio_scl,i2c_gpio_sda,i2c_frequency,i2c_port);

		ESP_LOGI(TAG,"Initializing driver with config %s", config_buffer);
		if((err=i2c_master_driver_initialize())==ESP_OK){
			ESP_LOGI(TAG,"Initalize success. Storing config to nvs");
			config_set_value(NVS_TYPE_STR, "i2c_config", config_buffer);
			ESP_LOGI(TAG,"Starting the i2s driver.");
			// now start the i2c driver
			if((err=i2c_master_driver_install())!=ESP_OK){
				 res=1;
			}
		}
		else {
			ESP_LOGE(TAG,"I2C initialization failed. %s", esp_err_to_name(err));
			res=1;
		}
		if(config_buffer) free(config_buffer);
	}
    return res;
}

static void register_i2cconfig(void)
{
    i2cconfig_args.port = arg_int1("p", "port", "<0|1>", "Set the I2C bus port number");
    i2cconfig_args.freq = arg_int1("f", "freq", "<Hz>", "Set the frequency(Hz) of I2C bus. e.g. 100000");
    i2cconfig_args.sda = arg_int1("d", "sda", "<gpio>", "Set the gpio for I2C SDA. e.g. 19");
    i2cconfig_args.scl = arg_int1("c", "scl", "<gpio>", "Set the gpio for I2C SCL. e.g. 18");
    i2cconfig_args.load = arg_litn("l", "load", 0, 1, "load existing configuration and return");
    i2cconfig_args.end = arg_end(2);
    const esp_console_cmd_t i2cconfig_cmd = {
        .command = "i2cconfig",
        .help = "Config I2C bus",
        .hint = NULL,
        .func = &do_i2cconfig_cmd,
        .argtable = &i2cconfig_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cconfig_cmd));
}
#define RUN_SHOW_ERROR(c)

static int do_i2cdetect_cmd(int argc, char **argv)
{
	esp_err_t err=ESP_OK;

    if((err=i2c_master_driver_initialize())!=ESP_OK){
    	ESP_LOGE(TAG,"Error initializing i2c driver. %s",esp_err_to_name(err));
    	return 1;
    }
	if((err=i2c_master_driver_install())!=ESP_OK){
		 return 1;
	}

    uint8_t address;
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    for (int i = 0; i < 128; i += 16) {
        printf("%02x: ", i);
        for (int j = 0; j < 16; j++) {
            fflush(stdout);
            address = i + j;
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (address << 1) | WRITE_BIT, ACK_CHECK_EN);
            i2c_master_stop(cmd);
            esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, 50 / portTICK_RATE_MS);
            i2c_cmd_link_delete(cmd);
            if (ret == ESP_OK) {
                printf("%02x ", address);
            } else if (ret == ESP_ERR_TIMEOUT) {
                printf("UU ");
            } else {
                printf("-- ");
            }
        }
        printf("\r\n");
    }

    i2c_driver_delete(i2c_port);
    return 0;
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
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cdetect_cmd));
}

static struct {
    struct arg_int *chip_address;
    struct arg_int *register_address;
    struct arg_int *data_length;
    struct arg_end *end;
} i2cget_args;

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
    i2c_driver_delete(i2c_port);
    return 0;
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
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cget_cmd));
}

static struct {
    struct arg_int *chip_address;
    struct arg_int *register_address;
    struct arg_int *data;
    struct arg_end *end;
} i2cset_args;

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
    i2c_driver_delete(i2c_port);
    return 0;
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
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cset_cmd));
}

static struct {
    struct arg_int *chip_address;
    struct arg_int *size;
    struct arg_end *end;
} i2cdump_args;

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
    i2c_master_driver_initialize();
	if(i2c_master_driver_install()!=ESP_OK){
		 return 1;
	}

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
    i2c_driver_delete(i2c_port);
    return 0;
}

static void register_i2cdump(void)
{
    i2cdump_args.chip_address = arg_int1("c", "chip", "<chip_addr>", "Specify the address of the chip on that bus");
    i2cdump_args.size = arg_int0("s", "size", "<size>", "Specify the size of each read");
    i2cdump_args.end = arg_end(1);
    const esp_console_cmd_t i2cdump_cmd = {
        .command = "i2cdump",
        .help = "Examine registers visible through the I2C bus",
        .hint = NULL,
        .func = &do_i2cdump_cmd,
        .argtable = &i2cdump_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2cdump_cmd));
}

void register_i2ctools(void)
{
    register_i2cconfig();
    register_i2cdectect();
    register_i2cget();
    register_i2cset();
    register_i2cdump();
    register_i2c_set_display();
}
