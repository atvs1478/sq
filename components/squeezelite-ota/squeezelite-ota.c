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


static const char *TAG = "squeezelite-ota";
extern const uint8_t server_cert_pem_start[] asm("_binary_github_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_github_pem_end");
extern bool wait_for_wifi();
extern char current_namespace[];
static struct {
	char status_text[31];
	uint32_t ota_actual_len;
	uint32_t ota_total_len;
	char * actual_url;
	bool bOTA_started;
} ota_status;

static esp_http_client_config_t config;
static esp_http_client_config_t ota_config;
static esp_http_client_handle_t client;

const char * ota_get_status(){
	return ota_status.status_text;
}
uint8_t ota_get_pct_complete(){
	return ota_status.ota_total_len==0?0:
			(uint8_t)((uint32_t)ota_status.ota_actual_len/(uint32_t)ota_status.ota_total_len*100);
}

static void __attribute__((noreturn)) task_fatal_error(void)
{
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    (void)vTaskDelete(NULL);

    while (1) {
        ;
    }
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
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
        //strncpy(ota_status,"HTTP_EVENT_ERROR",sizeof(ota_status)-1);
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        if(!ota_status.bOTA_started){
        	ESP_LOGW(TAG, "Resetting the OTA stats");
			ota_status.ota_total_len=0;
			ota_status.ota_actual_len=0;
			if(ota_status.actual_url!=NULL){
				free(ota_status.actual_url);
				ota_status.actual_url=NULL;
			}
        }
       break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
       /// strncpy(ota_status,"HTTP_EVENT_HEADER_SENT",sizeof(ota_status)-1);
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, status_code=%d, key=%s, value=%s",esp_http_client_get_status_code(evt->client),evt->header_key, evt->header_value);
		ESP_LOGD(TAG,"Heap internal:%zu (min:%zu) external:%zu (min:%zu)\n",
					heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
					heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
					heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
					heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
		if (strcasecmp(evt->header_key, "location") == 0) {
        	if(ota_status.actual_url!=NULL) {
        		free(ota_status.actual_url);
        		ota_status.actual_url = NULL;
        	}
        	ota_status.actual_url=strdup(evt->header_value);
        	ESP_LOGW(TAG,"OTA will redirect to url: %s",ota_status.actual_url);
        	ota_status.bOTA_started = true;
        }
        if (strcasecmp(evt->header_key, "content-length") == 0) {
        	ota_status.ota_total_len = atol(evt->header_value);
        	 ESP_LOGW(TAG, "Content length found: %s, parsed to %d", evt->header_value, ota_status.ota_total_len);
        }
        break;
    case HTTP_EVENT_ON_DATA:
    	if(!ota_status.bOTA_started) ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, status_code=%d, len=%d",esp_http_client_get_status_code(evt->client), evt->data_len);
		ESP_LOGD(TAG,"Heap internal:%zu (min:%zu) external:%zu (min:%zu)\n",
					heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
					heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
					heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
					heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
		if(esp_http_client_get_status_code(evt->client) == 302){
                	// This is an indication of a redirect.  Let's follow it
                	return ESP_OK;
        }
        if(esp_http_client_get_status_code(evt->client) == 200 ){
        	if(!ota_status.bOTA_started) return ESP_OK;
			ota_status.ota_actual_len+=evt->data_len;
			ESP_LOGD(TAG,"Receiving OTA data chunk len: %d, %d of %d (%d pct)", evt->data_len, ota_status.ota_actual_len, ota_status.ota_total_len, ota_get_pct_complete());
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

static void check_http_redirect(void)
{
	esp_err_t err=ESP_OK;
	ESP_LOGD(TAG, "Checking for http redirects. initializing http client");
    client = esp_http_client_init(&config);
    if (client == NULL) {
		ESP_LOGE(TAG, "Failed to initialise HTTP connection");
		task_fatal_error();
	}
    ESP_LOGD(TAG, "opening http connection");
	err = esp_http_client_open(client, 0);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
		esp_http_client_cleanup(client);
		task_fatal_error();
	}
	ESP_LOGD(TAG, "fetching headers");
	esp_http_client_fetch_headers(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "redirect check returned success");
    } else {
        ESP_LOGE(TAG, "redirect check returned %s", esp_err_to_name(err));
    }
}
esp_err_t init_config(esp_http_client_config_t * conf, const char * url){
	memset(&ota_status, 0x00, sizeof(ota_status));
	if(url==NULL || strlen(url)==0){
		ESP_LOGE(TAG,"HTTP OTA called without a url");
		return ESP_ERR_INVALID_ARG;
	}
	memset(conf, 0x00, sizeof(esp_http_client_config_t));
	ota_status.actual_url= strdup(url);
	ota_status.bOTA_started=false;
	conf->cert_pem = (char *)server_cert_pem_start;
	conf->event_handler = _http_event_handler;
	conf->buffer_size = 1024*4;
	conf->disable_auto_redirect=true;
	conf->skip_cert_common_name_check = false;
	conf->url = strdup(ota_status.actual_url);
	conf->max_redirection_count = 0;

	return ESP_OK;
}
void ota_task(void *pvParameter)
{
	ESP_LOGD(TAG, "HTTP ota Thread started");

	if(init_config(&config,(char *)pvParameter)!=ESP_OK){
		if(pvParameter!=NULL) free(pvParameter);
		vTaskDelete(NULL);
		return;
	}
	free(pvParameter);

	check_http_redirect();
	if(!ota_status.bOTA_started || ota_status.actual_url == NULL){
		// OTA Failed miserably.  Errors would have been logged somewhere
		ESP_LOGE(TAG,"Redirect check failed. Bailing out");
		vTaskDelete(NULL);
	}
	ESP_LOGD(TAG,"Calling esp_https_ota");
	if(init_config(&ota_config,ota_status.actual_url)!=ESP_OK){
		if(pvParameter!=NULL) free(pvParameter);
		vTaskDelete(NULL);
		return;
	}
	free(ota_status.actual_url);
	ota_status.actual_url=NULL;

	esp_err_t err = esp_https_ota(&config);
    if (err == ESP_OK) {
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware upgrade failed with error : %s", esp_err_to_name(err));
    }
    vTaskDelete(NULL);
}

void start_ota(const char * bin_url)
{
	ESP_LOGW(TAG, "Called to update the firmware from url: %s",bin_url);
#if RECOVERY_APPLICATION
	// Initialize NVS.
	int ret=0;
	esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    	// todo: If we ever change the size of the nvs partition, we need to figure out a mechanism to enlarge the nvs.
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    char * urlPtr=malloc((strlen(bin_url)+1)*sizeof(char));
    strcpy(urlPtr,bin_url);
	// the first thing we need to do here is to erase the firmware url
	// to avoid a boot loop
    nvs_handle nvs;

    err = nvs_open(current_namespace, NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_erase_key(nvs, "fwurl");
        if (err == ESP_OK) {
            err = nvs_commit(nvs);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Value with key '%s' erased", "fwurl");
            }
        }
        nvs_close(nvs);
    }


    ESP_LOGI(TAG, "Starting ota: %s", urlPtr);
    ret=xTaskCreate(&ota_task, "ota_task", 1024*8,(void *) urlPtr, 3, NULL);
    if (ret != pdPASS)  {
            ESP_LOGI(TAG, "create thread %s failed", "ota_task");
        }
#else
    ESP_LOGW(TAG, "Rebooting to recovery to complete the installation");
    guided_factory();
#endif

}






































