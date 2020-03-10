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
 
#pragma once

#include "esp_pthread.h"
#ifndef SQUEEZELITE_ESP32_RELEASE_URL
#define SQUEEZELITE_ESP32_RELEASE_URL "https://github.com/sle118/squeezelite-esp32/releases"
#endif
extern bool is_recovery_running;
extern void run_command(char * line);
extern  bool wait_for_wifi();
extern void console_start();
extern pthread_cond_t wifi_connect_suspend_cond;
extern pthread_t wifi_connect_suspend_mutex;
typedef enum {
	INFO,
	WARNING,
	ERROR
} message_severity_t;
extern void set_status_message(message_severity_t severity, const char * message);