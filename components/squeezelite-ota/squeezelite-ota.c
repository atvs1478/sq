/* OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#ifndef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_https_ota.h"
#include "string.h"
#include <stdbool.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "cmd_system.h"
#include "esp_err.h"
#include "tcpip_adapter.h"
#include "squeezelite-ota.h"
#include "platform_config.h"
#include "platform_esp32.h"
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "esp_secure_boot.h"
#include "esp_flash_encrypt.h"
#include "esp_spi_flash.h"
#include "platform_esp32.h"
#include "sdkconfig.h"

#include "esp_ota_ops.h"
extern const char * get_certificate();

static const char *TAG = "squeezelite-ota";
char * ota_write_data = NULL;
esp_http_client_handle_t ota_http_client = NULL;
#define IMAGE_HEADER_SIZE sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t) + 1
#define BUFFSIZE 4096
#define HASH_LEN 32 /* SHA-256 digest length */


static struct {
	char status_text[81];
	uint32_t ota_actual_len;
	uint32_t ota_total_len;
	char * redirected_url;
	char * current_url;
	bool bOTAStarted;
	bool bInitialized;
	uint8_t lastpct;
	uint8_t newpct;
	struct timeval OTA_start;
	bool bOTAThreadStarted;

} ota_status;
struct timeval tv;
static esp_http_client_config_t ota_config;

extern void wifi_manager_refresh_ota_json();

void _printMemStats(){
	ESP_LOGD(TAG,"Heap internal:%zu (min:%zu) external:%zu (min:%zu)",
			heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
			heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
			heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
			heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
}
void triggerStatusJsonRefresh(bool bDelay,const char * status, ...){
    va_list args;
    va_start(args, status);
    vsnprintf(ota_status.status_text,sizeof(ota_status.status_text)-1,status, args);
    va_end(args);
    _printMemStats();
	wifi_manager_refresh_ota_json();
	if(bDelay){
		ESP_LOGD(TAG,"Holding task...");
	    vTaskDelay(200 / portTICK_PERIOD_MS);  // wait here for a short amount of time.  This will help with refreshing the UI status
		ESP_LOGD(TAG,"Done holding task...");
	}
	else {
		ESP_LOGI(TAG,"%s",ota_status.status_text);
		taskYIELD();
	}
}
const char *  ota_get_status(){
	if(!ota_status.bInitialized)
		{
			memset(ota_status.status_text, 0x00,sizeof(ota_status.status_text));
			ota_status.bInitialized = true;
		}
	return ota_status.status_text;
}
uint8_t  ota_get_pct_complete(){
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
        _printMemStats();
        //strncpy(ota_status,"HTTP_EVENT_ERROR",sizeof(ota_status)-1);
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");

        if(ota_status.bOTAStarted) triggerStatusJsonRefresh(true,"Installing...");
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
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s",evt->header_key, evt->header_value);
		if (strcasecmp(evt->header_key, "location") == 0) {
			FREE_RESET(ota_status.redirected_url);
        	ota_status.redirected_url=strdup(evt->header_value);
        	ESP_LOGW(TAG,"OTA will redirect to url: %s",ota_status.redirected_url);
        }
        if (strcasecmp(evt->header_key, "content-length") == 0) {
        	ota_status.ota_total_len = atol(evt->header_value);
        	 ESP_LOGW(TAG, "Content length found: %s, parsed to %d", evt->header_value, ota_status.ota_total_len);
        }
        break;
    case HTTP_EVENT_ON_DATA:
    	if(!ota_status.bOTAStarted)  {
    		ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, status_code=%d, len=%d",esp_http_client_get_status_code(evt->client), evt->data_len);
    	}
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

esp_err_t init_config(char * url){
	memset(&ota_config, 0x00, sizeof(ota_config));
	ota_status.bInitialized = true;
	triggerStatusJsonRefresh(true,"Initializing...");
	if(url==NULL || strlen(url)==0){
		ESP_LOGE(TAG,"HTTP OTA called without a url");
		return ESP_FAIL;
	}
	ota_status.current_url= url;
	ota_config.cert_pem =get_certificate();
	ota_config.event_handler = _http_event_handler;
	ota_config.buffer_size = BUFFSIZE;
	//ota_config.disable_auto_redirect=true;
	ota_config.disable_auto_redirect=false;
	ota_config.skip_cert_common_name_check = false;
	ota_config.url = strdup(url);
	ota_config.max_redirection_count = 3;
	ota_write_data = heap_caps_malloc(ota_config.buffer_size+1 , MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
	//ota_write_data = malloc(ota_config.buffer_size+1);
	if(ota_write_data== NULL){
		ESP_LOGE(TAG,"Error allocating the ota buffer");
		return ESP_ERR_NO_MEM;
	}
	return ESP_OK;
}
esp_partition_t * _get_ota_partition(esp_partition_subtype_t subtype){
	esp_partition_t *ota_partition=NULL;
	ESP_LOGI(TAG, "Looking for OTA partition.");

	esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, subtype , NULL);
	if(it == NULL){
		ESP_LOGE(TAG,"Unable initialize partition iterator!");
	}
	else {
		ota_partition = (esp_partition_t *) esp_partition_get(it);
		if(ota_partition != NULL){
			ESP_LOGI(TAG, "Found OTA partition: %s.",ota_partition->label);
		}
		else {
			ESP_LOGE(TAG,"OTA partition not found!  Unable update application.");
		}
		esp_partition_iterator_release(it);
	}
	return ota_partition;

}



esp_err_t _erase_last_boot_app_partition(esp_partition_t *ota_partition)
{
	uint16_t num_passes=0;
	uint16_t remain_size=0;
	uint32_t single_pass_size=0;
	esp_err_t err=ESP_OK;

    char * ota_erase_size=config_alloc_get(NVS_TYPE_STR, "ota_erase_blk");
	if(ota_erase_size!=NULL) {
		single_pass_size = atol(ota_erase_size);
		ESP_LOGD(TAG,"OTA Erase block size is %d (from string: %s)",single_pass_size, ota_erase_size );
		free(ota_erase_size);
	}
	else {
		ESP_LOGW(TAG,"OTA Erase block config not found");
		single_pass_size = OTA_FLASH_ERASE_BLOCK;
	}

	if(single_pass_size % SPI_FLASH_SEC_SIZE !=0){
		uint32_t temp_single_pass_size = single_pass_size-(single_pass_size % SPI_FLASH_SEC_SIZE);
		ESP_LOGW(TAG,"Invalid erase block size of %u. Value should be a multiple of %d and will be adjusted to %u.", single_pass_size, SPI_FLASH_SEC_SIZE,temp_single_pass_size);
		single_pass_size=temp_single_pass_size;
	}
	ESP_LOGI(TAG,"Erasing flash partition of size %u in blocks of %d bytes", ota_partition->size, single_pass_size);
	num_passes=ota_partition->size/single_pass_size;
	remain_size=ota_partition->size-(num_passes*single_pass_size);
	ESP_LOGI(TAG,"Erasing in %d passes with blocks of %d bytes ", num_passes,single_pass_size);
	for(uint16_t i=0;i<num_passes;i++){
		ESP_LOGD(TAG,"Erasing flash (%u%%)",i/num_passes);
		ESP_LOGD(TAG,"Pass %d of %d, with chunks of %d bytes, from %d to %d", i+1, num_passes,single_pass_size,i*single_pass_size,i*single_pass_size+single_pass_size);
		err=esp_partition_erase_range(ota_partition, i*single_pass_size, single_pass_size);
		if(err!=ESP_OK) return err;
//		triggerStatusJsonRefresh(i%10==0?true:false,"Erasing flash (%u/%u)",i,num_passes);
		if(i%2) {
			triggerStatusJsonRefresh(false,"Erasing flash (%u/%u)",i,num_passes);
		}
		vTaskDelay(200/ portTICK_PERIOD_MS);  // wait here for a short amount of time.  This will help with reducing WDT errors
	}
	if(remain_size>0){
		err=esp_partition_erase_range(ota_partition, ota_partition->size-remain_size, remain_size);
		if(err!=ESP_OK) return err;
	}
	triggerStatusJsonRefresh(true,"Erasing flash complete.");
	taskYIELD();
	return ESP_OK;
}

static bool process_again(int status_code)
{
    switch (status_code) {
        case HttpStatus_MovedPermanently:
        case HttpStatus_Found:
        case HttpStatus_Unauthorized:
            return true;
        default:
            return false;
    }
    return false;
}
static esp_err_t _http_handle_response_code(esp_http_client_handle_t http_client, int status_code)
{
    esp_err_t err=ESP_OK;
    if (status_code == HttpStatus_MovedPermanently || status_code == HttpStatus_Found) {
    	ESP_LOGW(TAG, "Handling HTTP redirection. ");
        err = esp_http_client_set_redirection(http_client);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "URL redirection Failed. %s", esp_err_to_name(err));
            return err;
        }
    } else if (status_code == HttpStatus_Unauthorized) {
    	ESP_LOGW(TAG, "Handling Unauthorized. ");
        esp_http_client_add_auth(http_client);
    }
    ESP_LOGD(TAG, "Redirection done, checking if we need to read the data. ");
    if (process_again(status_code)) {
    	char * local_buff = heap_caps_malloc(ota_config.buffer_size+1, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    	//char * local_buff = malloc(ota_config.buffer_size+1);
    	if(local_buff==NULL){
    		ESP_LOGE(TAG,"Failed to allocate internal memory buffer for http processing");
    		return ESP_ERR_NO_MEM;
    	}
        while (1) {
        	ESP_LOGD(TAG, "Reading data chunk. ");
            int data_read = esp_http_client_read(http_client, local_buff, ota_config.buffer_size);
            if (data_read < 0) {
                ESP_LOGE(TAG, "Error: SSL data read error");
                err= ESP_FAIL;
                break;
            } else if (data_read == 0) {
            	ESP_LOGD(TAG, "No more data. ");
            	err= ESP_OK;
            	break;
            }
        }
        FREE_RESET(local_buff);
    }

    return err;
}
static esp_err_t _http_connect(esp_http_client_handle_t http_client)
{
    esp_err_t err = ESP_FAIL;
    int status_code, header_ret;
    do {
    	ESP_LOGD(TAG, "connecting the http client. ");
        err = esp_http_client_open(http_client, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
            return err;
        }
        ESP_LOGD(TAG, "Fetching headers");
        header_ret = esp_http_client_fetch_headers(http_client);
        if (header_ret < 0) {
        	// Error found
            return header_ret;
        }
        ESP_LOGD(TAG, "HTTP Header fetch completed, found content length of %d",header_ret);
        status_code = esp_http_client_get_status_code(http_client);
        ESP_LOGD(TAG, "HTTP status code was %d",status_code);



        err = _http_handle_response_code(http_client, status_code);
        if (err != ESP_OK) {
            return err;
        }
    } while (process_again(status_code));
    return err;
}
void ota_task_cleanup(const char * message, ...){
	ota_status.bOTAThreadStarted=false;
	if(message!=NULL){

	    va_list args;
	    va_start(args, message);
		triggerStatusJsonRefresh(true,message, args);
	    va_end(args);
	    ESP_LOGE(TAG, "%s",ota_status.status_text);
	}
	FREE_RESET(ota_status.redirected_url);
	FREE_RESET(ota_status.current_url);
	FREE_RESET(ota_write_data);
	if(ota_http_client!=NULL) {
		esp_http_client_cleanup(ota_http_client);
		ota_http_client=NULL;
	}
	ota_status.bOTAStarted = false;
	task_fatal_error();
}
void ota_task(void *pvParameter)
{
	esp_err_t err = ESP_OK;
	size_t buffer_size = BUFFSIZE;
	ESP_LOGD(TAG, "HTTP ota Thread started");
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t * update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "esp_ota_get_next_update_partition returned : partition [%s] subtype %d at offset 0x%x",
    			update_partition->label, update_partition->subtype, update_partition->address);

    if (configured != running) {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x", configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition [%s] type %d subtype %d (offset 0x%08x)", running->label, running->type, running->subtype, running->address);
    _printMemStats();


	ESP_LOGI(TAG,"Initializing OTA configuration");
	err = init_config(pvParameter);
	if(err!=ESP_OK){
		ota_task_cleanup("Error: Failed to initialize OTA.");
		return;
	}

	/* Locate and erase ota application partition */
	ESP_LOGW(TAG,"****************  Expecting WATCHDOG errors below during flash erase. This is OK and not to worry about **************** ");
	triggerStatusJsonRefresh(true,"Erasing OTA partition");
	esp_partition_t *ota_partition = _get_ota_partition(ESP_PARTITION_SUBTYPE_APP_OTA_0);
	if(ota_partition == NULL){
		ESP_LOGE(TAG,"Unable to locate OTA application partition. ");
        ota_task_cleanup("Error: OTA application partition not found. (%s)",esp_err_to_name(err));
        return;
	}
	_printMemStats();
	err=_erase_last_boot_app_partition(ota_partition);
	if(err!=ESP_OK){
		ota_task_cleanup("Error: Unable to erase last APP partition. (%s)",esp_err_to_name(err));
		return;
	}

	_printMemStats();
	ota_status.bOTAStarted = true;
	triggerStatusJsonRefresh(true,"Starting OTA...");
    ota_http_client = esp_http_client_init(&ota_config);
    if (ota_http_client == NULL) {
        ota_task_cleanup("Error: Failed to initialize HTTP connection.");
        return;
    }
    _printMemStats();
    // Open the http connection and follow any redirection
    err = _http_connect(ota_http_client);
    if (err != ESP_OK) {
       ota_task_cleanup("Error: HTTP Start read failed. (%s)",esp_err_to_name(err));
       return;
    }

    _printMemStats();

    esp_ota_handle_t update_handle = 0 ;
    int binary_file_length = 0;

    /*deal with all receive packet*/
    bool image_header_was_checked = false;
    while (1) {
        int data_read = esp_http_client_read(ota_http_client, ota_write_data, buffer_size);
        if (data_read < 0) {
            ota_task_cleanup("Error: Data read error");
            return;
        } else if (data_read > 0) {
        	if (image_header_was_checked == false) {
                esp_app_desc_t new_app_info;
                if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                    // check current version with downloading
                    memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                    ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

                    esp_app_desc_t running_app_info;
                    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
                        ESP_LOGI(TAG, "Running recovery version: %s", running_app_info.version);
                    }

                    const esp_partition_t* last_invalid_app = esp_ota_get_last_invalid_partition();
                    esp_app_desc_t invalid_app_info;
                    if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK) {
                        ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);
                    }

                    // check current version with last invalid partition
//                    if (last_invalid_app != NULL) {
//                        if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0) {
//                            ESP_LOGW(TAG, "New version is the same as invalid version.");
//                            ESP_LOGW(TAG, "Previously, there was an attempt to launch the firmware with %s version, but it failed.", invalid_app_info.version);
//                            ESP_LOGW(TAG, "The firmware has been rolled back to the previous version.");
//                    		  ota_task_cleanup("esp_ota_begin failed (%s)", esp_err_to_name(err));
//                        }
//                    }

                    if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0) {
                        ESP_LOGW(TAG, "Current running version is the same as a new.");
                    }

                    image_header_was_checked = true;

                    // Call OTA Begin with a small partition size - this drives the erase operation which was already done;
                    err = esp_ota_begin(ota_partition, 512, &update_handle);
                    if (err != ESP_OK) {
                        ota_task_cleanup("esp_ota_begin failed (%s)", esp_err_to_name(err));
                        return;
                    }
					ESP_LOGD(TAG, "esp_ota_begin succeeded");
                } else {
                    ota_task_cleanup("Error: Binary file too large for the current partition");
                    return;
                }
            }
            err = esp_ota_write( update_handle, (const void *)ota_write_data, data_read);
            if (err != ESP_OK) {
                ota_task_cleanup("Error: OTA Partition write failure. (%s)",esp_err_to_name(err));
                return;
            }
            binary_file_length += data_read;
            ESP_LOGD(TAG, "Written image length %d", binary_file_length);
			ota_status.ota_actual_len=binary_file_length;
			if(ota_get_pct_complete()%5 == 0) ota_status.newpct = ota_get_pct_complete();
			if(ota_status.lastpct!=ota_status.newpct ) {
				gettimeofday(&tv, NULL);
				uint32_t elapsed_ms= (tv.tv_sec-ota_status.OTA_start.tv_sec )*1000+(tv.tv_usec-ota_status.OTA_start.tv_usec)/1000;
				ESP_LOGI(TAG,"OTA progress : %d/%d (%d pct), %d KB/s", ota_status.ota_actual_len, ota_status.ota_total_len, ota_status.newpct, elapsed_ms>0?ota_status.ota_actual_len*1000/elapsed_ms/1024:0);
				triggerStatusJsonRefresh(true,"Downloading & writing update.");
				ota_status.lastpct=ota_status.newpct;
			}
			taskYIELD();

        } else if (data_read == 0) {
            ESP_LOGI(TAG, "Connection closed");
            break;
        }
    }

    ESP_LOGI(TAG, "Total Write binary data length: %d", binary_file_length);
    if (ota_status.ota_total_len != binary_file_length) {
        ota_task_cleanup("Error: Error in receiving complete file");
        return;
    }
    _printMemStats();

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        ota_task_cleanup("Error: %s",esp_err_to_name(err));
        return;
     }
    _printMemStats();
    err = esp_ota_set_boot_partition(ota_partition);
    if (err == ESP_OK) {
    	ESP_LOGI(TAG,"OTA Process completed successfully!");
    	triggerStatusJsonRefresh(true,"Success!");
    	vTaskDelay(1500/ portTICK_PERIOD_MS);  // wait here to give the UI a chance to refresh
        esp_restart();
    } else {
        ota_task_cleanup("Error: Unable to update boot partition [%s]",esp_err_to_name(err));
        return;
    }
    ota_task_cleanup(NULL);
    return;
}

esp_err_t process_recovery_ota(const char * bin_url){
	int ret = 0;
	uint16_t stack_size, task_priority;
    if(ota_status.bOTAThreadStarted){
		ESP_LOGE(TAG,"OTA Already started. ");
		return ESP_FAIL;
	}
	memset(&ota_status, 0x00, sizeof(ota_status));
	ota_status.bOTAThreadStarted=true;
    char * urlPtr=strdup(bin_url);
	// the first thing we need to do here is to erase the firmware url
	// to avoid a boot loop

#ifdef CONFIG_ESP32_WIFI_TASK_PINNED_TO_CORE_1
#define OTA_CORE 0
#warning "OTA will run on core 0"
#else
#pragma message "OTA will run on core 1"
#define OTA_CORE 1
#endif
    ESP_LOGI(TAG, "Starting ota on core %u for : %s", OTA_CORE,urlPtr);
    char * num_buffer=config_alloc_get(NVS_TYPE_STR, "ota_stack");
  	if(num_buffer!=NULL) {
  		stack_size= atol(num_buffer);
  		free(num_buffer);
  		num_buffer=NULL;
  	}
  	else {
		ESP_LOGW(TAG,"OTA stack size config not found");
  		stack_size = OTA_STACK_SIZE;
  	}
  	num_buffer=config_alloc_get(NVS_TYPE_STR, "ota_prio");
	if(num_buffer!=NULL) {
		task_priority= atol(num_buffer);
		free(num_buffer);
		num_buffer=NULL;
	}
	else {
		ESP_LOGW(TAG,"OTA task priority not found");
		task_priority= OTA_TASK_PRIOTITY;
  	}
  	ESP_LOGD(TAG,"OTA task stack size %d, priority %d (%d %s ESP_TASK_MAIN_PRIO)",stack_size , task_priority, abs(task_priority-ESP_TASK_MAIN_PRIO), task_priority-ESP_TASK_MAIN_PRIO>0?"above":"below");
    ret=xTaskCreatePinnedToCore(&ota_task, "ota_task", stack_size , (void *)urlPtr, task_priority, NULL, OTA_CORE);
    //ret=xTaskCreate(&ota_task, "ota_task", 1024*20, (void *)urlPtr, ESP_TASK_MAIN_PRIO+2, NULL);
    if (ret != pdPASS)  {
            ESP_LOGI(TAG, "create thread %s failed", "ota_task");
            return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t start_ota(const char * bin_url)
{
	if(is_recovery_running){
		return process_recovery_ota(bin_url);
	}
	ESP_LOGW(TAG, "Called to update the firmware from url: %s",bin_url);
	if(config_set_value(NVS_TYPE_STR, "fwurl", bin_url) != ESP_OK){
		ESP_LOGE(TAG,"Failed to save the OTA url into nvs cache");
		return ESP_FAIL;
	}

	if(!wait_for_commit()){
		ESP_LOGW(TAG,"Unable to commit configuration. ");
	}

	ESP_LOGW(TAG, "Rebooting to recovery to complete the installation");
	return guided_factory();
}
