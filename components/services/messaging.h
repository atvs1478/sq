#include "sdkconfig.h"
#include "freertos/ringbuf.h"
#include "cJSON.h"
#pragma once
typedef enum {
	MESSAGING_INFO,
	MESSAGING_WARNING,
	MESSAGING_ERROR
} messaging_types;
typedef enum {
	MESSAGING_CLASS_OTA,
	MESSAGING_CLASS_SYSTEM,
	MESSAGING_CLASS_STATS
} messaging_classes;

typedef struct messaging_list_t *messaging_handle_t;

typedef struct {
	time_t sent_time;
	messaging_types type;
	messaging_classes msg_class;
	size_t msg_size;
	char message[];
} single_message_t;

cJSON *  messaging_retrieve_messages(RingbufHandle_t buf_handle);
messaging_handle_t messaging_register_subscriber(uint8_t max_count, char * name);
esp_err_t messaging_post_to_queue(messaging_handle_t subscriber_handle, single_message_t * message, size_t message_size);
void messaging_post_message(messaging_types type,messaging_classes msg_class, char * fmt, ...);
cJSON *  messaging_retrieve_messages(RingbufHandle_t buf_handle);
single_message_t *  messaging_retrieve_message(RingbufHandle_t buf_handle);
void messaging_service_init();
