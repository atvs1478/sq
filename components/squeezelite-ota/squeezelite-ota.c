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

#include "esp_err.h"
#include "tcpip_adapter.h"


static const char *TAG = "squeezelite-ota";
//extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
//extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

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
        strncpy(ota_status,sizeof(ota_status)-1,"HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        strncpy(ota_status,sizeof(ota_status)-1,"HTTP_EVENT_ON_CONNECTED");
                break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        strncpy(ota_status,sizeof(ota_status)-1,"HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        strncpy(ota_status,sizeof(ota_status)-1,"HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

void ota_task(void *pvParameter)
{
	char * bin_url=(char *)pvParameter;
    ESP_LOGI(TAG, "Starting OTA example");

    esp_http_client_config_t config = {
        .url = bin_url,
        //.cert_pem = (char *)server_cert_pem_start,
        .event_handler = _http_event_handler,
    };

    // todo: review how certificates work
    config.skip_cert_common_name_check = true;

    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK) {
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware upgrade failed");
    }
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void start_ota(const char * bin_url)
{
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

    xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, urlPtr);
}
