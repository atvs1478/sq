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

#include "freertos/FreeRTOS.h"
#include "driver/i2s.h"

typedef enum { ADAC_ON = 0, ADAC_STANDBY, ADAC_OFF } adac_power_e;

struct adac_s {
	char *model;
	bool (*init)(char *config, int i2c_port_num);
	void (*deinit)(void);
	void (*power)(adac_power_e mode);
	void (*speaker)(bool active);
	void (*headset)(bool active);
	bool (*volume)(unsigned left, unsigned right);
};

extern const struct adac_s dac_tas57xx;
extern const struct adac_s dac_ac101;
extern const struct adac_s dac_external;
