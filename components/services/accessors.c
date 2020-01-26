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
#include "accessors.h"
#include "globdefs.h"

static const char *TAG = "services";

#define min(a,b) (((a) < (b)) ? (a) : (b))

/****************************************************************************************
 * 
 */
esp_err_t config_i2c_set(const i2c_config_t * config, int port){
	int buffer_size=255;
	char * config_buffer=calloc(buffer_size,1);
	if(config_buffer)  {
		snprintf(config_buffer,buffer_size,"scl=%u sda=%u speed=%u port=%u",config->scl_io_num,config->sda_io_num,config->master.clk_speed,port);
		ESP_LOGI(TAG,"Updating i2c configuration to %s",config_buffer);
		config_set_value(NVS_TYPE_STR, "i2c_config", config_buffer);
		free(config_buffer);
	}
	return ESP_OK;
}

/****************************************************************************************
 * 
 */
const i2c_config_t * config_i2c_get(int * i2c_port) {
	char *nvs_item, *p;
	static i2c_config_t i2c = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = -1,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_io_num = -1,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 400000,
	};

	nvs_item = config_alloc_get(NVS_TYPE_STR, "i2c_config");
	if (nvs_item) {
		if ((p = strcasestr(nvs_item, "scl")) != NULL) i2c.scl_io_num = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(nvs_item, "sda")) != NULL) i2c.sda_io_num = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(nvs_item, "speed")) != NULL) i2c.master.clk_speed = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(nvs_item, "port")) != NULL) i2c_system_port = atoi(strchr(p, '=') + 1);
		free(nvs_item);
	}
	if(i2c_port) *i2c_port=i2c_system_port;
	return &i2c;
}

/****************************************************************************************
 * 
 */
char *config_metadata_format(char *artist, char *album, char *title, int *speed, int len) {
	char *nvs_item, *string = calloc(len + 1, 1), *p;
	
	nvs_item = config_alloc_get(NVS_TYPE_STR, "metadata_config");
	
	if (nvs_item) {
		if ((p = strcasestr(nvs_item, "format")) != NULL) {
			char token[16], *q;
			int space = len;
			bool skip = false;
			
			p = strchr(nvs_item, '=');
			
			while (p++) {
				// find token and copy what's after when reaching last one
				if (sscanf(p, "%*[^%%]%%%[^%]%%", token) < 0) {
					q = strchr(p, ',');
					strncat(string, p, q ? min(q - p, space) : space);
					break;
				}

				// copy what's before token (be safe)
				if ((q = strchr(p, '%')) == NULL) break;
				
				// skip whatever is after a token if this token is empty
				if (!skip) {
					strncat(string, p, min(q - p, space));
					space = len - strlen(string);
				}	

				// then copy token's content
				if (strcasestr(q, "artist") && artist) strncat(string, p = artist, space);
				else if (strcasestr(q, "album") && album) strncat(string, p = album, space);
				else if (strcasestr(q, "title") && title) strncat(string, p = title, space);
				space = len - strlen(string);
				
				// flag to skip the data following an empty field
				if (*p) skip = false;
				else skip = true;

				// advance to next separator
				p = strchr(q + 1, '%');
			}
		} else {
			string = strdup(title ? title : "");
		}
		// get optional scroll speed
		if ((p = strcasestr(nvs_item, "speed")) != NULL) sscanf(p, "%*[^=]=%d", speed);
		else *speed = 0;
	}
	
	return string;
}	
