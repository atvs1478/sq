#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "mdns.h"
#include "nvs.h"
#include "tcpip_adapter.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_pthread.h"
#include "esp_system.h"
#include "freertos/timers.h"
#include "nvs_utilities.h"
#include "raop.h"
#include "audio_controls.h"

#include "log_util.h"

#include "trace.h"

#ifndef CONFIG_AIRPLAY_NAME
#define CONFIG_AIRPLAY_NAME		"ESP32-AirPlay"
#endif

log_level	raop_loglevel = lINFO;
log_level	util_loglevel;

static log_level *loglevel = &raop_loglevel;
static struct raop_ctx_s *raop;

static void raop_volume_up(void) {
	raop_cmd(raop, RAOP_VOLUME_UP, NULL);
	LOG_INFO("AirPlay volume up");
}

static void raop_volume_down(void) {
	raop_cmd(raop, RAOP_VOLUME_DOWN, NULL);
	LOG_INFO("AirPlay volume down");
}

static void raop_toggle(void) {
	raop_cmd(raop, RAOP_TOGGLE, NULL);
	LOG_INFO("AirPlay play/pause");
}

static void raop_pause(void) {
	raop_cmd(raop, RAOP_PAUSE, NULL);
	LOG_INFO("AirPlay pause");
}

static void raop_play(void) {
	raop_cmd(raop, RAOP_PLAY, NULL);
	LOG_INFO("AirPlay play");
}

static void raop_stop(void) {
	raop_cmd(raop, RAOP_STOP, NULL);
	LOG_INFO("AirPlay stop");
}

static void raop_prev(void) {
	raop_cmd(raop, RAOP_PREV, NULL);
	LOG_INFO("AirPlay previous");
}

static void raop_next(void) {
	raop_cmd(raop, RAOP_NEXT, NULL);
	LOG_INFO("AirPlay next");
}

const static actrls_t controls = {
	raop_volume_up, raop_volume_down,	// volume up, volume down
	raop_toggle, raop_play,				// toggle, play
	raop_pause, raop_stop,				// pause, stop
	NULL, NULL,							// rew, fwd
	raop_prev, raop_next,				// prev, next
};

/****************************************************************************************
 * Airplay taking/giving audio system's control 
 */
void raop_master(bool on) {
	if (on) actrls_set(controls);
	else actrls_unset();
}

/****************************************************************************************
 * Airplay sink de-initialization
 */
void raop_sink_deinit(void) {
	raop_delete(raop);
	mdns_free();
}	

/****************************************************************************************
 * Airplay sink initialization
 */
void raop_sink_init(raop_cmd_cb_t cmd_cb, raop_data_cb_t data_cb) {
    const char *hostname;
	char sink_name[64-6] = CONFIG_AIRPLAY_NAME;
	tcpip_adapter_ip_info_t ipInfo; 
	struct in_addr host;
   	
	// get various IP info
	tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);
	tcpip_adapter_get_hostname(TCPIP_ADAPTER_IF_STA, &hostname);
	host.s_addr = ipInfo.ip.addr;

    // initialize mDNS
    ESP_ERROR_CHECK( mdns_init() );
    ESP_ERROR_CHECK( mdns_hostname_set(hostname) );
        
    char * sink_name_buffer= (char *)config_alloc_get(NVS_TYPE_STR,"airplay_name");
    if(sink_name_buffer != NULL){
    	memset(sink_name, 0x00, sizeof(sink_name));
    	strncpy(sink_name,sink_name_buffer,sizeof(sink_name)-1 );
    	free(sink_name_buffer);
    }

	LOG_INFO( "mdns hostname set to: [%s] with servicename %s", hostname, sink_name);

    // create RAOP instance, latency is set by controller
	uint8_t mac[6];	
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
	raop = raop_create(host, sink_name, mac, 0, cmd_cb, data_cb);
}

/****************************************************************************************
 * Airplay forced disconnection
 */
void raop_disconnect(void) {
	LOG_INFO("forced disconnection");
	raop_cmd(raop, RAOP_STOP, NULL);
	actrls_unset();
}
