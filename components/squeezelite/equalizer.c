/* 
 *  Squeezelite for esp32
 *
 *  (c) Philippe G. 2020, philippe_44@outlook.com
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
#include "equalizer.h"
#include "esp_equalizer.h"
 
#define EQ_BANDS	10

static log_level loglevel = lINFO;
 
static struct {
	void *handle;
	float gain[EQ_BANDS];
	bool update;
} equalizer = { .update = true };
 
/****************************************************************************************
 * open equalizer
 */
void equalizer_open(u32_t sample_rate) {
	if (sample_rate != 11025 && sample_rate != 22050 && sample_rate != 44100 && sample_rate != 48000) {
		LOG_WARN("equalizer only supports 11025, 22050, 44100 and 48000 sample rates, not %u", sample_rate);
		return;
	}	
	
	equalizer.handle = esp_equalizer_init(2, sample_rate, EQ_BANDS, 0);
	equalizer.update = false;
    
	if (equalizer.handle) {
		LOG_INFO("equalizer initialized");
		for (int i = 0; i < EQ_BANDS; i++) {
			esp_equalizer_set_band_value(equalizer.handle, equalizer.gain[i], i, 0);
			esp_equalizer_set_band_value(equalizer.handle, equalizer.gain[i], i, 1);
		}
	} else {
		LOG_WARN("can't init equalizer");
	}	
}	

/****************************************************************************************
 * close equalizer
 */
void equalizer_close(void) {
	if (equalizer.handle) {
		esp_equalizer_uninit(equalizer.handle);
		equalizer.handle = NULL;
	}
}	

/****************************************************************************************
 * update equalizer gain
 */
void equalizer_update(s8_t *gain) {
	for (int i = 0; i < EQ_BANDS; i++) equalizer.gain[i] = gain[i];
	equalizer.update = true;
}

/****************************************************************************************
 * process equalizer 
 */
void equalizer_process(u8_t *buf, u32_t bytes, u32_t sample_rate) {
	// don't want to process with output locked, so tak ethe small risk to miss one parametric update
	if (equalizer.update) {
		equalizer_close();
		equalizer_open(sample_rate);
	}
	
	if (equalizer.handle) {
		esp_equalizer_process(equalizer.handle, buf, bytes, sample_rate, 2);
	}	
}