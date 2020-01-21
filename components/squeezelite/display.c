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
#include "squeezelite.h"
#include "display.h"

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

#pragma pack(pop)

static struct scroller_s {
	TaskHandle_t task;
	bool active;
	u8_t  screen, direction;	
	u32_t pause, speed;		
	u16_t by, mode, width, scroll_width;		
	u16_t max, size;
	u8_t *scroll_frame, *back_frame;
} scroller;

#define SCROLL_STACK_SIZE	2048
#define LINELEN				40
#define HEIGHT				32

static log_level loglevel = lINFO;
static SemaphoreHandle_t display_sem;
static bool (*slimp_handler_chain)(u8_t *data, int len);
static void (*notify_chain)(in_addr_t ip, u16_t hport, u16_t cport);

#define max(a,b) (((a) > (b)) ? (a) : (b))

static void server(in_addr_t ip, u16_t hport, u16_t cport);
static bool handler(u8_t *data, int len);
static void vfdc_handler( u8_t *_data, int bytes_read);
static void grfe_handler( u8_t *data, int len);
static void grfb_handler(u8_t *data, int len);
static void grfs_handler(u8_t *data, int len);
static void grfg_handler(u8_t *data, int len);
static void scroll_task(void* arg);

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
		
ANIC flags
ANIM_TRANSITION   0x01 # A transition animation has finished
ANIM_SCROLL_ONCE  0x02 # A scrollonce has finished
ANIM_SCREEN_1     0x04 # For scrollonce only, screen 1 was scrolling
ANIM_SCREEN_2     0x08 # For scrollonce only, screen 2 was scrolling
*/

/****************************************************************************************
 * 
 */
void sb_display_init(void) {
	static DRAM_ATTR StaticTask_t xTaskBuffer __attribute__ ((aligned (4)));
	static EXT_RAM_ATTR StackType_t xStack[SCROLL_STACK_SIZE] __attribute__ ((aligned (4)));
	
	// create scroll management task
	display_sem = xSemaphoreCreateMutex();
	scroller.task = xTaskCreateStatic( (TaskFunction_t) scroll_task, "scroll_thread", SCROLL_STACK_SIZE, NULL, ESP_TASK_PRIO_MIN + 1, xStack, &xTaskBuffer);
	
	// size scroller
	scroller.max = (display->width * display->height / 8) * 10;
	scroller.scroll_frame = malloc(scroller.max);
	scroller.back_frame = malloc(display->width * display->height / 8);

	// chain handlers
	slimp_handler_chain = slimp_handler;
	slimp_handler = handler;
	
	notify_chain = server_notify;
	server_notify = server;
}

/****************************************************************************************
 * 
 */
static void server(in_addr_t ip, u16_t hport, u16_t cport) {
	char msg[32];
	sprintf(msg, "%s:%hu", inet_ntoa(ip), hport);
	display->text(DISPLAY_CENTER, DISPLAY_CLEAR | DISPLAY_UPDATE, msg);
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

	LOG_INFO("\n\t%.40s\n\t%.40s", line1, line2);

	display->text(DISPLAY_TOP_LEFT, DISPLAY_CLEAR, line1);	
	display->text(DISPLAY_BOTTOM_LEFT, DISPLAY_UPDATE, line2);	
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
	scroller.active = false;
	xSemaphoreTake(display_sem, portMAX_DELAY);
	display->draw_cbr(data + sizeof(struct grfe_packet), HEIGHT);
	xSemaphoreGive(display_sem);
}	

/****************************************************************************************
 * Brightness
 */
static void grfb_handler(u8_t *data, int len) {
	struct grfb_packet *pkt = (struct grfb_packet*) data;
	
	pkt->brightness = htons(pkt->brightness);
	
	LOG_INFO("brightness %hu", pkt->brightness);
	if (pkt->brightness < 0) {
		display->on(false); 
	} else {
		display->on(true);
		display->brightness(pkt->brightness);
	}
}

/****************************************************************************************
 * Scroll set
 */
static void grfs_handler(u8_t *data, int len) {
	struct grfs_packet *pkt = (struct grfs_packet*) data;
	int size = len - sizeof(struct grfs_packet);
	int offset = htons(pkt->offset);
	
	LOG_INFO("gfrs s:%u d:%u p:%u sp:%u by:%hu m:%hu w:%hu o:%hu", 
				(int) pkt->screen,
				(int) pkt->direction,	// 1=left, 2=right
				htonl(pkt->pause),		// in ms	
				htonl(pkt->speed),		// in ms
				htons(pkt->by),			// # of pixel of scroll step
				htons(pkt->mode),			// 0=continuous, 1=once and stop, 2=once and end
				htons(pkt->width),		// total width of animation
				htons(pkt->offset)		// offset if multiple packets are sent
	);

	// copy scroll parameters
	scroller.screen = pkt->screen;
	scroller.direction = pkt->direction;
	scroller.pause = htonl(pkt->pause);
	scroller.speed = htonl(pkt->speed);
	scroller.by = htons(pkt->by);
	scroller.mode = htons(pkt->mode);
	scroller.width = htons(pkt->width);

	// copy scroll frame data
	if (scroller.size + size < scroller.max) {
		memcpy(scroller.scroll_frame + offset, data + sizeof(struct grfs_packet), size);
		scroller.size = offset + size;
		LOG_INFO("scroller size now %u", scroller.size);
	} else {
		LOG_INFO("scroller too larger %u/%u", scroller.size + size, scroller.max);
	}	
}

/****************************************************************************************
 * Scroll background frame update & go
 */
static void grfg_handler(u8_t *data, int len) {
	struct grfg_packet *pkt = (struct grfg_packet*) data;
	
	memcpy(scroller.back_frame, data + sizeof(struct grfg_packet), len - sizeof(struct grfg_packet));
	scroller.scroll_width = htons(pkt->width);
	scroller.active = true;
	vTaskResume(scroller.task);
	
	LOG_INFO("gfrg s:%hu w:%hu (len:%u)", htons(pkt->screen), htons(pkt->width), len);
}

/****************************************************************************************
 * Scroll task
 */
static void scroll_task(void *args) {
	u8_t *scroll_ptr, *frame;
	bool active = false;
	int len = display->width * display->height / 8;
	int scroll_len;

	while (1) {
		if (!active) vTaskSuspend(NULL);
		
		// restart at the beginning - I don't know what right scrolling means ...
		scroll_ptr = scroller.scroll_frame;
		scroll_len = len - (display->width - scroller.scroll_width) * display->height / 8;
		frame = malloc(display->width * display->height / 8);
						
		// scroll required amount of columns (within the window)
		while (scroll_ptr <= scroller.scroll_frame + scroller.size - scroller.by * display->height / 8 - len) {
			scroll_ptr += scroller.by * display->height / 8;
						
			// build a combined frame
			memcpy(frame, scroller.back_frame, len);
			for (int i = 0; i < scroll_len; i++) frame[i] |= scroll_ptr[i];
			
			xSemaphoreTake(display_sem, portMAX_DELAY);
			active = scroller.active;
			
			if (!active) {
				LOG_INFO("scrolling interrupted");
				xSemaphoreGive(display_sem);
				break;
			}
			
			display->draw_cbr(frame, HEIGHT);		
			xSemaphoreGive(display_sem);
			usleep(scroller.speed * 1000);
		}
		
		if (active) {
			// build default screen
			memcpy(frame, scroller.back_frame, len);
			for (int i = 0; i < scroll_len; i++) frame[i] |= scroller.scroll_frame[i];
			xSemaphoreTake(display_sem, portMAX_DELAY);
			display->draw_cbr(frame, HEIGHT);		
			xSemaphoreGive(display_sem);
		
			// pause for required time and continue (or not)
			usleep(scroller.pause * 1000);
			active = (scroller.mode == 0);
		}	
		
		free(frame);
	}	
}	







