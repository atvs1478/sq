/* OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "string.h"
#include <stdbool.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "cmd_system.h"
#include "esp_err.h"
#include "tcpip_adapter.h"
#include "squeezelite-ota.h"
#include "nvs_utilities.h"
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>

static const char *TAG = "squeezelite-ota";
extern const uint8_t server_cert_pem_start[] asm("_binary_github_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_github_pem_end");
extern bool wait_for_wifi();
extern char current_namespace[];
static struct {
	char status_text[31];
	uint32_t ota_actual_len;
	uint32_t ota_total_len;
	char * redirected_url;
	char * current_url;
	bool bRedirectFound;
	bool bOTAStarted;
	bool bInitialized;
	uint8_t lastpct;
	uint8_t newpct;
	struct timeval OTA_start;
	bool bOTAThreadStarted;

} ota_status;
struct timeval tv;
static esp_http_client_config_t ota_config;

extern void CODE_RAM_LOCATION wifi_manager_refresh_ota_json();

void CODE_RAM_LOCATION _printMemStats(){
	ESP_LOGI(TAG,"Heap internal:%zu (min:%zu) external:%zu (min:%zu)\n",
			heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
			heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
			heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
			heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
}
void triggerStatusJsonRefresh(const char * status, ...){
    va_list args;
    va_start(args, status);
    snprintf(ota_status.status_text,sizeof(ota_status.status_text)-1,status, args);
    va_end(args);
    _printMemStats();
	wifi_manager_refresh_ota_json();
}
const char * CODE_RAM_LOCATION ota_get_status(){
	if(!ota_status.bInitialized)
		{
			memset(ota_status.status_text, 0x00,sizeof(ota_status.status_text));
			ota_status.bInitialized = true;
		}
	return ota_status.status_text;
}
uint8_t CODE_RAM_LOCATION  ota_get_pct_complete(){
	return ota_status.ota_total_len==0?0:
			(uint8_t)((float)ota_status.ota_actual_len/(float)ota_status.ota_total_len*100.0f);
}

static void __attribute__((noreturn)) task_fatal_error(void)
{
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    (void)vTaskDelete(NULL);

    while (1) {
        ;
    }
}
#define FREE_RESET(p) if(p!=NULL) { free(p); p=NULL; }
esp_err_t CODE_RAM_LOCATION _http_event_handler(esp_http_client_event_t *evt)
{
// --------------
//	Received parameters
//
//	esp_http_client_event_id_tevent_id event_id, to know the cause of the event
//	esp_http_client_handle_t client
//	esp_http_client_handle_t context

//	void *data data of the event

//	int data_len - data length of data
//	void *user_data -- user_data context, from esp_http_client_config_t user_data

//	char *header_key For HTTP_EVENT_ON_HEADER event_id, it’s store current http header key
//	char *header_value For HTTP_EVENT_ON_HEADER event_id, it’s store current http header value
// --------------
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        _printMemStats();
        //strncpy(ota_status,"HTTP_EVENT_ERROR",sizeof(ota_status)-1);
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");

        if(ota_status.bOTAStarted) triggerStatusJsonRefresh("Installing...");
        ota_status.ota_total_len=0;
		ota_status.ota_actual_len=0;
		ota_status.lastpct=0;
		ota_status.newpct=0;
		gettimeofday(&ota_status.OTA_start, NULL);

			break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, status_code=%d, key=%s, value=%s",esp_http_client_get_status_code(evt->client),evt->header_key, evt->header_value);
		if (strcasecmp(evt->header_key, "location") == 0) {
			FREE_RESET(ota_status.redirected_url);
        	ota_status.redirected_url=strdup(evt->header_value);
        	ESP_LOGW(TAG,"OTA will redirect to url: %s",ota_status.redirected_url);
        	ota_status.bRedirectFound= true;
        }
        if (strcasecmp(evt->header_key, "content-length") == 0) {
        	ota_status.ota_total_len = atol(evt->header_value);
        	 ESP_LOGW(TAG, "Content length found: %s, parsed to %d", evt->header_value, ota_status.ota_total_len);
        }
        break;
    case HTTP_EVENT_ON_DATA:

    	vTaskDelay(5/ portTICK_RATE_MS);
    	if(!ota_status.bOTAStarted)  {
    		ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, status_code=%d, len=%d",esp_http_client_get_status_code(evt->client), evt->data_len);
    	}
    	else if(ota_status.bOTAStarted && esp_http_client_get_status_code(evt->client) == 200 ){
			ota_status.ota_actual_len+=evt->data_len;
			if(ota_get_pct_complete()%2 == 0) ota_status.newpct = ota_get_pct_complete();
			if(ota_status.lastpct!=ota_status.newpct )
			{
				gettimeofday(&tv, NULL);

				uint32_t elapsed_ms= (tv.tv_sec-ota_status.OTA_start.tv_sec )*1000+(tv.tv_usec-ota_status.OTA_start.tv_usec)/1000;

				wifi_manager_refresh_ota_json();
				ota_status.lastpct=ota_status.newpct;
				ESP_LOGI(TAG,"OTA chunk : %d bytes (%d of %d) (%d pct), %d KB/s", evt->data_len, ota_status.ota_actual_len, ota_status.ota_total_len, ota_status.newpct, elapsed_ms>0?ota_status.ota_actual_len*1000/elapsed_ms/1024:0);
				ESP_LOGD(TAG,"Heap internal:%zu (min:%zu) external:%zu (min:%zu)\n",
						heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
						heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
						heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
						heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
			}
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
		ESP_LOGD(TAG,"Heap internal:%zu (min:%zu) external:%zu (min:%zu)\n",
					heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
					heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
					heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
					heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

esp_err_t CODE_RAM_LOCATION init_config(esp_http_client_config_t * conf, const char * url){
	memset(conf, 0x00, sizeof(esp_http_client_config_t));
	conf->cert_pem = (char *)server_cert_pem_start;
	conf->event_handler = _http_event_handler;
	conf->buffer_size = 4096;
	conf->disable_auto_redirect=true;
	conf->skip_cert_common_name_check = false;
	conf->url = strdup(url);
	conf->max_redirection_count = 0;

	return ESP_OK;
}
void CODE_RAM_LOCATION ota_task(void *pvParameter)
{
	char * passedURL=(char *)pvParameter;

	ota_status.bInitialized = true;
	ESP_LOGD(TAG, "HTTP ota Thread started");
	triggerStatusJsonRefresh("Initializing...");
	ota_status.bRedirectFound=false;
	if(passedURL==NULL || strlen(passedURL)==0){
		ESP_LOGE(TAG,"HTTP OTA called without a url");
		ota_status.bOTAThreadStarted=false;
		vTaskDelete(NULL);
		return ;
	}
	ota_status.current_url= strdup(passedURL);
	FREE_RESET(pvParameter);
	ESP_LOGD(TAG,"Calling esp_https_ota");
	init_config(&ota_config,ota_status.bRedirectFound?ota_status.redirected_url:ota_status.current_url);
	ota_status.bOTAStarted = true;
	triggerStatusJsonRefresh("Starting OTA...");
	esp_err_t err = esp_https_ota(&ota_config);
    if (err == ESP_OK) {
    	triggerStatusJsonRefresh("Success!");
        esp_restart();
    } else {
    	triggerStatusJsonRefresh("Error: %s",esp_err_to_name(err));
    	wifi_manager_refresh_ota_json();
        ESP_LOGE(TAG, "Firmware upgrade failed with error : %s", esp_err_to_name(err));
        ota_status.bOTAThreadStarted=false;
    }
	FREE_RESET(ota_status.current_url);
	FREE_RESET(ota_status.redirected_url);

    vTaskDelete(NULL);
}

esp_err_t process_recovery_ota(const char * bin_url){
#if RECOVERY_APPLICATION
	// Initialize NVS.
	int ret=0;
    nvs_handle nvs;
    esp_err_t err = nvs_flash_init();

    if(ota_status.bOTAThreadStarted){
		ESP_LOGE(TAG,"OTA Already started. ");
		return ESP_FAIL;
	}
	memset(&ota_status, 0x00, sizeof(ota_status));
	ota_status.bOTAThreadStarted=true;
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    	// todo: If we ever change the size of the nvs partition, we need to figure out a mechanism to enlarge the nvs.
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
    	ESP_LOGW(TAG,"NVS flash size has changed. Formatting nvs");
    	ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    char * urlPtr=strdup(bin_url);
	// the first thing we need to do here is to erase the firmware url
	// to avoid a boot loop


#ifdef CONFIG_ESP32_WIFI_TASK_PINNED_TO_CORE_1
#define OTA_CORE 0
#warning "OTA will run on core 0"
#else
#warning "OTA will run on core 1"
#define OTA_CORE 1
#endif
    ESP_LOGI(TAG, "Starting ota on core %u for : %s", OTA_CORE,urlPtr);

    ret=xTaskCreatePinnedToCore(&ota_task, "ota_task", 1024*20, (void *)urlPtr, ESP_TASK_MAIN_PRIO+4, NULL, OTA_CORE);
    if (ret != pdPASS)  {
            ESP_LOGI(TAG, "create thread %s failed", "ota_task");
            return ESP_FAIL;
    }
#endif
    return ESP_OK;
}

esp_err_t start_ota(const char * bin_url, bool bFromAppMain)
{
#if RECOVERY_APPLICATION
	return process_recovery_ota(bin_url);
#else
		ESP_LOGW(TAG, "Called to update the firmware from url: %s",bin_url);
		store_nvs_value(NVS_TYPE_STR, "fwurl", bin_url);
		ESP_LOGW(TAG, "Rebooting to recovery to complete the installation");
	return guided_factory();
	return ESP_OK;
#endif
}
