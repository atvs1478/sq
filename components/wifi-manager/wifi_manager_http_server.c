/*
 *  Squeezelite for esp32
 *
 *  (c) Sebastien 2019
 *      Philippe G. 2019, philippe_44@outlook.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include  "http_server_handlers.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"

static const char TAG[] = "http_server";

static httpd_handle_t _server = NULL;
rest_server_context_t *rest_context = NULL;

void register_common_handlers(httpd_handle_t server){
	httpd_uri_t res_get = { .uri = "/res/*", .method = HTTP_GET, .handler = resource_filehandler, .user_ctx = rest_context };
	httpd_register_uri_handler(server, &res_get);

}
void register_regular_handlers(httpd_handle_t server){
	httpd_uri_t root_get = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = rest_context };
	httpd_register_uri_handler(server, &root_get);

	httpd_uri_t ap_get = { .uri = "/ap.json", .method = HTTP_GET, .handler = ap_get_handler, .user_ctx = rest_context };
	httpd_register_uri_handler(server, &ap_get);
	httpd_uri_t scan_get = { .uri = "/scan.json", .method = HTTP_GET, .handler = ap_scan_handler, .user_ctx = rest_context };
	httpd_register_uri_handler(server, &scan_get);
	httpd_uri_t config_get = { .uri = "/config.json", .method = HTTP_GET, .handler = config_get_handler, .user_ctx = rest_context };
	httpd_register_uri_handler(server, &config_get);
	httpd_uri_t status_get = { .uri = "/status.json", .method = HTTP_GET, .handler = status_get_handler, .user_ctx = rest_context };
	httpd_register_uri_handler(server, &status_get);

	httpd_uri_t config_post = { .uri = "/config.json", .method = HTTP_POST, .handler = config_post_handler, .user_ctx = rest_context };
	httpd_register_uri_handler(server, &config_post);
	httpd_uri_t connect_post = { .uri = "/connect.json", .method = HTTP_POST, .handler = connect_post_handler, .user_ctx = rest_context };
	httpd_register_uri_handler(server, &connect_post);

	httpd_uri_t reboot_ota_post = { .uri = "/reboot_ota.json", .method = HTTP_POST, .handler = reboot_ota_post_handler, .user_ctx = rest_context };
	httpd_register_uri_handler(server, &reboot_ota_post);

	httpd_uri_t reboot_post = { .uri = "/reboot.json", .method = HTTP_POST, .handler = reboot_post_handler, .user_ctx = rest_context };
	httpd_register_uri_handler(server, &reboot_post);

	httpd_uri_t recovery_post = { .uri = "/recovery.json", .method = HTTP_POST, .handler = recovery_post_handler, .user_ctx = rest_context };
	httpd_register_uri_handler(server, &recovery_post);

	httpd_uri_t connect_delete = { .uri = "/connect.json", .method = HTTP_DELETE, .handler = connect_delete_handler, .user_ctx = rest_context };
	httpd_register_uri_handler(server, &connect_delete);


	// from https://github.com/tripflex/wifi-captive-portal/blob/master/src/mgos_wifi_captive_portal.c
	// https://unix.stackexchange.com/questions/432190/why-isnt-androids-captive-portal-detection-triggering-a-browser-window
	 // Known HTTP GET requests to check for Captive Portal

	///kindle-wifi/wifiredirect.html Kindle when requested with com.android.captiveportallogin
	///kindle-wifi/wifistub.html Kindle before requesting with captive portal login window (maybe for detection?)


	httpd_uri_t connect_redirect_1 = { .uri = "/mobile/status.php", .method = HTTP_GET, .handler = redirect_200_ev_handler, .user_ctx = rest_context };// Android 8.0 (Samsung s9+)
	httpd_register_uri_handler(server, &connect_redirect_1);
	httpd_uri_t connect_redirect_2 = { .uri = "/generate_204", .method = HTTP_GET, .handler = redirect_200_ev_handler, .user_ctx = rest_context };// Android
	httpd_register_uri_handler(server, &connect_redirect_2);
	httpd_uri_t connect_redirect_3 = { .uri = "/gen_204", .method = HTTP_GET, .handler = redirect_ev_handler, .user_ctx = rest_context };// Android 9.0
	httpd_register_uri_handler(server, &connect_redirect_3);
	httpd_uri_t connect_redirect_4 = { .uri = "/ncsi.txt", .method = HTTP_GET, .handler = redirect_ev_handler, .user_ctx = rest_context };// Windows
	httpd_register_uri_handler(server, &connect_redirect_4);
	httpd_uri_t connect_redirect_5 = { .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = redirect_ev_handler, .user_ctx = rest_context }; // iOS 8/9
	httpd_register_uri_handler(server, &connect_redirect_5);
	httpd_uri_t connect_redirect_6 = { .uri = "/library/test/success.html", .method = HTTP_GET, .handler = redirect_ev_handler, .user_ctx = rest_context };// iOS 8/9
	httpd_register_uri_handler(server, &connect_redirect_6);
	httpd_uri_t connect_redirect_7 = { .uri = "/hotspotdetect.html", .method = HTTP_GET, .handler = redirect_ev_handler, .user_ctx = rest_context }; // iOS
	httpd_register_uri_handler(server, &connect_redirect_7);
	httpd_uri_t connect_redirect_8 = { .uri = "/success.txt", .method = HTTP_GET, .handler = redirect_ev_handler, .user_ctx = rest_context }; // OSX
	httpd_register_uri_handler(server, &connect_redirect_8);


	ESP_LOGD(TAG,"Registering default error handler for 404");
	httpd_register_err_handler(server, HTTPD_404_NOT_FOUND,&err_handler);

}

esp_err_t http_server_start()
{
	ESP_LOGI(TAG, "Initializing HTTP Server");
    rest_context = calloc(1, sizeof(rest_server_context_t));
    if(rest_context==NULL){
    	ESP_LOGE(TAG,"No memory for http context");
    	return ESP_FAIL;
    }
    strlcpy(rest_context->base_path, "/res/", sizeof(rest_context->base_path));

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    config.max_open_sockets = 8;
    config.uri_match_fn = httpd_uri_match_wildcard;
    //todo:  use the endpoint below to configure session token?
    // config.open_fn

    ESP_LOGI(TAG, "Starting HTTP Server");
    esp_err_t err= httpd_start(&_server, &config);
    if(err != ESP_OK){
    	ESP_LOGE_LOC(TAG,"Start server failed");
    }
    else {

    	register_common_handlers(_server);
    	register_regular_handlers(_server);
    }

    return err;
}


/* Function to free context */
void adder_free_func(void *ctx)
{
    ESP_LOGI(TAG, "/adder Free Context function called");
    free(ctx);
}


void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}





#if 0

if(strstr(line, "GET / ")) {
	netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
	netconn_write(conn, index_html_start, index_html_end- index_html_start, NETCONN_NOCOPY);
}
else if(strstr(line, "GET /code.js ")) {
	netconn_write(conn, http_js_hdr, sizeof(http_js_hdr) - 1, NETCONN_NOCOPY);
	netconn_write(conn, code_js_start, code_js_end - code_js_start, NETCONN_NOCOPY);
}
else if(strstr(line, "GET /style.css ")) {
	netconn_write(conn, http_css_hdr, sizeof(http_css_hdr) - 1, NETCONN_NOCOPY);
	netconn_write(conn, style_css_start, style_css_end - style_css_start, NETCONN_NOCOPY);
}
else if(strstr(line, "GET /jquery.js ")) {
	http_server_send_resource_file(conn,jquery_gz_start, jquery_gz_end, "text/javascript", "gzip" );
}
else if(strstr(line, "GET /popper.js ")) {
	http_server_send_resource_file(conn,popper_gz_start, popper_gz_end, "text/javascript", "gzip" );
}
else if(strstr(line, "GET /bootstrap.js ")) {
	http_server_send_resource_file(conn,bootstrap_js_gz_start, bootstrap_js_gz_end, "text/javascript", "gzip" );
}
else if(strstr(line, "GET /bootstrap.css ")) {
	http_server_send_resource_file(conn,bootstrap_css_gz_start, bootstrap_css_gz_end, "text/css", "gzip" );
}

//dynamic stuff
else if(strstr(line, "GET /scan.json ")) {
	ESP_LOGI(TAG,  "Starting wifi scan");
	wifi_manager_scan_async();
}
else if(strstr(line, "GET /ap.json ")) {
	/* if we can get the mutex, write the last version of the AP list */
	ESP_LOGI(TAG,  "Processing ap.json request");
	if(wifi_manager_lock_json_buffer(( TickType_t ) 10)) {
		netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY);
		char *buff = wifi_manager_alloc_get_ap_list_json();
		wifi_manager_unlock_json_buffer();
		if(buff!=NULL){
			netconn_write(conn, buff, strlen(buff), NETCONN_NOCOPY);
			free(buff);
		}
		else {
			ESP_LOGD(TAG,  "Error retrieving ap list json string. ");
			netconn_write(conn, http_503_hdr, sizeof(http_503_hdr) - 1, NETCONN_NOCOPY);
		}
	}
	else {
		netconn_write(conn, http_503_hdr, sizeof(http_503_hdr) - 1, NETCONN_NOCOPY);
		ESP_LOGE(TAG,   "http_server_netconn_serve: GET /ap.json failed to obtain mutex");
	}
	/* request a wifi scan */
	ESP_LOGI(TAG,  "Starting wifi scan");
	wifi_manager_scan_async();
	ESP_LOGI(TAG,  "Done serving ap.json");
}
else if(strstr(line, "GET /config.json ")) {
	ESP_LOGI(TAG,  "Serving config.json");
	ESP_LOGI(TAG,   "About to get config from flash");
	http_server_send_config_json(conn);
	ESP_LOGD(TAG,  "Done serving config.json");
}
else if(strstr(line, "POST /config.json ")) {
	ESP_LOGI(TAG,  "Serving POST config.json");
	int lenA=0;
	char * last_parm=save_ptr;
	char * next_parm=save_ptr;
	char  * last_parm_name=NULL;
	bool bErrorFound=false;
	bool bOTA=false;
	char * otaURL=NULL;
	// todo:  implement json body parsing
	//http_server_process_config(conn,save_ptr);

	while(last_parm!=NULL) {
		// Search will return
		ESP_LOGD(TAG,   "Getting parameters from X-Custom headers");
		last_parm = http_server_search_header(next_parm, "X-Custom-", &lenA, &last_parm_name,&next_parm,buf+buflen);
		if(last_parm!=NULL && last_parm_name!=NULL) {
			ESP_LOGI(TAG,   "http_server_netconn_serve: POST config.json, config %s=%s", last_parm_name, last_parm);
			if(strcmp(last_parm_name, "fwurl")==0) {
				// we're getting a request to do an OTA from that URL
				ESP_LOGW(TAG,   "Found OTA request!");
				otaURL=strdup(last_parm);
				bOTA=true;
			}
			else {
				ESP_LOGV(TAG,   "http_server_netconn_serve: POST config.json Storing parameter");
				if(config_set_value(NVS_TYPE_STR, last_parm_name , last_parm) != ESP_OK){
					ESP_LOGE(TAG,  "Unable to save nvs value.");
				}
			}
		}
		if(last_parm_name!=NULL) {
			free(last_parm_name);
			last_parm_name=NULL;
		}
	}
	if(bErrorFound) {
		netconn_write(conn, http_400_hdr, sizeof(http_400_hdr) - 1, NETCONN_NOCOPY); //400 invalid request
	}
	else {
		netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY); //200ok
		if(bOTA) {

#if RECOVERY_APPLICATION
			ESP_LOGW(TAG,   "Starting process OTA for url %s",otaURL);
#else
			ESP_LOGW(TAG,   "Restarting system to process OTA for url %s",otaURL);
#endif
			wifi_manager_reboot_ota(otaURL);
			free(otaURL);
		}
	}
	ESP_LOGI(TAG,  "Done Serving POST config.json");
}
else if(strstr(line, "POST /connect.json ")) {
	ESP_LOGI(TAG,   "http_server_netconn_serve: POST /connect.json");
	bool found = false;
	int lenS = 0, lenP = 0, lenN = 0;
	char *ssid = NULL, *password = NULL;
	ssid = http_server_get_header(save_ptr, "X-Custom-ssid: ", &lenS);
	password = http_server_get_header(save_ptr, "X-Custom-pwd: ", &lenP);
	char * new_host_name_b = http_server_get_header(save_ptr, "X-Custom-host_name: ", &lenN);
	if(lenN > 0){
		lenN++;
		char * new_host_name = malloc(lenN);
		strlcpy(new_host_name, new_host_name_b, lenN);
		if(config_set_value(NVS_TYPE_STR, "host_name", new_host_name) != ESP_OK){
			ESP_LOGE(TAG,  "Unable to save host name configuration");
		}
		free(new_host_name);
	}

	if(ssid && lenS <= MAX_SSID_SIZE && password && lenP <= MAX_PASSWORD_SIZE) {
		wifi_config_t* config = wifi_manager_get_wifi_sta_config();
		memset(config, 0x00, sizeof(wifi_config_t));
		memcpy(config->sta.ssid, ssid, lenS);
		memcpy(config->sta.password, password, lenP);
		ESP_LOGD(TAG,   "http_server_netconn_serve: wifi_manager_connect_async() call, with ssid: %s, password: %s", config->sta.ssid, config->sta.password);
		wifi_manager_connect_async();
		netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY); //200ok
		found = true;
	}
	else{
		ESP_LOGE(TAG,  "SSID or Password invalid");
	}


	if(!found) {
		/* bad request the authentification header is not complete/not the correct format */
		netconn_write(conn, http_400_hdr, sizeof(http_400_hdr) - 1, NETCONN_NOCOPY);
		ESP_LOGE(TAG,   "bad request the authentification header is not complete/not the correct format");
	}

	ESP_LOGI(TAG,   "http_server_netconn_serve: done serving connect.json");
}
else if(strstr(line, "DELETE /connect.json ")) {
	ESP_LOGI(TAG,   "http_server_netconn_serve: DELETE /connect.json");
	/* request a disconnection from wifi and forget about it */
	wifi_manager_disconnect_async();
	netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY); /* 200 ok */
	ESP_LOGI(TAG,   "http_server_netconn_serve: done serving DELETE /connect.json");
}
else if(strstr(line, "POST /reboot_ota.json ")) {
	ESP_LOGI(TAG,   "http_server_netconn_serve: POST reboot_ota.json");
	netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY); /* 200 ok */
	wifi_manager_reboot(OTA);
	ESP_LOGI(TAG,   "http_server_netconn_serve: done serving POST reboot_ota.json");
}
else if(strstr(line, "POST /reboot.json ")) {
	ESP_LOGI(TAG,   "http_server_netconn_serve: POST reboot.json");
	netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY); /* 200 ok */
	wifi_manager_reboot(RESTART);
	ESP_LOGI(TAG,   "http_server_netconn_serve: done serving POST reboot.json");
}
else if(strstr(line, "POST /recovery.json ")) {
	ESP_LOGI(TAG,   "http_server_netconn_serve: POST recovery.json");
	netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY); /* 200 ok */
	wifi_manager_reboot(RECOVERY);
	ESP_LOGI(TAG,   "http_server_netconn_serve: done serving POST recovery.json");
}
else if(strstr(line, "GET /status.json ")) {
	ESP_LOGI(TAG,  "Serving status.json");
	if(wifi_manager_lock_json_buffer(( TickType_t ) 10)) {
		char *buff = wifi_manager_alloc_get_ip_info_json();
		wifi_manager_unlock_json_buffer();
		if(buff) {
			netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY);
			netconn_write(conn, buff, strlen(buff), NETCONN_NOCOPY);
			free(buff);
		}
		else {
			netconn_write(conn, http_503_hdr, sizeof(http_503_hdr) - 1, NETCONN_NOCOPY);
		}

	}
	else {
		netconn_write(conn, http_503_hdr, sizeof(http_503_hdr) - 1, NETCONN_NOCOPY);
		ESP_LOGE(TAG,   "http_server_netconn_serve: GET /status failed to obtain mutex");
	}
	ESP_LOGI(TAG,  "Done Serving status.json");
}
else {
	netconn_write(conn, http_400_hdr, sizeof(http_400_hdr) - 1, NETCONN_NOCOPY);
	ESP_LOGE(TAG,   "bad request from host: %s, request %s",remote_address, line);
}
}
}
else {
ESP_LOGE(TAG,   "URL not found processing for remote host : %s",remote_address);
netconn_write(conn, http_404_hdr, sizeof(http_404_hdr) - 1, NETCONN_NOCOPY);
}
free(host);

}
#endif
