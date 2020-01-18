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
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "platform_esp32.h"
#include "led.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "nvs_utilities.h"
#include "http_server.h"
#include "trace.h"
#include "wifi_manager.h"
#include "squeezelite-ota.h"
#include <math.h>
#include "config.h"
#include "audio_controls.h"

static const char certs_namespace[] = "certificates";
static const char certs_key[] = "blob";
static const char certs_version[] = "version";

EventGroupHandle_t wifi_event_group;

bool bypass_wifi_manager=false;
const int CONNECTED_BIT = BIT0;
#define JOIN_TIMEOUT_MS (10000)
#define LOCAL_MAC_SIZE 20
static const char TAG[] = "esp_app_main";
#define DEFAULT_HOST_NAME "squeezelite"
char * fwurl = NULL;

static bool bWifiConnected=false;
extern const uint8_t server_cert_pem_start[] asm("_binary_github_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_github_pem_end");

// as an exception _init function don't need include
extern void services_init(void);
extern void	display_init(char *welcome);

/* brief this is an exemple of a callback that you can setup in your own app to get notified of wifi manager event */
void cb_connection_got_ip(void *pvParameter){
	ESP_LOGI(TAG, "I have a connection!");
	xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
	bWifiConnected=true;
	led_unpush(LED_GREEN);
}
void cb_connection_sta_disconnected(void *pvParameter){
	led_blink_pushed(LED_GREEN, 250, 250);
	bWifiConnected=false;
	xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
}
bool wait_for_wifi(){
	bool connected=(xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT)!=0;
	if(!connected){
		ESP_LOGD(TAG,"Waiting for WiFi...");
	    connected = (xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
	                                   pdFALSE, pdTRUE, JOIN_TIMEOUT_MS / portTICK_PERIOD_MS)& CONNECTED_BIT)!=0;
	    if(!connected){
	    	ESP_LOGW(TAG,"wifi timeout.");
	    }
	    else
	    {
	    	ESP_LOGI(TAG,"WiFi Connected!");
	    }
	}
    return connected;
}

char * process_ota_url(){
    ESP_LOGI(TAG,"Checking for update url");
    char * fwurl=config_alloc_get(NVS_TYPE_STR, "fwurl");
	if(fwurl!=NULL)
	{
		ESP_LOGD(TAG,"Deleting nvs entry for Firmware URL %s", fwurl);
		config_delete_key("fwurl");
	}
	return fwurl;
}

esp_log_level_t  get_log_level_from_char(char * level){
	if(!strcasecmp(level, "NONE"    )) { return ESP_LOG_NONE  ;}
	if(!strcasecmp(level, "ERROR"   )) { return ESP_LOG_ERROR ;}
	if(!strcasecmp(level, "WARN"    )) { return ESP_LOG_WARN  ;}
	if(!strcasecmp(level, "INFO"    )) { return ESP_LOG_INFO  ;}
	if(!strcasecmp(level, "DEBUG"   )) { return ESP_LOG_DEBUG ;}
	if(!strcasecmp(level, "VERBOSE" )) { return ESP_LOG_VERBOSE;}
	return ESP_LOG_WARN;
}
void set_log_level(char * tag, char * level){
	esp_log_level_set(tag, get_log_level_from_char(level));
}
esp_err_t update_certificates(){
//	server_cert_pem_start
//	server_cert_pem_end

	nvs_handle handle;
	esp_err_t esp_err;
    esp_app_desc_t running_app_info;

	ESP_LOGI(TAG,   "About to check if certificates need to be updated in flash");
	esp_err = nvs_open_from_partition(settings_partition, certs_namespace, NVS_READWRITE, &handle);
	if (esp_err != ESP_OK) {
		ESP_LOGE(TAG,  "Unable to open name namespace %s. Error %s", certs_namespace, esp_err_to_name(esp_err));
		return esp_err;
	}

	const esp_partition_t *running = esp_ota_get_running_partition();
	if(running->subtype !=ESP_PARTITION_SUBTYPE_APP_FACTORY ){
		ESP_LOGI(TAG, "Running partition [%s] type %d subtype %d (offset 0x%08x)", running->label, running->type, running->subtype, running->address);

	}

	if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
		ESP_LOGI(TAG, "Running version: %s", running_app_info.version);
	}


	size_t len=0;
	char *str=NULL;
	bool changed=false;
	if ( (esp_err= nvs_get_str(handle, certs_version, NULL, &len)) == ESP_OK) {
		str=(char *)malloc(len);
		if ( (esp_err = nvs_get_str(handle,  certs_version, str, &len)) == ESP_OK) {
			printf("String associated with key '%s' is %s \n", certs_version, str);
		}
	}
	if(str!=NULL){
		if(strcmp((char *)running_app_info.version,(char *)str )){
			// Versions are different
			ESP_LOGW(TAG,"Found a different software version. Updating certificates");
			changed=true;
		}
		free(str);
	}
	else {
		ESP_LOGW(TAG,"No certificate found. Adding certificates");
		changed=true;
	}

	if(changed){

		esp_err = nvs_set_blob(handle, certs_key, server_cert_pem_start, (server_cert_pem_end-server_cert_pem_start));
		if(esp_err!=ESP_OK){
			ESP_LOGE(TAG, "Failed to store certificate data: %s", esp_err_to_name(esp_err));
		}
		else {
			ESP_LOGI(TAG,"Updated stored https certificates");
			esp_err = nvs_set_str(handle,  certs_version, running_app_info.version);
			if(esp_err!=ESP_OK){
				ESP_LOGE(TAG, "Failed to store app version: %s", esp_err_to_name(esp_err));
			}
			else {
				esp_err = nvs_commit(handle);
				if(esp_err!=ESP_OK){
					ESP_LOGE(TAG, "Failed to commit certificate changes: %s", esp_err_to_name(esp_err));
				}
			}
		}
	}

	nvs_close(handle);
	return ESP_OK;
}
const char * get_certificate(){
	nvs_handle handle;
	esp_err_t esp_err;
	char *blob =NULL;
//
	ESP_LOGD(TAG,  "Fetching certificate.");
	esp_err = nvs_open_from_partition(settings_partition, certs_namespace, NVS_READONLY, &handle);
	if(esp_err == ESP_OK){
        size_t len;
        esp_err = nvs_get_blob(handle, certs_key, NULL, &len);
        if( esp_err == ESP_OK) {
            blob = (char *)malloc(len);
            esp_err = nvs_get_blob(handle, certs_key, blob, &len);
            if ( esp_err  == ESP_OK) {
                printf("Blob associated with key '%s' is %d bytes long: \n", certs_key, len);
            }
        }
        else{
        	ESP_LOGE(TAG,  "Unable to get the existing blob from namespace %s. [%s]", certs_namespace, esp_err_to_name(esp_err));
        }
        nvs_close(handle);
	}
	else{
		ESP_LOGE(TAG,  "Unable to open name namespace %s. [%s]", certs_namespace, esp_err_to_name(esp_err));
	}
	return blob;
}

#define DEFAULT_NAME_WITH_MAC(var,defval) char var[strlen(defval)+sizeof(macStr)]; strcpy(var,defval); strcat(var,macStr)
void register_default_nvs(){
	uint8_t mac[6];
	char macStr[LOCAL_MAC_SIZE+1];
	char default_command_line[strlen(CONFIG_DEFAULT_COMMAND_LINE)+sizeof(macStr)];

	esp_read_mac((uint8_t *)&mac, ESP_MAC_WIFI_STA);
	snprintf(macStr, LOCAL_MAC_SIZE-1,"-%x%x%x", mac[3], mac[4], mac[5]);

	DEFAULT_NAME_WITH_MAC(default_bt_name,CONFIG_BT_NAME);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "bt_name", default_bt_name);
	config_set_default(NVS_TYPE_STR, "bt_name", default_bt_name, 0);

	DEFAULT_NAME_WITH_MAC(default_host_name,DEFAULT_HOST_NAME);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "host_name", default_host_name);
	config_set_default(NVS_TYPE_STR, "host_name", default_host_name, 0);

	DEFAULT_NAME_WITH_MAC(default_airplay_name,CONFIG_AIRPLAY_NAME);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "airplay_name",default_airplay_name);
	config_set_default(NVS_TYPE_STR, "airplay_name",default_airplay_name , 0);

	DEFAULT_NAME_WITH_MAC(default_ap_name,CONFIG_DEFAULT_AP_SSID);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "ap_ssid", default_ap_name);
	config_set_default(NVS_TYPE_STR, "ap_ssid",default_ap_name , 0);

	strncpy(default_command_line, CONFIG_DEFAULT_COMMAND_LINE,sizeof(default_command_line)-1);
	strncat(default_command_line, " -n ",sizeof(default_command_line)-1);
	strncat(default_command_line, default_host_name,sizeof(default_command_line)-1);

	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "autoexec", "1");
	config_set_default(NVS_TYPE_STR,"autoexec","1", 0);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "autoexec1",default_command_line);
	config_set_default(NVS_TYPE_STR,"autoexec1",default_command_line,0);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "a2dp_sink_name", CONFIG_A2DP_SINK_NAME);
	config_set_default(NVS_TYPE_STR, "a2dp_sink_name", CONFIG_A2DP_SINK_NAME, 0);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "bt_sink_pin", STR(CONFIG_BT_SINK_PIN));
	config_set_default(NVS_TYPE_STR, "bt_sink_pin", STR(CONFIG_BT_SINK_PIN), 0);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "release_url", SQUEEZELITE_ESP32_RELEASE_URL);
	config_set_default(NVS_TYPE_STR, "release_url", SQUEEZELITE_ESP32_RELEASE_URL, 0);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s","ap_ip_address",CONFIG_DEFAULT_AP_IP );
	config_set_default(NVS_TYPE_STR, "ap_ip_address",CONFIG_DEFAULT_AP_IP , 0);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "ap_ip_gateway",CONFIG_DEFAULT_AP_GATEWAY );
	config_set_default(NVS_TYPE_STR, "ap_ip_gateway",CONFIG_DEFAULT_AP_GATEWAY , 0);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s","ap_ip_netmask",CONFIG_DEFAULT_AP_NETMASK );
	config_set_default(NVS_TYPE_STR, "ap_ip_netmask",CONFIG_DEFAULT_AP_NETMASK , 0);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "ap_channel",STR(CONFIG_DEFAULT_AP_CHANNEL));
	config_set_default(NVS_TYPE_STR, "ap_channel",STR(CONFIG_DEFAULT_AP_CHANNEL) , 0);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "ap_pwd", CONFIG_DEFAULT_AP_PASSWORD);
	config_set_default(NVS_TYPE_STR, "ap_pwd", CONFIG_DEFAULT_AP_PASSWORD, 0);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "airplay_port", CONFIG_AIRPLAY_PORT);
	config_set_default(NVS_TYPE_STR, "airplay_port", CONFIG_AIRPLAY_PORT, 0);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "a2dp_dev_name", CONFIG_A2DP_DEV_NAME);
	config_set_default(NVS_TYPE_STR, "a2dp_dev_name", CONFIG_A2DP_DEV_NAME, 0);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "bypass_wm", "0");
	config_set_default(NVS_TYPE_STR, "bypass_wm", "0", 0);

	ESP_LOGD(TAG,"Registering default Audio control board type %s, value ","actrls_config");
	config_set_default(NVS_TYPE_STR, "actrls_config", "", 0);

	char number_buffer[101] = {};
	snprintf(number_buffer,sizeof(number_buffer)-1,"%u",OTA_FLASH_ERASE_BLOCK);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "ota_erase_blk", number_buffer);
	config_set_default(NVS_TYPE_STR, "ota_erase_blk", number_buffer, 0);

	snprintf(number_buffer,sizeof(number_buffer)-1,"%u",OTA_STACK_SIZE);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "ota_stack", number_buffer);
	config_set_default(NVS_TYPE_STR, "ota_stack", number_buffer, 0);

	snprintf(number_buffer,sizeof(number_buffer)-1,"%d",OTA_TASK_PRIOTITY);
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "ota_prio", number_buffer);
	config_set_default(NVS_TYPE_STR, "ota_prio", number_buffer, 0);

	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "enable_bt_sink", STR(CONFIG_BT_SINK));
	config_set_default(NVS_TYPE_STR, "enable_bt_sink", STR(CONFIG_BT_SINK), 0);
	
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "enable_airplay", STR(CONFIG_AIRPLAY_SINK));
	config_set_default(NVS_TYPE_STR, "enable_airplay", STR(CONFIG_AIRPLAY_SINK), 0);

	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "display_config", STR(CONFIG_DISPLAY_CONFIG));
	config_set_default(NVS_TYPE_STR, "display_config", STR(CONFIG_DISPLAY_CONFIG), 0);
	
	ESP_LOGD(TAG,"Registering default value for key %s", "i2c_config");
	config_set_default(NVS_TYPE_STR, "i2c_config", "", 0);
	
	ESP_LOGD(TAG,"Done setting default values in nvs.");
}

void app_main()
{
	char * fwurl = NULL;
	esp_err_t update_certificates();
	ESP_LOGD(TAG,"Creating event group for wifi");
	wifi_event_group = xEventGroupCreate();
	ESP_LOGD(TAG,"Clearing CONNECTED_BIT from wifi group");
	xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
	
	ESP_LOGI(TAG,"Starting app_main");
	initialize_nvs();

	ESP_LOGI(TAG,"Setting up config subsystem.");
	config_init();

	ESP_LOGI(TAG,"Registering default values");
	register_default_nvs();

	ESP_LOGD(TAG,"Configuring services");
	services_init();

	ESP_LOGD(TAG,"Initializing display");	
	display_init("SqueezeESP32");

#if !RECOVERY_APPLICATION
	ESP_LOGI(TAG,"Checking if certificates need to be updated");
	update_certificates();
#endif

	ESP_LOGD(TAG,"Getting firmware OTA URL (if any)");
	fwurl = process_ota_url();

	ESP_LOGD(TAG,"Getting value for WM bypass, nvs 'bypass_wm'");
	char * bypass_wm = config_alloc_get_default(NVS_TYPE_STR, "bypass_wm", "0", 0);
	if(bypass_wm==NULL)
	{
		ESP_LOGE(TAG, "Unable to retrieve the Wifi Manager bypass flag");
		bypass_wifi_manager = false;
	}
	else {
		bypass_wifi_manager=(strcmp(bypass_wm,"1")==0 ||strcasecmp(bypass_wm,"y")==0);
	}

	ESP_LOGD(TAG,"Getting audio control mapping ");
	char *actrls_config = config_alloc_get_default(NVS_TYPE_STR, "actrls_config", NULL, 0);
	if (actrls_config) {
		if(actrls_config[0] !='\0'){
			ESP_LOGD(TAG,"Initializing audio control buttons type %s", actrls_config);
			actrls_init_json(actrls_config, true);
		}
		free(actrls_config);
	} else {
		ESP_LOGD(TAG,"No audio control buttons");
	}

	/* start the wifi manager */
	ESP_LOGD(TAG,"Blinking led");
	led_blink(LED_GREEN, 250, 250);

	if(bypass_wifi_manager){
		ESP_LOGW(TAG,"\n\nwifi manager is disabled. Please use wifi commands to connect to your wifi access point.\n\n");
	}
	else {
		ESP_LOGW(TAG,"\n\nwifi manager is ENABLED. Starting...\n\n");
		wifi_manager_start();
		wifi_manager_set_callback(EVENT_STA_GOT_IP, &cb_connection_got_ip);
		wifi_manager_set_callback(EVENT_STA_DISCONNECTED, &cb_connection_sta_disconnected);
	}
	console_start();
	if(fwurl && strlen(fwurl)>0){
#if RECOVERY_APPLICATION
		while(!bWifiConnected){
			wait_for_wifi();
			taskYIELD();
		}
		ESP_LOGI(TAG,"Updating firmware from link: %s",fwurl);
		start_ota(fwurl);
#else
		ESP_LOGE(TAG,"Restarted to application partition. We're not going to perform OTA!");
#endif
		free(fwurl);
	}
}
