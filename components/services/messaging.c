	/**
 *
 */
#include <stdlib.h> // Required for libtelnet.h
#include <esp_log.h>
#include "stdbool.h"
#include <lwip/def.h>
#include <lwip/sockets.h>
#include <errno.h>
#include <string.h>
#include "esp_app_trace.h"
#include "esp_attr.h"
#include "config.h"
#include "nvs_utilities.h"
#include "platform_esp32.h"
#include "messaging.h"
/************************************
 * Globals
 */

const static char tag[] = "messaging";
typedef struct {
	struct messaging_list_t * next;
	char * subscriber_name;
	size_t max_count;
	RingbufHandle_t buf_handle;
} messaging_list_t;
static messaging_list_t top;


messaging_list_t * get_struct_ptr(messaging_handle_t handle){
	return (messaging_list_t *)handle;
}
messaging_handle_t  get_handle_ptr(messaging_list_t * handle){
	return (messaging_handle_t )handle;
}
RingbufHandle_t messaging_create_ring_buffer(uint8_t max_count){
	RingbufHandle_t buf_handle = NULL;
	StaticRingbuffer_t *buffer_struct = malloc(sizeof(StaticRingbuffer_t));
	if (buffer_struct != NULL) {
		size_t buf_size = (size_t )(sizeof(single_message_t)+8)*(size_t )(max_count>0?max_count:5); // no-split buffer requires an additional 8 bytes
		uint8_t *buffer_storage = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
		if (buffer_storage== NULL) {
			ESP_LOGE(tag,"buff alloc failed");
		}
		else {
			buf_handle = xRingbufferCreateStatic(buf_size, RINGBUF_TYPE_NOSPLIT, buffer_storage, buffer_struct);
		}
	}
	else {
		ESP_LOGE(tag,"ringbuf alloc failed");
	}
	return buf_handle;
}
void messaging_fill_messages(messaging_list_t * target_subscriber){
	single_message_t * message=NULL;
    UBaseType_t uxItemsWaiting;

    vRingbufferGetInfo(top.buf_handle, NULL, NULL, NULL, NULL, &uxItemsWaiting);
    for(size_t i=0;i<uxItemsWaiting;i++){
    	message= messaging_retrieve_message(top.buf_handle);
    	if(message){
			//re-post to original queue so it is available to future subscribers
			messaging_post_to_queue(get_handle_ptr(&top), message, sizeof(single_message_t));
			// post to new subscriber
			messaging_post_to_queue(get_handle_ptr(target_subscriber) , message, sizeof(single_message_t));
			FREE_AND_NULL(message);
    	}
    }
}
messaging_handle_t messaging_register_subscriber(uint8_t max_count, char * name){
	messaging_list_t * cur=&top;
	while(cur->next){
		cur = get_struct_ptr(cur->next);
	}
	cur->next=heap_caps_malloc(sizeof(messaging_list_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if(!cur->next){
		ESP_LOGE(tag,"subscriber alloc failed");
		return NULL;
	}
	memset(cur->next,0x00,sizeof(messaging_list_t));
	cur = get_struct_ptr(cur->next);
	cur->max_count=max_count;
	cur->subscriber_name=strdup(name);
	cur->buf_handle = messaging_create_ring_buffer(max_count);
	if(cur->buf_handle){
		messaging_fill_messages(cur);
	}
	return cur->buf_handle;
}
void messaging_service_init(){
	size_t max_count=15;
	top.buf_handle = messaging_create_ring_buffer(max_count);
	if(!top.buf_handle){
		ESP_LOGE(tag, "messaging service init failed.");
	}
	else {
		top.max_count = max_count;
		top.subscriber_name = strdup("messaging");
	}
	return;
}

const char * messaging_get_type_desc(messaging_types msg_type){
	switch (msg_type) {
	CASE_TO_STR(MESSAGING_INFO);
	CASE_TO_STR(MESSAGING_WARNING);
	CASE_TO_STR(MESSAGING_ERROR);
		default:
			return "Unknown";
			break;
	}
}
const char * messaging_get_class_desc(messaging_classes msg_class){
	switch (msg_class) {
	CASE_TO_STR(MESSAGING_CLASS_OTA);
	CASE_TO_STR(MESSAGING_CLASS_SYSTEM);
		default:
			return "Unknown";
			break;
	}
}

cJSON *  messaging_retrieve_messages(RingbufHandle_t buf_handle){
	single_message_t * message=NULL;
	cJSON * json_messages=cJSON_CreateArray();
	cJSON * json_message=NULL;
	size_t item_size;
    UBaseType_t uxItemsWaiting;
    vRingbufferGetInfo(buf_handle, NULL, NULL, NULL, NULL, &uxItemsWaiting);
	if(uxItemsWaiting>0){
		message = (single_message_t *)xRingbufferReceive(buf_handle, &item_size, pdMS_TO_TICKS(50));
		//Check received data
		if (message== NULL) {
			ESP_LOGE(tag,"received null ptr");
		}
		else {
			json_message = cJSON_CreateObject();
			cJSON_AddStringToObject(json_message, "message", message->message);
			cJSON_AddStringToObject(json_message, "type", messaging_get_type_desc(message->type));
			cJSON_AddStringToObject(json_message, "class", messaging_get_class_desc(message->msg_class));
			cJSON_AddNumberToObject(json_message,"sent_time",message->sent_time);
			cJSON_AddNumberToObject(json_message,"current_time",esp_timer_get_time() / 1000);
			cJSON_AddItemToArray(json_messages,json_message);
			vRingbufferReturnItem(buf_handle, (void *)message);
		}
	}
	return json_messages;
}
single_message_t *  messaging_retrieve_message(RingbufHandle_t buf_handle){
	single_message_t * message=NULL;
	single_message_t * message_copy=NULL;
	size_t item_size;
    UBaseType_t uxItemsWaiting;
    vRingbufferGetInfo(buf_handle, NULL, NULL, NULL, NULL, &uxItemsWaiting);
	if(uxItemsWaiting>0){
		message = (single_message_t *)xRingbufferReceive(buf_handle, &item_size, pdMS_TO_TICKS(50));
		if(item_size!=sizeof(single_message_t)){
			ESP_LOGE(tag,"Invalid message length!");
		}
		else {
			message_copy  = heap_caps_malloc(item_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
			if(message_copy){
				memcpy(message_copy,message,item_size);
			}
		}
		vRingbufferReturnItem(buf_handle, (void *)message);
	}
	return message_copy;
}

esp_err_t messaging_post_to_queue(messaging_handle_t subscriber_handle, single_message_t * message, size_t message_size){
	UBaseType_t uxItemsWaiting=0;
	size_t item_size=0;
	messaging_list_t * subscriber=get_struct_ptr(subscriber_handle);
	if(!subscriber->buf_handle){
		ESP_LOGE(tag,"post failed: null buffer for %s", str_or_unknown(subscriber->subscriber_name));
		return ESP_FAIL;
	}
	vRingbufferGetInfo(subscriber->buf_handle,NULL,NULL,NULL,NULL,&uxItemsWaiting);
	if(uxItemsWaiting>=subscriber->max_count){
		ESP_LOGW(tag,"messaged dropped for %s",str_or_unknown(subscriber->subscriber_name));
		single_message_t * dummy = (single_message_t *)xRingbufferReceive(subscriber->buf_handle, &item_size, pdMS_TO_TICKS(50));
		if (dummy== NULL) {
			ESP_LOGE(tag,"receive from buffer failed");
		}
		else {
			vRingbufferReturnItem(subscriber->buf_handle, (void *)dummy);
		}
	}
	UBaseType_t res =  xRingbufferSend(subscriber->buf_handle, message, message_size, pdMS_TO_TICKS(1000));
	if (res != pdTRUE) {
		ESP_LOGE(tag,"post to %s failed",str_or_unknown(subscriber->subscriber_name));
		return ESP_FAIL;
	}
	return ESP_OK;
}
void messaging_post_message(messaging_types type,messaging_classes msg_class, char *fmt, ...){
	single_message_t message={};
	messaging_list_t * cur=&top;
	va_list va;
	va_start(va, fmt);
	vsnprintf(message.message, sizeof(message.message), fmt, va);
	va_end(va);
	message.type = type;
	message.msg_class = msg_class;
	message.sent_time = esp_timer_get_time() / 1000;
	while(cur){
		messaging_post_to_queue(get_handle_ptr(cur),  &message, sizeof(single_message_t));
		cur = get_struct_ptr(cur->next);
	}
	return;

}
