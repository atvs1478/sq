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
#include "esp_https_ota.h"
#include "string.h"
#include <stdbool.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "cmd_system.h"
#include "esp_err.h"
#include "tcpip_adapter.h"
#include "squeezelite-ota.h"
#include "config.h"
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "esp_secure_boot.h"
#include "esp_flash_encrypt.h"
#include "esp_spi_flash.h"
#include "sdkconfig.h"
#include "messaging.h"
#include "trace.h"
#include "esp_ota_ops.h"
#include "display.h"

extern const char * get_certificate();

#ifdef CONFIG_ESP32_WIFI_TASK_PINNED_TO_CORE_1
#define OTA_CORE 0
#else
#define OTA_CORE 1
#endif

static const size_t bin_ota_chunk = 4096*2;
static const char *TAG = "squeezelite-ota";
esp_http_client_handle_t ota_http_client = NULL;
#define IMAGE_HEADER_SIZE sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t) + 1
#define BUFFSIZE 2048
#define HASH_LEN 32 /* SHA-256 digest length */
typedef struct  {
	char * url;
	char * bin;
	uint32_t length;
} ota_thread_parms_t ;
static ota_thread_parms_t ota_thread_parms;
typedef enum  {
	OTA_TYPE_HTTP,
	OTA_TYPE_BUFFER,
	OTA_TYPE_INVALID
} ota_type_t;
static struct {
	uint32_t actual_image_len;
	uint32_t total_image_len;
	uint32_t remain_image_len;
	ota_type_t ota_type;
	char * ota_write_data;
	bool bOTAStarted;
	size_t buffer_size;
	uint8_t lastpct;
	uint8_t newpct;
	struct timeval OTA_start;
	bool bOTAThreadStarted;

} ota_status;
struct timeval tv;
static esp_http_client_config_t ota_config;

void _printMemStats(){
	ESP_LOGD(TAG,"Heap internal:%zu (min:%zu) external:%zu (min:%zu)",
			heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
			heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
			heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
			heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
}
uint8_t  ota_get_pct_complete(){
	return ota_status.total_image_len==0?0:
			(uint8_t)((float)ota_status.actual_image_len/(float)ota_status.total_image_len*100.0f);
}
static bool (*display_bus_chain)(void *from, enum display_bus_cmd_e cmd);
static bool display_dummy_handler(void *from, enum display_bus_cmd_e cmd) {
	// chain to rest of "bus"
	if (display_bus_chain) return (*display_bus_chain)(from, cmd);
	else return true;
}
void sendMessaging(messaging_types type,const char * fmt, ...){
    va_list args;
    cJSON * msg = cJSON_CreateObject();
    size_t str_len=0;
    char * msg_str=NULL;

    va_start(args, fmt);
    str_len = vsnprintf(NULL,0,fmt,args)+1;
    if(str_len>0){
    	msg_str = malloc(str_len);
    	vsnprintf(msg_str,str_len,fmt,args);
        if(type == MESSAGING_WARNING){
        	ESP_LOGW(TAG,"%s",msg_str);
        }
    	else if (type == MESSAGING_ERROR){
    		ESP_LOGE(TAG,"%s",msg_str);
    	}
    	else
    		ESP_LOGI(TAG,"%s",msg_str);
    }
    else {
    	ESP_LOGW(TAG, "Sending empty string message");
    }
    va_end(args);

    displayer_scroll(msg_str, 33);
    cJSON_AddStringToObject(msg,"ota_dsc",str_or_unknown(msg_str));
    free(msg_str);
    cJSON_AddNumberToObject(msg,"ota_pct",	ota_get_pct_complete()	);
    char * json_msg = cJSON_PrintUnformatted(msg);
	messaging_post_message(type, MESSAGING_CLASS_OTA, json_msg);
	free(json_msg);
	cJSON_free(msg);
    _printMemStats();
}
//esp_err_t decode_alloc_ota_message(single_message_t * message, char * ota_dsc, uint8_t * ota_pct ){
//	if(!message || !message->message) return ESP_ERR_INVALID_ARG;
//	cJSON * json = cJSON_Parse(message->message);
//	if(!json) return ESP_FAIL;
//	if(ota_dsc) {
//		ota_dsc = strdup(cJSON_GetObjectItem(json, "ota_dsc")?cJSON_GetStringValue(cJSON_GetObjectItem(json, "ota_dsc")):"");
//	}
//	if(ota_pct){
//		*ota_pct = cJSON_GetObjectItem(json, "ota_pct")?cJSON_GetObjectItem(json, "ota_pct")->valueint:0;
//	}
//	cJSON_free(json);
//	return ESP_OK;
//}

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
        _printMemStats();
        //strncpy(ota_status,"HTTP_EVENT_ERROR",sizeof(ota_status)-1);
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");

        if(ota_status.bOTAStarted) sendMessaging(MESSAGING_INFO,"Connecting to URL...");
        ota_status.total_image_len=0;
		ota_status.actual_image_len=0;
		ota_status.lastpct=0;
		ota_status.remain_image_len=0;
		ota_status.newpct=0;
		gettimeofday(&ota_status.OTA_start, NULL);

			break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s",evt->header_key, evt->header_value);
		if (strcasecmp(evt->header_key, "location") == 0) {
        	ESP_LOGW(TAG,"OTA will redirect to url: %s",evt->header_value);
        }
        if (strcasecmp(evt->header_key, "content-length") == 0) {
        	ota_status.total_image_len = atol(evt->header_value);
        	 ESP_LOGW(TAG, "Content length found: %s, parsed to %d", evt->header_value, ota_status.total_image_len);
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

esp_err_t init_config(ota_thread_parms_t * p_ota_thread_parms){
	memset(&ota_config, 0x00, sizeof(ota_config));
	sendMessaging(MESSAGING_INFO,"Initializing...");
	ota_status.ota_type= OTA_TYPE_INVALID;
	if(p_ota_thread_parms->url !=NULL && strlen(p_ota_thread_parms->url)>0 ){
		ota_status.ota_type= OTA_TYPE_HTTP;
	}
	else if(p_ota_thread_parms->bin!=NULL && p_ota_thread_parms->length > 0) {
		ota_status.ota_type= OTA_TYPE_BUFFER;
	}

	if(  ota_status.ota_type== OTA_TYPE_INVALID ){
		ESP_LOGE(TAG,"HTTP OTA called without a url or a binary buffer");
		return ESP_ERR_INVALID_ARG;
	}
	ota_status.buffer_size = BUFFSIZE;
	switch (ota_status.ota_type) {
	case OTA_TYPE_HTTP:
		ota_config.cert_pem =get_certificate();
		ota_config.event_handler = _http_event_handler;
		ota_config.buffer_size = ota_status.buffer_size;
		ota_config.disable_auto_redirect=true;
		ota_config.skip_cert_common_name_check = false;
		ota_config.url = strdup(p_ota_thread_parms->url);
		ota_config.max_redirection_count = 3;


		ota_status.ota_write_data = heap_caps_malloc(ota_status.buffer_size+1 , (MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));

		if(ota_status.ota_write_data== NULL){
			ESP_LOGE(TAG,"Error allocating the ota buffer");
			return ESP_ERR_NO_MEM;
		}

		break;
	case OTA_TYPE_BUFFER:
		ota_status.ota_write_data = p_ota_thread_parms->bin;
		ota_status.total_image_len = p_ota_thread_parms->length;
		ota_status.buffer_size = bin_ota_chunk;
		break;
	default:
		return ESP_FAIL;
		break;
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
		if(i%2) {
			sendMessaging(MESSAGING_INFO,"Erasing flash (%u/%u)",i,num_passes);
		}
		vTaskDelay(100/ portTICK_PERIOD_MS);  // wait here for a short amount of time.  This will help with reducing WDT errors
	}
	if(remain_size>0){
		err=esp_partition_erase_range(ota_partition, ota_partition->size-remain_size, remain_size);
		if(err!=ESP_OK) return err;
	}
	sendMessaging(MESSAGING_INFO,"Erasing flash complete.");

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
    if (status_code == HttpStatus_MovedPermanently || status_code == HttpStatus_Found ) {
    	ESP_LOGW(TAG, "Handling HTTP redirection. ");
        err = esp_http_client_set_redirection(http_client);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "URL redirection Failed. %s", esp_err_to_name(err));
            return err;
        }
        ESP_LOGW(TAG, "Done Handling HTTP redirection. ");

    } else if (status_code == HttpStatus_Unauthorized) {
    	ESP_LOGW(TAG, "Handling Unauthorized. ");
        esp_http_client_add_auth(http_client);
    }
    ESP_LOGD(TAG, "Redirection done, checking if we need to read the data. ");
    if (process_again(status_code)) {
    	//ESP_LOGD(TAG, "We have to read some more data. Allocating buffer size %u",ota_config.buffer_size+1);
    	char * local_buff = heap_caps_malloc(ota_status.buffer_size+1, (MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));

    	if(local_buff==NULL){
    		ESP_LOGE(TAG,"Failed to allocate internal memory buffer for http processing");
    		return ESP_ERR_NO_MEM;
    	}
        while (1) {
        	ESP_LOGD(TAG, "Buffer successfully allocated. Reading data chunk. ");
            int data_read = esp_http_client_read(http_client, local_buff, ota_status.buffer_size);
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
		sendMessaging(MESSAGING_ERROR,message, args);
	    va_end(args);
	}
	if(ota_status.ota_type == OTA_TYPE_HTTP){
		FREE_RESET(ota_status.ota_write_data);
	}
	if(ota_http_client!=NULL) {
		esp_http_client_cleanup(ota_http_client);
		ota_http_client=NULL;
	}
	ota_status.bOTAStarted = false;
	displayer_control(DISPLAYER_SHUTDOWN);
	task_fatal_error();
}


void ota_task(void *pvParameter)
{
	esp_err_t err = ESP_OK;
	displayer_control(DISPLAYER_ACTIVATE, "Firmware update");
	displayer_scroll("Initializing...", 33);
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
	sendMessaging(MESSAGING_INFO,"Erasing OTA partition");
	esp_partition_t *ota_partition = _get_ota_partition(ESP_PARTITION_SUBTYPE_APP_OTA_0);
	if(ota_partition == NULL){
		ESP_LOGE(TAG,"Unable to locate OTA application partition. ");
        ota_task_cleanup("Error: OTA application partition not found. (%s)",esp_err_to_name(err));
        return;
	}
	if(ota_status.ota_type == OTA_TYPE_BUFFER){
		if(ota_status.total_image_len > ota_partition->size){
			ota_task_cleanup("Error: Image size too large to fit in partition.");
			return;
		}
	}
	_printMemStats();
	err=_erase_last_boot_app_partition(ota_partition);
	if(err!=ESP_OK){
		ota_task_cleanup("Error: Unable to erase last APP partition. (%s)",esp_err_to_name(err));
		return;
	}

	_printMemStats();
	ota_status.bOTAStarted = true;
	sendMessaging(MESSAGING_INFO,"Starting OTA...");
	if (ota_status.ota_type == OTA_TYPE_HTTP){
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
	}
	else {
		gettimeofday(&ota_status.OTA_start, NULL);
	}
	_printMemStats();

    esp_ota_handle_t update_handle = 0 ;
    /*deal with all receive packet*/
    bool image_header_was_checked = false;
    int data_read = 0;
    if (ota_status.ota_type == OTA_TYPE_HTTP && ota_status.total_image_len > ota_partition->size){
    	ota_task_cleanup("Error: Image size too large to fit in partition.");
        return;
	}

    while (1) {
    	ota_status.remain_image_len =ota_status.total_image_len -ota_status.actual_image_len;

        if (ota_status.ota_type == OTA_TYPE_HTTP){

        	data_read = esp_http_client_read(ota_http_client, ota_status.ota_write_data, ota_status.buffer_size);
        }
        else {
        	if(ota_status.remain_image_len >ota_status.buffer_size){
        		data_read = ota_status.buffer_size;
        	} else {
        		data_read = ota_status.remain_image_len;
        	}
        }
        if (data_read < 0) {
            ota_task_cleanup("Error: Data read error");
            return;
        } else if (data_read > 0) {
        	if (image_header_was_checked == false) {
                esp_app_desc_t new_app_info;
                if (data_read >= IMAGE_HEADER_SIZE) {
                    // check current version with downloading
                    memcpy(&new_app_info, &ota_status.ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
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
                }
                else {
					ota_task_cleanup("Error: Increase ota http buffer.");
					return;
				}
            }

            err = esp_ota_write( update_handle, (const void *)ota_status.ota_write_data, data_read);
            if (err != ESP_OK) {
                ota_task_cleanup("Error: OTA Partition write failure. (%s)",esp_err_to_name(err));
                return;
            }
            ota_status.actual_image_len += data_read;
            if(ota_status.ota_type == OTA_TYPE_BUFFER){
            	// position the ota buffer in the next buffer chunk
            	ota_status.ota_write_data+= data_read;
            }
            ESP_LOGD(TAG, "Written image length %d", ota_status.actual_image_len);

			if(ota_get_pct_complete()%5 == 0) ota_status.newpct = ota_get_pct_complete();
			if(ota_status.lastpct!=ota_status.newpct ) {

				gettimeofday(&tv, NULL);
				uint32_t elapsed_ms= (tv.tv_sec-ota_status.OTA_start.tv_sec )*1000+(tv.tv_usec-ota_status.OTA_start.tv_usec)/1000;
				ESP_LOGI(TAG,"OTA progress : %d/%d (%d pct), %d KB/s", ota_status.actual_image_len, ota_status.total_image_len, ota_status.newpct, elapsed_ms>0?ota_status.actual_image_len*1000/elapsed_ms/1024:0);

				sendMessaging(MESSAGING_INFO,ota_status.ota_type == OTA_TYPE_HTTP?"Downloading & writing update.":"Writing binary file.");
				ota_status.lastpct=ota_status.newpct;
			}
			taskYIELD();

        } else if (data_read == 0) {
            ESP_LOGI(TAG, "End of OTA data stream");
            break;
        }
    }

    ESP_LOGI(TAG, "Total Write binary data length: %d", ota_status.actual_image_len);
    if (ota_status.total_image_len != ota_status.actual_image_len) {
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
    	sendMessaging(MESSAGING_INFO,"Success!");

    	vTaskDelay(1000/ portTICK_PERIOD_MS);  // wait here to give the UI a chance to refresh
        esp_restart();
    } else {
        ota_task_cleanup("Error: Unable to update boot partition [%s]",esp_err_to_name(err));
        return;
    }
    ota_task_cleanup(NULL);
    return;
}

esp_err_t process_recovery_ota(const char * bin_url, char * bin_buffer, uint32_t length){
	int ret = 0;
	display_bus_chain = display_bus;
	display_bus = display_dummy_handler;
	uint16_t stack_size, task_priority;
    if(ota_status.bOTAThreadStarted){
		ESP_LOGE(TAG,"OTA Already started. ");
		return ESP_FAIL;
	}
	memset(&ota_status, 0x00, sizeof(ota_status));
	ota_status.bOTAThreadStarted=true;

	if(bin_url){
		ota_thread_parms.url =strdup(bin_url);
		ESP_LOGI(TAG, "Starting ota on core %u for : %s", OTA_CORE,ota_thread_parms.url);
	}
	else {
		ota_thread_parms.bin = bin_buffer;
		ota_thread_parms.length = length;
		ESP_LOGI(TAG, "Starting ota on core %u for file upload", OTA_CORE);
	}

    char * num_buffer=config_alloc_get(NVS_TYPE_STR, "ota_stack");
  	if(num_buffer!=NULL) {
  		stack_size= atol(num_buffer);
  		FREE_AND_NULL(num_buffer);
  	}
  	else {
		ESP_LOGW(TAG,"OTA stack size config not found");
  		stack_size = OTA_STACK_SIZE;
  	}
  	num_buffer=config_alloc_get(NVS_TYPE_STR, "ota_prio");
	if(num_buffer!=NULL) {
		task_priority= atol(num_buffer);
		FREE_AND_NULL(num_buffer);
	}
	else {
		ESP_LOGW(TAG,"OTA task priority not found");
		task_priority= OTA_TASK_PRIOTITY;
  	}

  	ESP_LOGD(TAG,"OTA task stack size %d, priority %d (%d %s ESP_TASK_MAIN_PRIO)",stack_size , task_priority, abs(task_priority-ESP_TASK_MAIN_PRIO), task_priority-ESP_TASK_MAIN_PRIO>0?"above":"below");
    ret=xTaskCreatePinnedToCore(&ota_task, "ota_task", stack_size , (void *)&ota_thread_parms, task_priority, NULL, OTA_CORE);
    if (ret != pdPASS)  {
            ESP_LOGI(TAG, "create thread %s failed", "ota_task");
            return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t start_ota(const char * bin_url, char * bin_buffer, uint32_t length)
{
#if RECOVERY_APPLICATION
	return process_recovery_ota(bin_url,bin_buffer,length);
#else
	if(!bin_url){
		ESP_LOGE(TAG,"missing URL parameter. Unable to start OTA");
		return ESP_ERR_INVALID_ARG;
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
	return ESP_OK;
#endif
}
