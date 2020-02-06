/* 
 *  (c) Philippe G. 2019, philippe_44@outlook.com
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

#include "squeezelite.h"
#include "audio_controls.h"

static log_level loglevel = lINFO;

static in_addr_t server_ip;
static u16_t server_hport;
static u16_t server_cport;
static u8_t mac[6];
static void	(*chained_notify)(in_addr_t, u16_t, u16_t);

static void cli_send_cmd(char *cmd);

static void lms_volume_up(void) {
	cli_send_cmd("button volup");
}

static void lms_volume_down(void) {
	cli_send_cmd("button voldown");
}

static void lms_toggle(void) {
	cli_send_cmd("pause");
}

static void lms_pause(void) {
	cli_send_cmd("pause 1");
}

static void lms_play(void) {
	cli_send_cmd("button play.single");
}

static void lms_stop(void) {
	cli_send_cmd("button stop");
}

static void lms_rew(void) {
	cli_send_cmd("button rew.repeat");
}

static void lms_fwd(void) {
	cli_send_cmd("button fwd.repeat");
}

static void lms_prev(void) {
	cli_send_cmd("button rew");
}

static void lms_next(void) {
	cli_send_cmd("button fwd");
}

static void lms_up(void) {
	cli_send_cmd("button arrow_up");
}

static void lms_down(void) {
	cli_send_cmd("button arrow_down");
}

static void lms_left(void) {
	cli_send_cmd("button arrow_left");
}

static void lms_right(void) {
	cli_send_cmd("button arrow_right");
}

static void lms_knob_left(void) {
	cli_send_cmd("button knob_left");
}

static void lms_knob_right(void) {
	cli_send_cmd("button knob_right");
}

static void lms_knob_push(void) {
	cli_send_cmd("button knob_push");
}

const actrls_t LMS_controls = {
	lms_volume_up, lms_volume_down,	// volume up, volume down
	lms_toggle, lms_play,	// toggle, play
	lms_pause, lms_stop,	// pause, stop
	lms_rew, lms_fwd,		// rew, fwd
	lms_prev, lms_next,		// prev, next
	lms_up, lms_down,
	lms_left, lms_right, 
	lms_knob_left, lms_knob_right, lms_knob_push,
};

/****************************************************************************************
 * 
 */
static void cli_send_cmd(char *cmd) {
	char packet[64];
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	int len, sock = socket(AF_INET, SOCK_STREAM, 0);
	
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = server_ip;
	addr.sin_port = htons(server_cport);

	if (connect(sock, (struct sockaddr *) &addr, addrlen) < 0) {
		LOG_ERROR("unable to connect to server %s:%hu with cli", inet_ntoa(server_ip), server_cport);
		return;
	}

	len = sprintf(packet, "%02x:%02x:%02x:%02x:%02x:%02x %s\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], cmd);
	LOG_DEBUG("sending command %s at %s:%hu", packet, inet_ntoa(server_ip), server_cport);
		
	if (send(sock, packet, len, 0) < 0) {
		LOG_WARN("cannot send CLI %s", packet);
	}

	closesocket(sock);
}

/****************************************************************************************
 * Notification when server changes
 */
static void notify(in_addr_t ip, u16_t hport, u16_t cport) {
	server_ip = ip;
	server_hport = hport;
	server_cport = cport;
	LOG_INFO("notified server %s hport %hu cport %hu", inet_ntoa(ip), hport, cport);
	if (chained_notify) (*chained_notify)(ip, hport, cport);
}

/****************************************************************************************
 * Initialize controls - shall be called once from output_init_embedded
 */
void sb_controls_init(void) {
	LOG_INFO("initializing CLI controls");
	get_mac(mac);
	actrls_set_default(LMS_controls, NULL);
	chained_notify = server_notify;
	server_notify = notify;
}