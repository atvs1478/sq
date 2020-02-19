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

static messaging_list_t top;
RingbufHandle_t messaging_create_ring_buffer(uint8_t max_count){
	RingbufHandle_t buf_handle = NULL;
	StaticRingbuffer_t *buffer_struct = (StaticRingbuffer_t *)heap_caps_malloc(sizeof(StaticRingbuffer_t), MALLOC_CAP_SPIRAM);
	if (buffer_struct != NULL) {
		size_t buf_size = (size_t )(sizeof(single_message_t)+8)*(size_t )(max_count>0?max_count:5); // no-split buffer requires an additional 8 bytes
		uint8_t *buffer_storage = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
		if (buffer_storage== NULL) {
			ESP_LOGE(tag,"Failed to allocate memory for messaging ring buffer !");
		}
		else {
			buf_handle = xRingbufferCreateStatic(buf_size, RINGBUF_TYPE_NOSPLIT, buffer_storage, buffer_struct);
			if (buf_handle == NULL) {
				ESP_LOGE(tag,"Failed to create messaging ring buffer !");
			}
		}
	}
	else {
		ESP_LOGE(tag,"Failed to create ring buffer for messaging!");
	}
	return buf_handle;
}

RingbufHandle_t messaging_register_subscriber(uint8_t max_count, char * name){
	messaging_list_t * cur=&top;
	while(cur->next){
		cur = cur->next;
	}
	cur->next=heap_caps_malloc(sizeof(messaging_list_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
	if(!cur->next){
		ESP_LOGE(tag,"Failed to allocate messaging subscriber entry!");
		return NULL;
	}
	memset(cur->next,0x00,sizeof(messaging_list_t));
	cur = cur->next;
	cur->buf_handle = messaging_create_ring_buffer(max_count);
	strncpy(cur->subscriber_name,name,sizeof(cur->subscriber_name));

	return cur->buf_handle;
}
esp_err_t messaging_service_init(){
	top.buf_handle = messaging_create_ring_buffer(max_count);
	if(!top.buf_handle){
		ESP_LOGE(tag, "messaging service init failed.");
	}
	strncpy(top.subscriber_name,"messaging");
	return (top.buf_handle!=NULL);
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
			ESP_LOGE(tag,"Failed to receive message from buffer!");
		}
		else {
			json_message = cJSON_CreateObject();
			cJSON_AddStringToObject(json_message, "message", message->message);
			cJSON_AddStringToObject(json_message, "type", message->message);
			cJSON_AddNumberToObject(json_message,"sent_time",message->sent_time);
			cJSON_AddNumberToObject(json_message,"current_time",esp_timer_get_time() / 1000);
			cJSON_AddItemToArray(json_messages,json_message);
			vRingbufferReturnItem(buf_handle, (void *)message);
		}
	}
	return json_messages;
}

void messaging_release_message(RingbufHandle_t buf_handle, single_message_t * message){

}
esp_err_t messageing_post_to_queue(messaging_list_t * subscriber, single_message_t * message){
	UBaseType_t res =  xRingbufferSend(subscriber->buf_handle, message, sizeof(message), pdMS_TO_TICKS(1000));
	if (res != pdTRUE) {
		ESP_LOGE(tag,"Failed to post message to subscriber %s",subscriber->subscriber_name);
		return ESP_FAIL;
	}
	return ESP_OK;
}
void messaging_post_message(messaging_types type, char *fmt, ...){
	single_message_t message={};
	messaging_list_t * cur=&top;
	va_list va;
	va_start(va, fmt);
	vsnprintf(message.message, sizeof(message.message), fmt, va);
	va_end(va);
	message.type = type;
	message.sent_time = esp_timer_get_time() / 1000;
	while(cur->next){
		messageing_post_to_queue(cur,  &message);
		cur = cur->next;
	}
	return;

}
