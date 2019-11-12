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


EventGroupHandle_t wifi_event_group;
bool enable_bt_sink=false;
bool enable_airplay=false;
bool jack_mutes_amp=false;
bool bypass_wifi_manager=false;
const int CONNECTED_BIT = BIT0;
#define JOIN_TIMEOUT_MS (10000)
#define LOCAL_MAC_SIZE 20
static const char TAG[] = "esp_app_main";
#define DEFAULT_HOST_NAME "squeezelite"
char * fwurl = NULL;

#ifdef CONFIG_SQUEEZEAMP
#define LED_GREEN_GPIO 	12
#define LED_RED_GPIO	13
#else
#define LED_GREEN_GPIO 	-1
#define LED_RED_GPIO	-1
#endif
static bool bWifiConnected=false;




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

//CONFIG_SDIF_NUM=0
//CONFIG_SPDIF_BCK_IO=26
//CONFIG_SPDIF_WS_IO=25
//CONFIG_SPDIF_DO_IO=15
//CONFIG_A2DP_CONTROL_DELAY_MS=500
//CONFIG_A2DP_CONNECT_TIMEOUT_MS=1000
//CONFIG_WIFI_MANAGER_MAX_RETRY=2
u16_t get_adjusted_volume(u16_t volume){

	char * str_factor = config_alloc_get_default(NVS_TYPE_STR, "volumefactor", "3", 0);
	if(str_factor != NULL ){

		float factor = atof(str_factor);
		free(str_factor);
		return (u16_t) (65536.0f * powf( (volume/ 128.0f), factor) );
	}
	else {
		ESP_LOGW(TAG,"Error retrieving volume factor.  Returning unmodified volume level. ");
		return volume;
	}
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
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "volumefactor", "3");
	config_set_default(NVS_TYPE_STR, "volumefactor", "3", 0);
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
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "enable_bt_sink", STR(CONFIG_BT_SINK));
	char * flag = config_alloc_get_default(NVS_TYPE_STR, "enable_bt_sink", STR(CONFIG_BT_SINK), 0);
	if(flag !=NULL){
		enable_bt_sink= (strcmp(flag,"1")==0 ||strcasecmp(flag,"y")==0);
		free(flag);
	}
	else {
		ESP_LOGE(TAG,"Unable to get flag 'enable_bt_sink'");
	}
	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "enable_airplay", STR(CONFIG_AIRPLAY_SINK));
	flag = config_alloc_get_default(NVS_TYPE_STR, "enable_airplay", STR(CONFIG_AIRPLAY_SINK), 0);
	if(flag !=NULL){
		enable_airplay= (strcmp(flag,"1")==0 ||strcasecmp(flag,"y")==0);
		free(flag);
	}
	else {
		ESP_LOGE(TAG,"Unable to get flag 'enable_airplay'");
	}


	ESP_LOGD(TAG,"Registering default value for key %s, value %s", "jack_mutes_amp", "n");
	flag = config_alloc_get_default(NVS_TYPE_STR, "jack_mutes_amp", "n", 0);

	if(flag !=NULL){
		jack_mutes_amp= (strcmp(flag,"1")==0 ||strcasecmp(flag,"y")==0);
		free(flag);
	}
	else {
		ESP_LOGE(TAG,"Unable to get flag 'jack_mutes_amp'");
	}
	ESP_LOGD(TAG,"Done setting default values in nvs.");
}

void app_main()
{
	char * fwurl = NULL;

	ESP_LOGD(TAG,"Creating event group for wifi");
	wifi_event_group = xEventGroupCreate();
	ESP_LOGD(TAG,"Clearing CONNECTED_BIT from wifi group");
	xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);

	ESP_LOGI(TAG,"Starting app_main");
	initialize_nvs();
	ESP_LOGI(TAG,"Setting up config subsystem.");
	config_init();

	ESP_LOGD(TAG,"Registering default values");
	register_default_nvs();

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

	ESP_LOGD(TAG,"Configuring Green led");

	led_config(LED_GREEN, LED_GREEN_GPIO, 0);
	ESP_LOGD(TAG,"Configuring Red led");
	led_config(LED_RED, LED_RED_GPIO, 0);

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
		wifi_manager_set_callback(WIFI_EVENT_STA_DISCONNECTED, &cb_connection_sta_disconnected);
	}
	console_start();
	if(fwurl && strlen(fwurl)>0){
#if RECOVERY_APPLICATION
		while(!bWifiConnected){
			wait_for_wifi();
			taskYIELD();
		}
		ESP_LOGI(TAG,"Updating firmware from link: %s",fwurl);
		start_ota(fwurl, true);
#else
		ESP_LOGE(TAG,"Restarted to application partition. We're not going to perform OTA!");
#endif
		free(fwurl);
	}
}
