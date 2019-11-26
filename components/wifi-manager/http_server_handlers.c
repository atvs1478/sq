/*
Copyright (c) 2017-2019 Tony Pottier

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

@file http_server.c
@author Tony Pottier
@brief Defines all functions necessary for the HTTP server to run.

Contains the freeRTOS task for the HTTP listener and all necessary support
function to process requests, decode URLs, serve files, etc. etc.

@note http_server task cannot run without the wifi_manager task!
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include "http_server_handlers.h"

#include "esp_http_server.h"
#include "cmd_system.h"
#include <inttypes.h>
#include "squeezelite-ota.h"
#include "nvs_utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"

#define HTTP_STACK_SIZE	(5*1024)
#define FREE_AND_NULL(p) if(p!=NULL){ free(p); p=NULL;}

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "http_server";
/* @brief task handle for the http server */
static TaskHandle_t task_http_server = NULL;
static StaticTask_t task_http_buffer;
#if RECOVERY_APPLICATION
static StackType_t task_http_stack[HTTP_STACK_SIZE];
#else
static StackType_t EXT_RAM_ATTR task_http_stack[HTTP_STACK_SIZE];
#endif
SemaphoreHandle_t http_server_config_mutex = NULL;

#define AUTH_TOKEN_SIZE 50
typedef struct session_context {
    char * auth_token;
    bool authenticated;
} session_context_t;




/**
 * @brief embedded binary data.
 * @see file "component.mk"
 * @see https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#embedding-binary-data
 */
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[]   asm("_binary_style_css_end");
extern const uint8_t jquery_gz_start[] asm("_binary_jquery_min_js_gz_start");
extern const uint8_t jquery_gz_end[] asm("_binary_jquery_min_js_gz_end");
extern const uint8_t popper_gz_start[] asm("_binary_popper_min_js_gz_start");
extern const uint8_t popper_gz_end[] asm("_binary_popper_min_js_gz_end");
extern const uint8_t bootstrap_js_gz_start[] asm("_binary_bootstrap_min_js_gz_start");
extern const uint8_t bootstrap_js_gz_end[] asm("_binary_bootstrap_min_js_gz_end");
extern const uint8_t bootstrap_css_gz_start[] asm("_binary_bootstrap_min_css_gz_start");
extern const uint8_t bootstrap_css_gz_end[] asm("_binary_bootstrap_min_css_gz_end");
extern const uint8_t code_js_start[] asm("_binary_code_js_start");
extern const uint8_t code_js_end[] asm("_binary_code_js_end");
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
//
//
///* const http headers stored in ROM */
//const static char http_hdr_template[] = "HTTP/1.1 200 OK\nContent-type: %s\nAccept-Ranges: bytes\nContent-Length: %d\nContent-Encoding: %s\nAccess-Control-Allow-Origin: *\n\n";
//const static char http_html_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/html\nAccess-Control-Allow-Origin: *\nAccept-Encoding: identity\n\n";
//const static char http_css_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/css\nCache-Control: public, max-age=31536000\nAccess-Control-Allow-Origin: *\n\n";
//const static char http_js_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\nAccess-Control-Allow-Origin: *\n\n";
//const static char http_400_hdr[] = "HTTP/1.1 400 Bad Request\nContent-Length: 0\n\n";
//const static char http_404_hdr[] = "HTTP/1.1 404 Not Found\nContent-Length: 0\n\n";
//const static char http_503_hdr[] = "HTTP/1.1 503 Service Unavailable\nContent-Length: 0\n\n";
//const static char http_ok_json_no_cache_hdr[] = "HTTP/1.1 200 OK\nContent-type: application/json\nCache-Control: no-store, no-cache, must-revalidate, max-age=0\nPragma: no-cache\nAccess-Control-Allow-Origin: *\nAccept-Encoding: identity\n\n";
//const static char http_redirect_hdr_start[] = "HTTP/1.1 302 Found\nLocation: http://";
//const static char http_redirect_hdr_end[] = "/\n\n";


/* Custom function to free context */
void free_ctx_func(void *ctx)
{
    if(ctx){
    	if(ctx->auth_token) free(auth_token);
    	free(ctx);
    }
}

bool is_user_authenticated(httpd_req_t *req){
	    if (! req->sess_ctx) {
	        req->sess_ctx = malloc(sizeof(session_context_t));
	        memset(req->sess_ctx,0x00,sizeof(session_context_t));
	        req->free_ctx = free_ctx_func;
	    }
	    session_context_t *ctx_data = (session_context_t*)req->sess_ctx;
	    if(ctx_data->authenticated){
	    	ESP_LOGD(TAG,"User is authenticated.");
	    	return true;
	    }

	    // todo:  ask for user to authenticate
	    return false;
}



/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    } else if (IS_FILE_EXT(filename, ".css")) {
        return httpd_resp_set_type(req, "text/css");
    } else if (IS_FILE_EXT(filename, ".js")) {
        return httpd_resp_set_type(req, "text/javascript");
    } else if (IS_FILE_EXT(filename, ".json")) {
        return httpd_resp_set_type(req, "application/json");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}
static esp_err_t set_content_type_from_req(httpd_req_t *req)
{
	char filepath[FILE_PATH_MAX];
	const char *filename = get_path_from_uri(filepath, "/" ,
										req->uri, sizeof(filepath));
   if (!filename) {
	   ESP_LOGE(TAG, "Filename is too long");
	   /* Respond with 500 Internal Server Error */
	   httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
	   return ESP_FAIL;
   }

   /* If name has trailing '/', respond with directory contents */
   if (filename[strlen(filename) - 1] == '/') {
	   httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Browsing files forbidden.");
	   return ESP_FAIL;
   }
   return ESP_OK;
}


esp_err_t root_get_handler(httpd_req_t *req){
    ESP_LOGI(TAG, "serving [%s]", req->uri);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Accept-Encoding", "identity");

    if(!is_user_authenticated(httpd_req_t *req)){
    	// todo:  send password entry page and return
    }
    const size_t file_size = (index_html_end - index_html_start);
	esp_err_t err = set_content_type_from_req(req);
	if(err == ESP_OK){
		httpd_resp_send(req, (const char *)index_html_start, file_size);
	}
    return err;
}

esp_err_t resource_filehandler(httpd_req_t *req){
    char filepath[FILE_PATH_MAX];
   ESP_LOGI(TAG, "serving [%s]", req->uri);

   const char *filename = get_path_from_uri(filepath, "/res/" ,
											req->uri, sizeof(filepath));

   if (!filename) {
	   ESP_LOGE(TAG, "Filename is too long");
	   /* Respond with 500 Internal Server Error */
	   httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
	   return ESP_FAIL;
   }

   /* If name has trailing '/', respond with directory contents */
   if (filename[strlen(filename) - 1] == '/') {
	   httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Browsing files forbidden.");
	   return ESP_FAIL;
   }

   if(strstr(line, "GET /code.js ")) {
	    const size_t file_size = (code_js_end - code_js_start);
	    set_content_type_from_file(req, filename);
	    httpd_resp_send(req, (const char *)code_js_start, file_size);
	}
	else if(strstr(line, "GET /style.css ")) {
		set_content_type_from_file(req, filename);
	    const size_t file_size = (style_css_end - style_css_start);
	    httpd_resp_send(req, (const char *)style_css_start, file_size);
	} else if(strstr(line, "GET /jquery.js ")) {
		set_content_type_from_file(req, filename);
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
	    const size_t file_size = (jquery_gz_end - jquery_gz_start);
	    httpd_resp_send(req, (const char *)jquery_gz_start, file_size);
	}else if(strstr(line, "GET /popper.js")) {
		set_content_type_from_file(req, filename);
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
	    const size_t file_size = (popper_gz_end - popper_gz_start);
	    httpd_resp_send(req, (const char *)popper_gz_start, file_size);
	}
	else if(strstr(line, "GET /bootstrap.js ")) {
			set_content_type_from_file(req, filename);
			httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		    const size_t file_size = (bootstrap_js_gz_end - bootstrap_js_gz_start);
		    httpd_resp_send(req, (const char *)bootstrap_js_gz_start, file_size);
		}
	else if(strstr(line, "GET /bootstrap.css")) {
			set_content_type_from_file(req, filename);
			httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		    const size_t file_size = (bootstrap_css_gz_end - bootstrap_css_gz_start);
		    httpd_resp_send(req, (const char *)bootstrap_css_gz_start, file_size);
		}
	else {
	   ESP_LOGE(TAG, "Unknown resource: %s", filepath);
	   /* Respond with 404 Not Found */
	   httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
	   return ESP_FAIL;
   }
   ESP_LOGI(TAG, "Resource sending complete");
   return ESP_OK;

}
esp_err_t ap_scan_handler(httpd_req_t *req){
    const char empty[] = "{}";
	ESP_LOGI(TAG, "serving [%s]", req->uri);
    if(!is_user_authenticated(httpd_req_t *req)){
    	// todo:  redirect to login page
    	// return ESP_OK;
    }
	wifi_manager_scan_async();
	esp_err_t err = set_content_type_from_req(req);
	if(err == ESP_OK){
		httpd_resp_send(req, (const char *)empty, strlen(empty));
	}
	return err;
}
esp_err_t ap_get_handler(httpd_req_t *req){
    ESP_LOGI(TAG, "serving [%s]", req->uri);
    if(!is_user_authenticated(httpd_req_t *req)){
    	// todo:  redirect to login page
    	// return ESP_OK;
    }
    /* if we can get the mutex, write the last version of the AP list */
	esp_err_t err = set_content_type_from_req(req);
	if(err == ESP_OK wifi_manager_lock_json_buffer(( TickType_t ) 10)){
		char *buff = wifi_manager_alloc_get_ap_list_json();
		wifi_manager_unlock_json_buffer();
		if(buff!=NULL){
			httpd_resp_send(req, (const char *)buff, strlen(buff));
			free(buff);
		}
		else {
			ESP_LOGD(TAG,  "Error retrieving ap list json string. ");
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Unable to retrieve AP list");
		}
	}
	else {
		httpd_resp_send_err(req, HTTPD_503_NOT_FOUND, "AP list unavailable");
		ESP_LOGE(TAG,   "GET /ap.json failed to obtain mutex");
	}
	return err;
}

esp_err_t config_get_handler(httpd_req_t *req){
    ESP_LOGI(TAG, "serving [%s]", req->uri);
    if(!is_user_authenticated(httpd_req_t *req)){
    	// todo:  redirect to login page
    	// return ESP_OK;
    }
	esp_err_t err = set_content_type_from_req(req);
	if(err == ESP_OK){
		char * json = config_alloc_get_json(false);
		if(json==NULL){
			ESP_LOGD(TAG,  "Error retrieving config json string. ");
			httpd_resp_send_err(req, HTTPD_503_NOT_FOUND, "Error retrieving configuration object");
			err=ESP_FAIL;
		}
		else {
			ESP_LOGD(TAG,  "config json : %s",json );
			httpd_resp_send(req, (const char *)json, strlen(json));
			free(json);
		}
	}
	return err;
}
esp_err_t post_handler_buff_receive(httpd_req_t * req){
    esp_err_t err = ESP_OK;

    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }

    buf[total_len] = '\0';
}
esp_err_t config_post_handler(httpd_req_t *req){
    ESP_LOGI(TAG, "serving [%s]", req->uri);
	bool bOTA=false;
	char * otaURL=NULL;
    esp_err_t err = post_handler_buff_receive(req);
    if(err!=ESP_OK){
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return err;
    }
    if(!is_user_authenticated(httpd_req_t *req)){
    	// todo:  redirect to login page
    	// return ESP_OK;
    }
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    cJSON *root = cJSON_Parse(buf);
	cJSON *item=root->next;
	while (item && err == ESP_OK)
	{
		cJSON *prev_item = item;
		item=item->next;
		if(prev_item->name==NULL) {
			ESP_LOGE(TAG,"Config value does not have a name");
			httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Malformed config json.  Value does not have a name.");
			err = ESP_FAIL;
		}
		if(err == ESP_OK){
			ESP_LOGI(TAG,"Found config value name [%s]", prev_item->name);
			nvs_type_t item_type=  config_get_item_type(prev_item);
			if(item_type!=0){
				void * val = config_safe_alloc_get_entry_value(item_type, prev_item);
				if(val!=NULL){
					if(strcmp(prev_item->name, "fwurl")==0) {
						if(item_type!=NVS_TYPE_STR){
							ESP_LOGE(TAG,"Firmware url should be type %d. Found type %d instead.",NVS_TYPE_STR,item_type );
							httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Malformed config json.  Wrong type for firmware URL.");
							err = ESP_FAIL;
						}
						else {
							// we're getting a request to do an OTA from that URL
							ESP_LOGW(TAG,   "Found OTA request!");
							otaURL=strdup(val);
							bOTA=true;
						}
					}
					else {
						if(config_set_value(item_type, last_parm_name , last_parm) != ESP_OK){
							ESP_LOGE(TAG,"Unable to store value for [%s]", prev_item->name);
							httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Unable to store config value");
							err = ESP_FAIL;
						}
						else {
							ESP_LOGI(TAG,"Successfully set value for [%s]",prev_item->name);
						}
					}
					free(val);
				}
				else {
					ESP_LOGE(TAG,"Value not found for [%s]", prev_item->name);
					httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Malformed config json.  Missing value for entry.");
					err = ESP_FAIL;
				}
			}
			else {
				ESP_LOGE(TAG,"Unable to determine the type of config value [%s]", prev_item->name);
				httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Malformed config json.  Missing value for entry.");
				err = ESP_FAIL;
			}
		}
	}


	if(err==ESP_OK){
		set_content_type_from_req(req);
		httpd_resp_sendstr(req, "{ok}");
	}
    cJSON_Delete(root);
	if(bOTA) {

#if RECOVERY_APPLICATION
		ESP_LOGW(TAG,   "Starting process OTA for url %s",otaURL);
#else
		ESP_LOGW(TAG,   "Restarting system to process OTA for url %s",otaURL);
#endif
		wifi_manager_reboot_ota(otaURL);
		free(otaURL);
	}
    return err;

}
esp_err_t connect_post_handler(httpd_req_t *req){
    ESP_LOGI(TAG, "serving [%s]", req->uri);
    char success[]="{}";
    char * ssid=NULL;
    char * password=NULL;
    char * host_name;
	set_content_type_from_req(req);
	esp_err_t err = post_handler_buff_receive(req);
	if(err!=ESP_OK){
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
		return err;
	}
	char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    if(!is_user_authenticated(httpd_req_t *req)){
    	// todo:  redirect to login page
    	// return ESP_OK;
    }
	cJSON *root = cJSON_Parse(buf);

	if(root==NULL){
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
		return ESP_FAIL;
	}

	cJSON * ssid_object = cJSON_GetObjectItem(root, "ssid");
	if(ssid_object !=NULL){
		ssid = (char *)config_safe_alloc_get_entry_value(ssid_object);
	}
	cJSON * password_object = cJSON_GetObjectItem(root, "pwd");
	if(password_object !=NULL){
		password = (char *)config_safe_alloc_get_entry_value(password_object);
	}
	cJSON * host_name_object = cJSON_GetObjectItem(root, "host_name");
	if(host_name_object !=NULL){
		host_name = (char *)config_safe_alloc_get_entry_value(host_name_object);
	}
	cJSON_Delete(root);

	if(host_name!=NULL){
		if(config_set_value(NVS_TYPE_STR, "host_name", host_name) != ESP_OK){
			ESP_LOGW(TAG,  "Unable to save host name configuration");
		}
	}

	if(ssid !=NULL && strlen(ssid) <= MAX_SSID_SIZE && strlen(password) <= MAX_PASSWORD_SIZE  ){
		wifi_config_t* config = wifi_manager_get_wifi_sta_config();
		memset(config, 0x00, sizeof(wifi_config_t));
		strlcpy(config->sta.ssid, ssid, sizeof(config->sta.ssid)+1);
		if(password){
			strlcpy(config->sta.password, password, sizeof(config->sta.password)+1);
		}
		ESP_LOGD(TAG,   "http_server_netconn_serve: wifi_manager_connect_async() call, with ssid: %s, password: %s", config->sta.ssid, config->sta.password);
		wifi_manager_connect_async();
		httpd_resp_send(req, (const char *)success, strlen(success));
	}
	else {
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Malformed json. Missing or invalid ssid/password.");
		err = ESP_FAIL;
	}
	FREE_AND_NULL(ssid);
	FREE_AND_NULL(password);
	FREE_AND_NULL(host_name);
	return err;
}
esp_err_t connect_delete_handler(httpd_req_t *req){
	char success[]="{}";
    ESP_LOGI(TAG, "serving [%s]", req->uri);
    if(!is_user_authenticated(httpd_req_t *req)){
    	// todo:  redirect to login page
    	// return ESP_OK;
    }
	set_content_type_from_req(req);
	httpd_resp_send(req, (const char *)success, strlen(success));
	wifi_manager_disconnect_async();

    return ESP_OK;
}
esp_err_t reboot_ota_post_handler(httpd_req_t *req){
	char success[]="{}";
	ESP_LOGI(TAG, "serving [%s]", req->uri);
    if(!is_user_authenticated(httpd_req_t *req)){
    	// todo:  redirect to login page
    	// return ESP_OK;
    }
	set_content_type_from_req(req);
	httpd_resp_send(req, (const char *)success, strlen(success));
	wifi_manager_reboot(OTA);
    return ESP_OK;
}
esp_err_t reboot_post_handler(httpd_req_t *req){
    ESP_LOGI(TAG, "serving [%s]", req->uri);
    char success[]="{}";
    if(!is_user_authenticated(httpd_req_t *req)){
    	// todo:  redirect to login page
    	// return ESP_OK;
    }
	set_content_type_from_req(req);
	httpd_resp_send(req, (const char *)success, strlen(success));
	wifi_manager_reboot(RESTART);
	return ESP_OK;
}
esp_err_t recovery_post_handler(httpd_req_t *req){
    ESP_LOGI(TAG, "serving [%s]", req->uri);
    char success[]="{}";
    if(!is_user_authenticated(httpd_req_t *req)){
    	// todo:  redirect to login page
    	// return ESP_OK;
    }
	set_content_type_from_req(req);
	httpd_resp_send(req, (const char *)success, strlen(success));
	wifi_manager_reboot(RECOVERY);
	return ESP_OK;
}
esp_err_t status_post_handler(httpd_req_t *req){
    ESP_LOGI(TAG, "serving [%s]", req->uri);
    char success[]="{}";
    if(!is_user_authenticated(httpd_req_t *req)){
    	// todo:  redirect to login page
    	// return ESP_OK;
    }
	set_content_type_from_req(req);

	if(wifi_manager_lock_json_buffer(( TickType_t ) 10)) {
		char *buff = wifi_manager_alloc_get_ip_info_json();
		wifi_manager_unlock_json_buffer();
		if(buff) {
			httpd_resp_send(req, (const char *)buff, strlen(buff));
			free(buff);
		}
		else {
			httpd_resp_send_err(req, HTTPD_503_NOT_FOUND, "Empty status object");
		}
	}
	else {
		httpd_resp_send_err(req, HTTPD_503_NOT_FOUND, "Error retrieving status object");
	}

	return ESP_OK;
}










//void http_server_netconn_serve(struct netconn *conn) {
//
//	struct netbuf *inbuf;
//	char *buf = NULL;
//	u16_t buflen;
//	err_t err;
//	ip_addr_t remote_add;
//	u16_t port;
//	ESP_LOGV(TAG,  "Serving page.  Getting device AP address.");
//	const char new_line[2] = "\n";
//	char * ap_ip_address= config_alloc_get_default(NVS_TYPE_STR, "ap_ip_address", DEFAULT_AP_IP, 0);
//	if(ap_ip_address==NULL){
//		ESP_LOGE(TAG,  "Unable to retrieve default AP IP Address");
//		netconn_write(conn, http_503_hdr, sizeof(http_503_hdr) - 1, NETCONN_NOCOPY);
//		netconn_close(conn);
//		return;
//	}
//	ESP_LOGV(TAG,  "Getting remote device IP address.");
//	netconn_getaddr(conn,	&remote_add,	&port,	0);
//	char * remote_address = strdup(ip4addr_ntoa(ip_2_ip4(&remote_add)));
//	ESP_LOGD(TAG,  "Local Access Point IP address is: %s. Remote device IP address is %s. Receiving request buffer", ap_ip_address, remote_address);
//	err = netconn_recv(conn, &inbuf);
//	if(err == ERR_OK) {
//		ESP_LOGV(TAG,  "Getting data buffer.");
//		netbuf_data(inbuf, (void**)&buf, &buflen);
//		dump_net_buffer(buf, buflen);
//		int lenH = 0;
//		/* extract the first line of the request */
//		char *save_ptr = buf;
//		char *line = strtok_r(save_ptr, new_line, &save_ptr);
//		char *temphost = http_server_get_header(save_ptr, "Host: ", &lenH);
//		char * host = malloc(lenH+1);
//		memset(host,0x00,lenH+1);
//		if(lenH>0){
//			strlcpy(host,temphost,lenH+1);
//		}
//		ESP_LOGD(TAG,  "http_server_netconn_serve Host: [%s], host: [%s], Processing line [%s]",remote_address,host,line);
//
//		if(line) {
//
//			/* captive portal functionality: redirect to access point IP for HOST that are not the access point IP OR the STA IP */
//			const char * host_name=NULL;
//			if((err=tcpip_adapter_get_hostname(TCPIP_ADAPTER_IF_STA, &host_name )) !=ESP_OK) {
//				ESP_LOGE(TAG,  "Unable to get host name. Error: %s",esp_err_to_name(err));
//			}
//			else {
//				ESP_LOGI(TAG,"System host name %s, http requested host: %s.",host_name, host);
//			}
//
//			/* determine if Host is from the STA IP address */
//			wifi_manager_lock_sta_ip_string(portMAX_DELAY);
//			bool access_from_sta_ip = lenH > 0?strcasestr(host, wifi_manager_get_sta_ip_string()):false;
//			wifi_manager_unlock_sta_ip_string();
//			bool access_from_host_name = (host_name!=NULL) && strcasestr(host,host_name);
//
//			if(lenH > 0 && !strcasestr(host, ap_ip_address) && !(access_from_sta_ip || access_from_host_name)) {
//				ESP_LOGI(TAG,  "Redirecting host [%s] to AP IP Address : %s",remote_address, ap_ip_address);
//				netconn_write(conn, http_redirect_hdr_start, sizeof(http_redirect_hdr_start) - 1, NETCONN_NOCOPY);
//				netconn_write(conn, ap_ip_address, strlen(ap_ip_address), NETCONN_NOCOPY);
//				netconn_write(conn, http_redirect_hdr_end, sizeof(http_redirect_hdr_end) - 1, NETCONN_NOCOPY);
//			}
//			else {
//                //static stuff
//
//}



