/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include "battery.h"
#include "led.h"
#include "monitor.h"

extern void battery_svc_init(void);
extern void monitor_svc_init(void);
extern void led_svc_init(void);

static const char TAG[] = "services";

/****************************************************************************************
 * 
 */
void services_init(void) {
	battery_svc_init();
	monitor_svc_init();
	led_svc_init();
}
