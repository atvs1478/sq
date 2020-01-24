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

struct ANIC_header {
	char  opcode[4];
	u32_t length;
	u8_t mode;
};

#pragma pack(pop)

static struct scroller_s {
	// copy of grfs content
	u8_t  screen, direction;	
	u32_t pause, speed;		
	u16_t by, mode, full_width, window_width;		
	u16_t max, size;
	// scroller management & sharing between grfg and scrolling task
	TaskHandle_t task;
	u8_t *scroll_frame, *back_frame;
	bool active, updated;
	u8_t *scroll_ptr;
	int scroll_len, scroll_step;
} scroller;

#define ANIM_NONE		  0x00
#define ANIM_TRANSITION   0x01 // A transition animation has finished
#define ANIM_SCROLL_ONCE  0x02 
#define ANIM_SCREEN_1     0x04 
#define ANIM_SCREEN_2     0x08 

static u8_t ANIC_resp = ANIM_NONE;

#define SCROLL_STACK_SIZE	2048
#define LINELEN				40

static log_level loglevel = lINFO;
static SemaphoreHandle_t display_sem;
static bool (*slimp_handler_chain)(u8_t *data, int len);
static void (*slimp_loop_chain)(void);
static void (*notify_chain)(in_addr_t ip, u16_t hport, u16_t cport);
static int display_width, display_height;

#define max(a,b) (((a) > (b)) ? (a) : (b))

static void server(in_addr_t ip, u16_t hport, u16_t cport);
static void send_server(void);
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
void sb_display_init(void) {
	static DRAM_ATTR StaticTask_t xTaskBuffer __attribute__ ((aligned (4)));
	static EXT_RAM_ATTR StackType_t xStack[SCROLL_STACK_SIZE] __attribute__ ((aligned (4)));
	
	// need to force height to 32 maximum
	display_width = display->width;
	display_height = min(display->height, 32);
	
	// create scroll management task
	display_sem = xSemaphoreCreateMutex();
	scroller.task = xTaskCreateStatic( (TaskFunction_t) scroll_task, "scroll_thread", SCROLL_STACK_SIZE, NULL, ESP_TASK_PRIO_MIN + 1, xStack, &xTaskBuffer);
	
	// size scroller
	scroller.max = (display_width * display_height / 8) * 10;
	scroller.scroll_frame = malloc(scroller.max);
	scroller.back_frame = malloc(display_width * display_height / 8);

	// chain handlers
	slimp_handler_chain = slimp_handler;
	slimp_handler = handler;
	
	slimp_loop_chain = slimp_loop;
	slimp_loop = send_server;
	
	notify_chain = server_notify;
	server_notify = server;
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
	
	if (slimp_loop_chain) (*slimp_loop_chain)();
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
	xSemaphoreTake(display_sem, portMAX_DELAY);
	
	scroller.active = false;
	display->draw_cbr(data + sizeof(struct grfe_packet), display_height);
	
	xSemaphoreGive(display_sem);
	
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
	
	// new grfs frame, build scroller info
	if (!offset) {	
		// use the display as a general lock
		xSemaphoreTake(display_sem, portMAX_DELAY);

		// copy & set scroll parameters
		scroller.screen = pkt->screen;
		scroller.direction = pkt->direction;
		scroller.pause = htonl(pkt->pause);
		scroller.speed = htonl(pkt->speed);
		scroller.by = htons(pkt->by);
		scroller.mode = htons(pkt->mode);
		scroller.full_width = htons(pkt->width);
		scroller.updated = scroller.active = true;

		xSemaphoreGive(display_sem);
	}	

	// copy scroll frame data (no semaphore needed)
	if (scroller.size + size < scroller.max) {
		memcpy(scroller.scroll_frame + offset, data + sizeof(struct grfs_packet), size);
		scroller.size = offset + size;
		LOG_INFO("scroller current size %u", scroller.size);
	} else {
		LOG_INFO("scroller too larger %u/%u", scroller.size + size, scroller.max);
	}	
}

/****************************************************************************************
 * Scroll background frame update & go
 */
static void grfg_handler(u8_t *data, int len) {
	struct grfg_packet *pkt = (struct grfg_packet*) data;
	
	LOG_INFO("gfrg s:%hu w:%hu (len:%u)", htons(pkt->screen), htons(pkt->width), len);
	
	memcpy(scroller.back_frame, data + sizeof(struct grfg_packet), len - sizeof(struct grfg_packet));
	scroller.window_width = htons(pkt->width);
	
	xSemaphoreTake(display_sem, portMAX_DELAY);

	// can't be in grfs as we need full size & scroll_width
	if (scroller.updated) {
		scroller.scroll_len = display_width * display_height / 8 - (display_width - scroller.window_width) * display_height / 8;
		if (scroller.direction == 1) {
			scroller.scroll_ptr = scroller.scroll_frame; 
			scroller.scroll_step = scroller.by * display_height / 8;
		} else	{
			scroller.scroll_ptr = scroller.scroll_frame + scroller.size - scroller.scroll_len;
			scroller.scroll_step = -scroller.by * display_height / 8;
		}
		
		scroller.updated = false;	
	}	
	
	if (!scroller.active) {
		// this is a background update and scroller has been finished, so need to update here
		u8_t *frame = malloc(display_width * display_height / 8);
		memcpy(frame, scroller.back_frame, display_width * display_height / 8);
		for (int i = 0; i < scroller.scroll_len; i++) frame[i] |= scroller.scroll_ptr[i];
		display->draw_cbr(frame, display_height);		
		free(frame);
		LOG_DEBUG("direct drawing");
	}	
	else {
		// if we just got a content update, let the scroller manage the screen
		LOG_INFO("resuming scrolling task");
		vTaskResume(scroller.task);
	}
	
	xSemaphoreGive(display_sem);
}

/****************************************************************************************
 * Scroll task
 */
static void scroll_task(void *args) {
	u8_t *frame = NULL;
	int len = display_width * display_height / 8;
	
	while (1) {
		xSemaphoreTake(display_sem, portMAX_DELAY);
		
		// suspend ourselves if nothing to do, grfg will wake us up
		if (!scroller.active)  {
			xSemaphoreGive(display_sem);
			vTaskSuspend(NULL);
			xSemaphoreTake(display_sem, portMAX_DELAY);
		}	
		
		// lock screen & active status
		frame = malloc(display_width * display_height / 8);
				
		// scroll required amount of columns (within the window)
		while (scroller.direction == 1 ? (scroller.scroll_ptr <= scroller.scroll_frame + scroller.size - scroller.scroll_step - len) :
									     (scroller.scroll_ptr + scroller.scroll_step >= scroller.scroll_frame) ) {
			
			// don't do anything if we have aborted
			if (!scroller.active) break;
					
			// scroll required amount of columns (within the window)
			memcpy(frame, scroller.back_frame, display_width * display_height / 8);
			for (int i = 0; i < scroller.scroll_len; i++) frame[i] |= scroller.scroll_ptr[i];
			scroller.scroll_ptr += scroller.scroll_step;
			display->draw_cbr(frame, display_height);		
			
			xSemaphoreGive(display_sem);
			usleep(scroller.speed * 1000);
			xSemaphoreTake(display_sem, portMAX_DELAY);
		}
		
		// done with scrolling cycle reset scroller ptr
		scroller.scroll_ptr = scroller.scroll_frame + (scroller.direction == 2 ? scroller.size - scroller.scroll_len : 0);
				
		// scrolling done, update screen and see if we need to continue
		if (scroller.active) {
			memcpy(frame, scroller.back_frame, len);
			for (int i = 0; i < scroller.scroll_len; i++) frame[i] |= scroller.scroll_ptr[i];
			display->draw_cbr(frame, display_height);
			free(frame);

			// see if we need to pause or if we are done 				
			if (scroller.mode) {
				scroller.active = false;
				xSemaphoreGive(display_sem);
				// can't call directly send_packet from slimproto as it's not re-entrant
				ANIC_resp = ANIM_SCROLL_ONCE | ANIM_SCREEN_1;
				LOG_INFO("scroll-once terminated");
			} else {
				xSemaphoreGive(display_sem);
				usleep(scroller.pause * 1000);
				LOG_DEBUG("scroll cycle done, pausing for %u (ms)", scroller.pause);
			}
		} else {
			free(frame);
			xSemaphoreGive(display_sem);
			LOG_INFO("scroll aborted");
		}	
	}	
}	
			







