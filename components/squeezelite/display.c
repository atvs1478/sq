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

struct grfb_packet {
	char  opcode[4];
	u16_t  brightness;
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

#define LINELEN		40

static log_level loglevel = lINFO;

static bool (*slimp_handler_chain)(u8_t *data, int len);
static void (*notify_chain)(in_addr_t ip, u16_t hport, u16_t cport);

static void server(in_addr_t ip, u16_t hport, u16_t cport);
static bool handler(u8_t *data, int len);
static void vfdc_handler( u8_t *_data, int bytes_read);
static void grfe_handler( u8_t *data, int len);
static void grfb_handler(u8_t *data, int len);
static void grfs_handler(u8_t *data, int len);
static void grfg_handler(u8_t *data, int len);

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
	display->v_draw(data + 8);
}	

/****************************************************************************************
 * Brightness
 */
static void grfb_handler(u8_t *data, int len) {
	s16_t brightness = htons(*(uint16_t*) (data + 4));
	
	LOG_INFO("brightness %hx", brightness);
	if (brightness < 0) {
		display->on(false); 
	} else {
		display->on(true);
		display->brightness(brightness);
	}
}

/****************************************************************************************
 * Scroll set
 */
static void grfs_handler(u8_t *data, int len) {
}	
	
/****************************************************************************************
 * Scroll go
 */
static void grfg_handler(u8_t *data, int len) {
}	







