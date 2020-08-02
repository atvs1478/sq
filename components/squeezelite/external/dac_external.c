/* 
 *  Squeezelite for esp32
 *
 *  (c) Sebastien 2019
 *      Philippe G. 2019, philippe_44@outlook.com
 *
 *  This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 *
 */
 
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2s.h>
#include "esp_log.h"
#include "config.h"
#include "adac.h"

static const char TAG[] = "DAC external";

static bool init(char *config, int i2c_port_num);
static void deinit(void) { };
static void speaker(bool active) { };
static void headset(bool active) { } ;
static void volume(unsigned left, unsigned right) { };
static void power(adac_power_e mode) { };

const struct adac_s dac_external = { "i2s", init, deinit, power, speaker, headset, volume };

static bool init(char *config, int i2c_port_num) { 
	return true;	
}

