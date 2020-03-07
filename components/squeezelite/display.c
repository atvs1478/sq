/* 
 *  (c) 2004,2006 Richard Titmuss for SlimProtoLib 
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

#include <ctype.h>
#include <math.h>
#include "esp_dsp.h"
#include "squeezelite.h"
#include "slimproto.h"
#include "display.h"
#include "gds.h"
#include "gds_text.h"
#include "gds_draw.h"

#pragma pack(push, 1)

struct grfb_packet {
	char  opcode[4];
	s16_t  brightness;
};

struct grfe_packet {
	char  opcode[4];
	u16_t offset;
	u8_t  transition;
	u8_t  param;
};

struct grfs_packet {
	char  opcode[4];
	u8_t  screen;	
	u8_t  direction;	// 1=left, 2=right
	u32_t pause;		// in ms	
	u32_t speed;		// in ms
	u16_t by;			// # of pixel of scroll step
	u16_t mode;			// 0=continuous, 1=once and stop, 2=once and end
	u16_t width;		// total width of animation
	u16_t offset;		// offset if multiple packets are sent
};

struct grfg_packet {
	char  opcode[4];
	u16_t  screen;	
	u16_t  width;		// # of pixels of scrollable
};

struct visu_packet {
	char  opcode[4];
	u8_t which;
	u8_t count;
	union {
		struct {
			u32_t bars;
			u32_t spectrum_scale;
		} full;	
		struct {	
			u32_t width;
			u32_t height;
			s32_t col;
			s32_t row;	
			u32_t border;
			u32_t bars;
			u32_t spectrum_scale;
		};	
	};	
};

struct ANIC_header {
	char  opcode[4];
	u32_t length;
	u8_t mode;
};

#pragma pack(pop)

static struct {
	TaskHandle_t task;
	SemaphoreHandle_t mutex;
	int width, height;
	bool dirty;
	bool owned;
} displayer = { .dirty = true, .owned = true };	

#define LONG_WAKE 		(10*1000)
#define SB_HEIGHT		32

// lenght are number of frames, i.e. 2 channels of 16 bits
#define	FFT_LEN_BIT	7		
#define	FFT_LEN		(1 << FFT_LEN_BIT)
#define RMS_LEN_BIT	6
#define RMS_LEN		(1 << RMS_LEN_BIT)

#define DISPLAY_BW	20000

static struct scroller_s {
	// copy of grfs content
	u8_t  screen;	
	u32_t pause, speed;
	int wake;	
	u16_t mode;	
	s16_t by;
	// scroller management & sharing between grfg and scrolling task
	bool active, first, overflow;
	int scrolled;
	struct {
		u8_t *frame;
		u32_t width;
		u32_t max, size;
	} scroll;
	struct {
		u8_t *frame;
		u32_t width;
	} back;
	u8_t *frame;
	u32_t width;
} scroller;

#define MAX_BARS	32
static EXT_RAM_ATTR struct {
	int bar_gap, bar_width, bar_border;
	struct {
		int current, max;
		int limit;
	} bars[MAX_BARS];
	float spectrum_scale;
	int n, col, row, height, width, border;
	enum { VISU_BLANK, VISU_VUMETER, VISU_SPECTRUM, VISU_WAVEFORM } mode;
	int speed, wake;	
	float fft[FFT_LEN*2], samples[FFT_LEN*2], hanning[FFT_LEN];
} visu;

#define ANIM_NONE		  0x00
#define ANIM_TRANSITION   0x01 // A transition animation has finished
#define ANIM_SCROLL_ONCE  0x02 
#define ANIM_SCREEN_1     0x04 
#define ANIM_SCREEN_2     0x08 

static u8_t ANIC_resp = ANIM_NONE;
static uint16_t SETD_width;

#define SCROLL_STACK_SIZE	(3*1024)
#define LINELEN				40

static log_level loglevel = lINFO;

static bool (*slimp_handler_chain)(u8_t *data, int len);
static void (*slimp_loop_chain)(void);
static void (*notify_chain)(in_addr_t ip, u16_t hport, u16_t cport);
static bool (*display_bus_chain)(void *from, enum display_bus_cmd_e cmd);

#define max(a,b) (((a) > (b)) ? (a) : (b))

static void server(in_addr_t ip, u16_t hport, u16_t cport);
static void send_server(void);
static bool handler(u8_t *data, int len);
static bool display_bus_handler(void *from, enum display_bus_cmd_e cmd);
static void vfdc_handler( u8_t *_data, int bytes_read);
static void grfe_handler( u8_t *data, int len);
static void grfb_handler(u8_t *data, int len);
static void grfs_handler(u8_t *data, int len);
static void grfg_handler(u8_t *data, int len);
static void visu_handler(u8_t *data, int len);

static void displayer_task(void* arg);

/* scrolling undocumented information
	grfs	
		B: screen number
		B:1 = left, 2 = right,
		Q: scroll pause once done (ms)
		Q: scroll speed (ms)
		W: # of pixels to scroll each time
		W: 0 = continue scrolling after pause, 1 = scroll to scrollend and then stop, 2 = scroll to scrollend and then end animation (causing new update)
		W: width of total scroll area in pixels
			
	grfd
		W: screen number
		W: width of scrollable area	in pixels
	anic ( two versions, don't know what to chose)
		B: flag
			ANIM_TRANSITION (0x01) - transition animation has finished (previous use of ANIC)
			ANIM_SCREEN_1 (0x04)                           - end of first scroll on screen 1
			ANIM_SCREEN_2 (0x08)                           - end of first scroll on screen 2
			ANIM_SCROLL_ONCE (0x02) | ANIM_SCREEN_1 (0x04) - end of scroll once on screen 1
			ANIM_SCROLL_ONCE (0x02) | ANIM_SCREEN_2 (0x08) - end of scroll once on screen 2	
		- or -
			ANIM_TRANSITION   0x01 # A transition animation has finished
			ANIM_SCROLL_ONCE  0x02 # A scrollonce has finished
			ANIM_SCREEN_1     0x04 # For scrollonce only, screen 1 was scrolling
			ANIM_SCREEN_2     0x08 # For scrollonce only, screen 2 was scrolling
*/

/****************************************************************************************
 * 
 */
bool sb_display_init(void) {
	static DRAM_ATTR StaticTask_t xTaskBuffer __attribute__ ((aligned (4)));
	static EXT_RAM_ATTR StackType_t xStack[SCROLL_STACK_SIZE] __attribute__ ((aligned (4)));
	
	// no display, just make sure we won't have requests
	if (!display || GDS_GetWidth(display) <= 0 || GDS_GetHeight(display) <= 0) {
		LOG_INFO("no display for LMS");
		return false;
	}	
	
	// need to force height to 32 maximum
	displayer.width = GDS_GetWidth(display);
	displayer.height = min(GDS_GetHeight(display), SB_HEIGHT);
	SETD_width = displayer.width;

	// create visu configuration
	visu.bar_gap = 1;
	visu.speed = 100;
	dsps_fft2r_init_fc32(visu.fft, FFT_LEN);
	dsps_wind_hann_f32(visu.hanning, FFT_LEN);
		
	// create scroll management task
	displayer.mutex = xSemaphoreCreateMutex();
	displayer.task = xTaskCreateStatic( (TaskFunction_t) displayer_task, "displayer_thread", SCROLL_STACK_SIZE, NULL, ESP_TASK_PRIO_MIN + 1, xStack, &xTaskBuffer);
	
	// size scroller (width + current screen)
	scroller.scroll.max = (displayer.width * displayer.height / 8) * (15 + 1);
	scroller.scroll.frame = malloc(scroller.scroll.max);
	scroller.back.frame = malloc(displayer.width * displayer.height / 8);
	scroller.frame = malloc(displayer.width * displayer.height / 8);

	// chain handlers
	slimp_handler_chain = slimp_handler;
	slimp_handler = handler;
	
	slimp_loop_chain = slimp_loop;
	slimp_loop = send_server;
	
	notify_chain = server_notify;
	server_notify = server;
	
	display_bus_chain = display_bus;
	display_bus = display_bus_handler;
	
	return true;
}

/****************************************************************************************
 * Receive display bus commands
 */
static bool display_bus_handler(void *from, enum display_bus_cmd_e cmd) {
	// don't answer to own requests
	if (from == &displayer) return false ;
	
	LOG_INFO("Display bus command %d", cmd);
	
	xSemaphoreTake(displayer.mutex, portMAX_DELAY);
	
	switch (cmd) {
	case DISPLAY_BUS_TAKE:
		displayer.owned = false;
		break;
	case DISPLAY_BUS_GIVE:
		displayer.owned = true;
		break;
	}
	
	xSemaphoreGive(displayer.mutex);
	
	// chain to rest of "bus"
	if (display_bus_chain) return (*display_bus_chain)(from, cmd);
	else return true;
}


/****************************************************************************************
 * Send message to server (ANIC at that time)
 */
static void send_server(void) {
	/* 
	 This complication is needed as we cannot send direclty to LMS, because 
	 send_packet is not thread safe. So must subscribe to slimproto busy loop
	 end send from there
	*/ 
	if (ANIC_resp != ANIM_NONE) {
		struct ANIC_header pkt_header;

		memset(&pkt_header, 0, sizeof(pkt_header));
		memcpy(&pkt_header.opcode, "ANIC", 4);
		pkt_header.length = htonl(sizeof(pkt_header) - 8);
		pkt_header.mode = ANIC_resp;

		send_packet((u8_t *)&pkt_header, sizeof(pkt_header));
		
		ANIC_resp = ANIM_NONE;
	}	
	
	if (SETD_width) {
		struct SETD_header pkt_header;
		
		LOG_INFO("sending width %u", SETD_width);	
		
		memset(&pkt_header, 0, sizeof(pkt_header));
		memcpy(&pkt_header.opcode, "SETD", 4);

		pkt_header.id = 0xfe; // id 0xfe is width S:P:Squeezebox2
		pkt_header.length = htonl(sizeof(pkt_header) +  2 - 8);

		SETD_width = htons(SETD_width);
		send_packet((u8_t *)&pkt_header, sizeof(pkt_header));
		send_packet((uint8_t*) &SETD_width, 2);

		SETD_width = 0;
	}	
	
	if (slimp_loop_chain) (*slimp_loop_chain)();
}

/****************************************************************************************
 * 
 */
static void server(in_addr_t ip, u16_t hport, u16_t cport) {
	char msg[32];
	sprintf(msg, "%s:%hu", inet_ntoa(ip), hport);
	if (displayer.owned) GDS_TextPos(display, GDS_FONT_DEFAULT, GDS_TEXT_CENTERED, GDS_TEXT_CLEAR | GDS_TEXT_UPDATE, msg);
	SETD_width = displayer.width;
	displayer.dirty = true;
	if (notify_chain) (*notify_chain)(ip, hport, cport);
}

/****************************************************************************************
 * Process graphic display data
 */
static bool handler(u8_t *data, int len){
	bool res = true;
	
	if (!strncmp((char*) data, "vfdc", 4)) {
		vfdc_handler(data, len);
	} else if (!strncmp((char*) data, "grfe", 4)) {
		grfe_handler(data, len);
	} else if (!strncmp((char*) data, "grfb", 4)) {
		grfb_handler(data, len);
	} else if (!strncmp((char*) data, "grfs", 4)) {
		grfs_handler(data, len);		
	} else if (!strncmp((char*) data, "grfg", 4)) {
		grfg_handler(data, len);
	} else if (!strncmp((char*) data, "visu", 4)) {
		visu_handler(data, len);
	} else {
		res = false;
	}
	
	// chain protocol handlers (bitwise or is fine)
	if (*slimp_handler_chain) res |= (*slimp_handler_chain)(data, len);
	
	return res;
}

/****************************************************************************************
 * Change special LCD chars to something more printable on screen 
 */
static void makeprintable(unsigned char * line) {
	for (int n = 0; n < LINELEN; n++) {
		switch (line[n]) {
		case 11:		/* block */
			line[n] = '#';
			break;;
		case 16:		/* rightarrow */
			line[n] = '>';
			break;;
		case 22:		/* circle */
			line[n] = '@';
			break;;
		case 145:		/* note */
			line[n] = ' ';
			break;;
		case 152:		/* bell */
			line[n] = 'o';
			break;
		default:
			break;
		}
	}
}

/****************************************************************************************
 * Check if char is printable, or a valid symbol
 */
static bool charisok(unsigned char c) {
   switch (c) {
	  case 11:		/* block */
	  case 16:		/* rightarrow */
	  case 22:		/* circle */
	  case 145:		/* note */
	  case 152:		/* bell */
		 return true;
	 break;;
	  default:
		 return isprint(c);
   }
}

/****************************************************************************************
 * Show the display (text mode)
 */
static void show_display_buffer(char *ddram) {
	char line1[LINELEN+1];
	char *line2;

	memset(line1, 0, LINELEN+1);
	strncpy(line1, ddram, LINELEN);
	line2 = &(ddram[LINELEN]);
	line2[LINELEN] = '\0';

	/* Convert special LCD chars */
	makeprintable((unsigned char *)line1);
	makeprintable((unsigned char *)line2);

	LOG_DEBUG("\n\t%.40s\n\t%.40s", line1, line2);

	GDS_TextLine(display, 1, GDS_TEXT_LEFT, GDS_TEXT_CLEAR, line1);	
	GDS_TextLine(display, 2, GDS_TEXT_LEFT, GDS_TEXT_CLEAR | GDS_TEXT_UPDATE, line2);	
}

/****************************************************************************************
 * Process display data
 */
static void vfdc_handler( u8_t *_data, int bytes_read) {
	unsigned short *data = (unsigned short*) _data, *display_data;
	char ddram[(LINELEN + 1) * 2];
	int n, addr = 0; /* counter */

	bytes_read -= 4;
	if (bytes_read % 2) bytes_read--; /* even number of bytes */
	// if we use Noritake VFD codes, display data starts at 12
	display_data = &(data[5]); /* display data starts at byte 10 */

	memset(ddram, ' ', LINELEN * 2);

	for (n = 0; n < (bytes_read/2); n++) {
		unsigned short d; /* data element */
		unsigned char t, c;

		d = ntohs(display_data[n]);
		t = (d & 0x00ff00) >> 8; /* type of display data */
		c = (d & 0x0000ff); /* character/command */
		switch (t) {
			case 0x03: /* character */
				if (!charisok(c)) c = ' ';
				if (addr <= LINELEN * 2) {
					ddram[addr++] = c;
		}
				break;
			case 0x02: /* command */
				switch (c) {
					case 0x06: /* display clear */
						memset(ddram, ' ', LINELEN * 2);
						break;
					case 0x02: /* cursor home */
						addr = 0;
						break;
					case 0xc0: /* cursor home2 */
						addr = LINELEN;
						break;
				}
		}
	}

	show_display_buffer(ddram);
}

/****************************************************************************************
 * Process graphic display data
 */
static void grfe_handler( u8_t *data, int len) {
		
	xSemaphoreTake(displayer.mutex, portMAX_DELAY);
	
	scroller.active = false;
	
	// we are not in control or we are displaying visu on a small screen, do not do screen update
	if (visu.mode && !visu.col && visu.row < SB_HEIGHT) {
		xSemaphoreGive(displayer.mutex);
		return;
	}	
	
	if (displayer.owned) {
		// did we have something that might have write on the bottom of a SB_HEIGHT+ display
		if (displayer.dirty) {
			GDS_ClearExt(display, true);
			displayer.dirty = false;
		}	
	
		// draw new frame, it might be less than full screen (small visu)
		int width = ((len - sizeof(struct grfe_packet)) * 8) / displayer.height;
		GDS_DrawBitmapCBR(display, data + sizeof(struct grfe_packet), width, displayer.height, GDS_COLOR_WHITE);
		GDS_Update(display);
	}	
	
	xSemaphoreGive(displayer.mutex);
	
	LOG_DEBUG("grfe frame %u", len);
}	

/****************************************************************************************
 * Brightness
 */
static void grfb_handler(u8_t *data, int len) {
	struct grfb_packet *pkt = (struct grfb_packet*) data;
	
	pkt->brightness = htons(pkt->brightness);
	
	LOG_INFO("brightness %hu", pkt->brightness);
	if (pkt->brightness < 0) {
		GDS_DisplayOff(display); 
	} else {
		GDS_DisplayOn(display);
		GDS_SetContrast(display, pkt->brightness);
	}
}

/****************************************************************************************
 * Scroll set
 */
static void grfs_handler(u8_t *data, int len) {
	struct grfs_packet *pkt = (struct grfs_packet*) data;
	int size = len - sizeof(struct grfs_packet);
	int offset = htons(pkt->offset);
	
	LOG_DEBUG("gfrs s:%u d:%u p:%u sp:%u by:%hu m:%hu w:%hu o:%hu", 
				(int) pkt->screen,
				(int) pkt->direction,	// 1=left, 2=right
				htonl(pkt->pause),		// in ms	
				htonl(pkt->speed),		// in ms
				htons(pkt->by),			// # of pixel of scroll step
				htons(pkt->mode),		// 0=continuous, 1=once and stop, 2=once and end
				htons(pkt->width),		// last column of animation that contains a "full" screen
				htons(pkt->offset)		// offset if multiple packets are sent
	);
	
	// new grfs frame, build scroller info
	if (!offset) {	
		// use the display as a general lock
		xSemaphoreTake(displayer.mutex, portMAX_DELAY);

		// copy & set scroll parameters
		scroller.screen = pkt->screen;
		scroller.pause = htonl(pkt->pause);
		scroller.speed = htonl(pkt->speed);
		scroller.mode = htons(pkt->mode);
		scroller.scroll.width = htons(pkt->width);
		scroller.first = true;
		scroller.overflow = false;
		
		// background excludes space taken by visu (if any)
		scroller.back.width = displayer.width - ((visu.mode && visu.row < SB_HEIGHT) ? visu.width : 0);
		
		// set scroller steps & beginning
		if (pkt->direction == 1) {
			scroller.scrolled = 0;
			scroller.by = htons(pkt->by);
		} else {
			scroller.scrolled = scroller.scroll.width;
			scroller.by = -htons(pkt->by);
		}	

		xSemaphoreGive(displayer.mutex);
	}	

	// copy scroll frame data (no semaphore needed)
	if (scroller.scroll.size + size < scroller.scroll.max && !scroller.overflow) {
		memcpy(scroller.scroll.frame + offset, data + sizeof(struct grfs_packet), size);
		scroller.scroll.size = offset + size;
		LOG_INFO("scroller current size %u (w:%u)", scroller.scroll.size, scroller.scroll.width);
	} else {
		LOG_INFO("scroller too large %u/%u (w:%u)", scroller.scroll.size + size, scroller.scroll.max, scroller.scroll.width);
		scroller.scroll.width = scroller.scroll.size / (displayer.height / 8) - scroller.back.width;
		scroller.overflow = true;
	}	
}

/****************************************************************************************
 * Scroll background frame update & go
 */
static void grfg_handler(u8_t *data, int len) {
	struct grfg_packet *pkt = (struct grfg_packet*) data;
	
	LOG_DEBUG("gfrg s:%hu w:%hu (len:%u)", htons(pkt->screen), htons(pkt->width), len);
	
	xSemaphoreTake(displayer.mutex, portMAX_DELAY);
	
	// size of scrollable area (less than background)
	scroller.width = htons(pkt->width);
	memcpy(scroller.back.frame, data + sizeof(struct grfg_packet), len - sizeof(struct grfg_packet));
		
	// update display asynchronously (frames are organized by columns)
	memcpy(scroller.frame, scroller.back.frame, scroller.back.width * displayer.height / 8);
	for (int i = 0; i < scroller.width * displayer.height / 8; i++) scroller.frame[i] |= scroller.scroll.frame[scroller.scrolled * displayer.height / 8 + i];
	
	// can only write if we really own display
	if (displayer.owned) {
		GDS_DrawBitmapCBR(display, scroller.frame, scroller.back.width, displayer.height, GDS_COLOR_WHITE);
		GDS_Update(display);
	}	
		
	// now we can active scrolling, but only if we are not on a small screen
	if (!visu.mode || visu.col || visu.row >= SB_HEIGHT) scroller.active = true;
		
	// if we just got a content update, let the scroller manage the screen
	LOG_DEBUG("resuming scrolling task");
			
	xSemaphoreGive(displayer.mutex);
	
	// resume task once we have background, not in grfs
	vTaskResume(displayer.task);
}

/****************************************************************************************
 * Update visualization bars
 */
static void visu_update(void) {
	// no need to protect against no woning the display as we are playing	
	if (pthread_mutex_trylock(&visu_export.mutex)) return;
				
	// not enough samples
	if (visu_export.level < (visu.mode == VISU_VUMETER ? RMS_LEN : FFT_LEN) * 2 && visu_export.running) {
		pthread_mutex_unlock(&visu_export.mutex);
		return;
	}
	
	// reset bars for all cases first	
	for (int i = visu.n; --i >= 0;) visu.bars[i].current = 0;
	
	if (visu_export.running) {
					
		if (visu.mode == VISU_VUMETER) {
			s16_t *iptr = visu_export.buffer;
			
			// calculate sum(L²+R²), try to not overflow at the expense of some precision
			for (int i = RMS_LEN; --i >= 0;) {
				visu.bars[0].current += (*iptr * *iptr + (1 << (RMS_LEN_BIT - 2))) >> (RMS_LEN_BIT - 1);
				iptr++;
				visu.bars[1].current += (*iptr * *iptr + (1 << (RMS_LEN_BIT - 2))) >> (RMS_LEN_BIT - 1);
				iptr++;
			}	
		
			// convert to dB (1 bit remaining for getting X²/N, 60dB dynamic starting from 0dBFS = 3 bits back-off)
			for (int i = visu.n; --i >= 0;) {	 
				visu.bars[i].current = 32 * (0.01667f*10*log10f(0.0000001f + (visu.bars[i].current >> 1)) - 0.2543f);
				if (visu.bars[i].current > 31) visu.bars[i].current = 31;
				else if (visu.bars[i].current < 0) visu.bars[i].current = 0;
			}
		} else {
			// on xtensa/esp32 the floating point FFT takes 1/2 cycles of the fixed point
			for (int i = 0 ; i < FFT_LEN ; i++) {
				// don't normalize here, but we are due INT16_MAX and FFT_LEN / 2 / 2
				visu.samples[i * 2 + 0] = (float) (visu_export.buffer[2*i] + visu_export.buffer[2*i + 1]) * visu.hanning[i];
				visu.samples[i * 2 + 1] = 0;
			}

			// actual FFT that might be less cycle than all the crap below		
			dsps_fft2r_fc32_ae32(visu.samples, FFT_LEN);
			dsps_bit_rev_fc32_ansi(visu.samples, FFT_LEN);
			float rate = visu_export.rate;
			
			// now arrange the result with the number of bar and sampling rate (don't want DC)
			for (int i = 0, j = 1; i < visu.n && j < (FFT_LEN / 2); i++) {
				float power, count;

				// find the next point in FFT (this is real signal, so only half matters)
				for (count = 0, power = 0; j * visu_export.rate < visu.bars[i].limit * FFT_LEN && j < FFT_LEN / 2; j++, count += 1) {
					power += visu.samples[2*j] * visu.samples[2*j] + visu.samples[2*j+1] * visu.samples[2*j+1];
				}
				// due to sample rate, we have reached the end of the available spectrum
				if (j >= (FFT_LEN / 2)) {
					// normalize accumulated data
					if (count) power /= count * 2.;
				} else if (count) {
					// how much of what remains do we need to add
					float ratio = j - (visu.bars[i].limit * FFT_LEN) / rate;
					power += (visu.samples[2*j] * visu.samples[2*j] + visu.samples[2*j+1] * visu.samples[2*j+1]) * ratio;
					
					// normalize accumulated data
					power /= (count + ratio) * 2;
				} else {
					// no data for that band (sampling rate too high), just assume same as previous one
					power = (visu.samples[2*j] * visu.samples[2*j] + visu.samples[2*j+1] * visu.samples[2*j+1]) / 2.;
				}	
			
				// convert to dB and bars, same back-off
				if (power) visu.bars[i].current = 32 * (0.01667f*10*(log10f(power) - log10f(FFT_LEN/2*2)) - 0.2543f);
				if (visu.bars[i].current > 31) visu.bars[i].current = 31;
				else if (visu.bars[i].current < 0) visu.bars[i].current = 0;
			}	
		}
	} 
		
	// we took what we want, we can release the buffer
	visu_export.level = 0;
	pthread_mutex_unlock(&visu_export.mutex);

	// don't refresh screen if all max are 0 (we were are somewhat idle)
	int clear = 0;
	for (int i = visu.n; --i >= 0;) clear = max(clear, visu.bars[i].max);
	if (clear) GDS_ClearExt(display, false, false, visu.col, visu.row, visu.col + visu.width - 1, visu.row + visu.height - 1);

	// there is much more optimization to be done here, like not redrawing bars unless needed
	for (int i = visu.n; --i >= 0;) {
		int x1 = visu.col + visu.border + visu.bar_border + i*(visu.bar_width + visu.bar_gap);
		int y1 = visu.row + visu.height - 1;
			
		if (visu.bars[i].current > visu.bars[i].max) visu.bars[i].max = visu.bars[i].current;
		else if (visu.bars[i].max) visu.bars[i].max--;
		else if (!clear) continue;
		
		for (int j = 0; j <= visu.bars[i].current; j += 2) 
			GDS_DrawLine(display, x1, y1 - j, x1 + visu.bar_width - 1, y1 - j, GDS_COLOR_WHITE);
			
		if (visu.bars[i].max > 2) {
			GDS_DrawLine(display, x1, y1 - visu.bars[i].max, x1 + visu.bar_width - 1, y1 - visu.bars[i].max, GDS_COLOR_WHITE);			
			GDS_DrawLine(display, x1, y1 - visu.bars[i].max + 1, x1 + visu.bar_width - 1, y1 - visu.bars[i].max + 1, GDS_COLOR_WHITE);			
		}	
	}
}


/****************************************************************************************
 * Visu packet handler
 */
void spectrum_limits(int min, int n, int pos) {
	if (n / 2) {
		int step = ((DISPLAY_BW - min) * visu.spectrum_scale)  / (n/2);
		visu.bars[pos].limit = min + step;
		for (int i = 1; i < n/2; i++) visu.bars[pos+i].limit = visu.bars[pos+i-1].limit + step;
		spectrum_limits(visu.bars[pos + n/2 - 1].limit, n - n/2, pos + n/2);
	} else {
		visu.bars[pos].limit = DISPLAY_BW;
	}	
}

/****************************************************************************************
 * Visu packet handler
 */
static void visu_handler( u8_t *data, int len) {
	struct visu_packet *pkt = (struct visu_packet*) data;
	int bars = 0;

	LOG_DEBUG("visu %u with %u parameters", pkt->which, pkt->count);
		
	/* 
	 If width is specified, then respect all coordinates, otherwise we try to 
	 use the bottom part of the display and if it is a small display, we overwrite
	 text
	*/ 
	
	xSemaphoreTake(displayer.mutex, portMAX_DELAY);
	visu.mode = pkt->which;
	
	// little trick to clean the taller screens when switching visu 
	if (visu.row >= SB_HEIGHT) GDS_ClearExt(display, false, true, visu.col, visu.row, visu.col + visu.width - 1, visu.row - visu.height - 1);
	
	if (visu.mode) {
		if (pkt->count >= 4) {
			// small visu, then go were we are told to
			pkt->height = htonl(pkt->height);
			pkt->row = htonl(pkt->row);
			pkt->col = htonl(pkt->col);

			visu.width = htonl(pkt->width);
			visu.height = pkt->height ? pkt->height : SB_HEIGHT;
			visu.col = pkt->col < 0 ? displayer.width + pkt->col : pkt->col;
			visu.row = pkt->row < 0 ? GDS_GetHeight(display) + pkt->row : pkt->row;
			visu.border =  htonl(pkt->border);
			bars = htonl(pkt->bars);
			visu.spectrum_scale = htonl(pkt->spectrum_scale) / 100.;
			
			// might have a race condition with scroller message, so update width in case
			if (scroller.active) scroller.back.width = displayer.width - visu.width;
		} else {
			// full screen visu, try to use bottom screen if available
			visu.width = displayer.width;
			visu.height = GDS_GetHeight(display) > SB_HEIGHT ? GDS_GetHeight(display) - SB_HEIGHT : GDS_GetHeight(display);
			visu.col = visu.border = 0;
			visu.row = GDS_GetHeight(display) - visu.height;			
			bars = htonl(pkt->full.bars);
			visu.spectrum_scale = htonl(pkt->full.spectrum_scale) / 100.;
		}
		
		// try to adapt to what we have
		if (visu.mode == VISU_SPECTRUM) {
			visu.n = bars ? bars : MAX_BARS;
			if (visu.spectrum_scale <= 0 || visu.spectrum_scale > 0.5) visu.spectrum_scale = 0.5;
			spectrum_limits(0, visu.n, 0);
		} else {
			visu.n = 2;
		}	
		
		do {
			visu.bar_width = (visu.width - visu.border - visu.bar_gap * (visu.n - 1)) / visu.n;
			if (visu.bar_width > 0) break;
		} while (--visu.n);	
		visu.bar_border = (visu.width - visu.border - (visu.bar_width + visu.bar_gap) * visu.n + visu.bar_gap) / 2;
		
		// give up if not enough space
		if (visu.bar_width < 0)	{
			visu.mode = VISU_BLANK;
			LOG_WARN("Not enough room for displaying visu");
		} else {
			// de-activate scroller if we are taking main screen
			if (visu.row < SB_HEIGHT) scroller.active = false;
			vTaskResume(displayer.task);
		}	
		visu.wake = 0;
		
		// reset bars maximum
		for (int i = visu.n; --i >= 0;) visu.bars[i].max = 0;
				
		GDS_ClearExt(display, false, true, visu.col, visu.row, visu.col + visu.width - 1, visu.row - visu.height - 1);
		
		LOG_INFO("Visualizer with %u bars of width %d:%d:%d:%d (%w:%u,h:%u,c:%u,r:%u,s:%.02f)", visu.n, visu.bar_border, visu.bar_width, visu.bar_gap, visu.border, visu.width, visu.height, visu.col, visu.row, visu.spectrum_scale);
	} else {
		LOG_INFO("Stopping visualizer");
	}	
	
	xSemaphoreGive(displayer.mutex);
}	

/****************************************************************************************
 * Scroll task
 *  - with the addition of the visualizer, it's a bit a 2-headed beast not easy to 
 * maintain, so som better separation between the visu and scroll is probably needed
  */
static void displayer_task(void *args) {
	int sleep;
	
	while (1) {
		xSemaphoreTake(displayer.mutex, portMAX_DELAY);
		
		// suspend ourselves if nothing to do, grfg or visu will wake us up
		if (!scroller.active && !visu.mode)  {
			xSemaphoreGive(displayer.mutex);
			vTaskSuspend(NULL);
			xSemaphoreTake(displayer.mutex, portMAX_DELAY);
			scroller.wake = visu.wake = 0;
		}	
		
		// go for long sleep when either item is disabled
		if (!visu.mode) visu.wake = LONG_WAKE;
		if (!scroller.active) scroller.wake = LONG_WAKE;
								
		// scroll required amount of columns (within the window)
		if (scroller.active && scroller.wake <= 0) {
			// by default go for the long sleep, will change below if required
			scroller.wake = LONG_WAKE;
			
			// do we have more to scroll (scroll.width is the last column from which we have a full zone)
			if (scroller.by > 0 ? (scroller.scrolled <= scroller.scroll.width) : (scroller.scrolled >= 0)) {
				memcpy(scroller.frame, scroller.back.frame, scroller.back.width * displayer.height / 8);
				for (int i = 0; i < scroller.width * displayer.height / 8; i++) scroller.frame[i] |= scroller.scroll.frame[scroller.scrolled * displayer.height / 8 + i];
				scroller.scrolled += scroller.by;
				if (displayer.owned) GDS_DrawBitmapCBR(display, scroller.frame, scroller.width, displayer.height, GDS_COLOR_WHITE);	
				
				// short sleep & don't need background update
				scroller.wake = scroller.speed;
			} else if (scroller.first || !scroller.mode) {
				// at least one round done
				scroller.first = false;
				
				// see if we need to pause or if we are done 				
				if (scroller.mode) {
					// can't call directly send_packet from slimproto as it's not re-entrant
					ANIC_resp = ANIM_SCROLL_ONCE | ANIM_SCREEN_1;
					LOG_INFO("scroll-once terminated");
				} else {
					scroller.wake = scroller.pause;
					LOG_DEBUG("scroll cycle done, pausing for %u (ms)", scroller.pause);
				}
								
				// need to reset pointers for next scroll
				scroller.scrolled = scroller.by < 0 ? scroller.scroll.width : 0;
			} 
		}

		// update visu if active
		if (visu.mode && visu.wake <= 0) {
			visu_update();
			visu.wake = 100;
		}
		
		// need to make sure we own display
		if (displayer.owned) GDS_Update(display);
		
		// release semaphore and sleep what's needed
		xSemaphoreGive(displayer.mutex);
		
		sleep = min(visu.wake, scroller.wake);
		vTaskDelay(sleep / portTICK_PERIOD_MS);
		scroller.wake -= sleep;
		visu.wake -= sleep;
	}	
}	
			







