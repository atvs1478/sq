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
#include "esp_http_client.h"
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

#define OTA_URL_SIZE 256
static char ota_status[31]={0};
static uint8_t ota_pct=0;
const char * ota_get_status(){
	return ota_status;
}
uint8_t ota_get_pct_complete(){
	return ota_pct;
}
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        //strncpy(ota_status,"HTTP_EVENT_ERROR",sizeof(ota_status)-1);
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
       // strncpy(ota_status,"HTTP_EVENT_ON_CONNECTED",sizsffeof(ota_status)-1);
                break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
       /// strncpy(ota_status,"HTTP_EVENT_HEADER_SENT",sizeof(ota_status)-1);
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, status_code=%d, key=%s, value=%s",esp_http_client_get_status_code(evt->client),evt->header_key, evt->header_value);
        // check for header Content-Length:2222240
        // convert to numeric and store in total bin size

        //snprintf(ota_status,sizeof(ota_status)-1,"HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, status_code=%d, len=%d",esp_http_client_get_status_code(evt->client), evt->data_len);
        //increment the total data received, then divide by the bin size and store as ota_pct

        //snprintf(ota_status,sizeof(ota_status)-1, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        //strncpy(ota_status,"HTTP_EVENT_DISCONNECTED",sizeof(ota_status)-1);
        break;
    }
    return ESP_OK;
}

void ota_task(void *pvParameter)
{
	char * bin_url=(char *)pvParameter;

     esp_http_client_config_t config = {
        .url = bin_url,
        .cert_pem = (char *)server_cert_pem_start,
        .event_handler = _http_event_handler,
		.buffer_size = 1024*50,

    };

    config.skip_cert_common_name_check = false;

    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK) {
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware upgrade failed");
    }
    free(pvParameter);
    return;
}

void start_ota(const char * bin_url)
{
	ESP_LOGW(TAG, "Called to update the firmware from url: %s",bin_url);
#if RECOVERY_APPLICATION
	// Initialize NVS.
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

    ESP_LOGI(TAG, "Starting ota: %s", urlPtr);
    xTaskCreate(&ota_task, "ota_task", 8192*3,(void *) urlPtr, 5, NULL);
#else
    ESP_LOGW(TAG, "Rebooting to recovery to complete the installation");
    guided_factory();
#endif

}
