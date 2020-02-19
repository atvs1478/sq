#include "sdkconfig.h"
#include "freertos/ringbuf.h"
#include "cJSON.h"
#pragma once
typedef enum {
	MESSAGING_INFO,
	MESSAGING_WARNING,
	MESSAGING_ERROR
} messaging_types;
typedef struct {
	void  * next;
	char subscriber_name[21];
	RingbufHandle_t buf_handle;
} messaging_list_t;

typedef struct {
	time_t sent_time;
	messaging_types type;
	char message[101];
} single_message_t;

cJSON *  messaging_retrieve_messages(RingbufHandle_t buf_handle);
RingbufHandle_t messaging_register_subscriber(uint8_t max_count, char * name);

void messaging_post_message(messaging_types type, char * fmt, ...);
