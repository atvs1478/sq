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

#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "esp_log.h"
#include "display.h"

#include "ssd1306.h"
#include "ssd1306_draw.h"
#include "ssd1306_font.h"
#include "ssd1306_default_if.h"

#define I2C_PORT 	1
#define I2C_ADDRESS	0x3C
#define LINELEN		40
static const char *TAG = "display";

static void 	vfdc_handler( u8_t *_data, int bytes_read);
static void 	grfe_handler( u8_t *data, int len);
static bool 	display_init(char *config, char *welcome);
static void 	print_message(char *msg);

struct display_handle_s SSD1306_handle = {
	display_init,
	print_message, 
	vfdc_handler,
	grfe_handler,
	NULL, NULL,
};

static struct SSD1306_Device I2CDisplay;
static SSD1306_AddressMode AddressMode = AddressMode_Invalid;

static const unsigned char BitReverseTable256[] = 
{
  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0, 
  0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 
  0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4, 
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC, 
  0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 
  0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6, 
  0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
  0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9, 
  0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
  0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3, 
  0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
  0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7, 
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

/****************************************************************************************
 * 
 */
static bool display_init(char *config, char *welcome) {
	bool res = false;

	if (strstr(config, "I2C")) {
		int scl = -1, sda = -1;
		int width = -1, height = -1;
		char *p;

		// no time for smart parsing - this is for tinkerers
		if ((p = strcasestr(config, "scl")) != NULL) scl = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(config, "sda")) != NULL) sda = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(config, "width")) != NULL) width = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(config, "height")) != NULL) height = atoi(strchr(p, '=') + 1);

		if (sda != -1 && scl != -1 && width != -1 && height != -1) {
			SSD1306_I2CMasterInitDefault( I2C_PORT, sda, scl );
			SSD1306_I2CMasterAttachDisplayDefault( &I2CDisplay, width, height, I2C_ADDRESS, -1);
			SSD1306_SetFont( &I2CDisplay, &Font_droid_sans_fallback_15x17 );
			print_message(welcome);
			ESP_LOGI(TAG, "Initialized I2C display %dx%d (sda:%d, scl:%d)", width, height, sda, scl);
			res = true;
		} else {
			ESP_LOGI(TAG, "Cannot initialized I2C display %s [%dx%d sda:%d, scl:%d]", config, width, height, sda, scl);
		}
	} else {
		// other types
	}

	return res;
}

/****************************************************************************************
 * 
 */
static void print_message(char *msg) {
	if (!msg) return;
	SSD1306_Clear( &I2CDisplay, SSD_COLOR_BLACK );
	SSD1306_SetDisplayAddressMode( &I2CDisplay, AddressMode_Horizontal );
	SSD1306_FontDrawAnchoredString( &I2CDisplay, TextAnchor_Center, msg, SSD_COLOR_WHITE );
	SSD1306_Update( &I2CDisplay );
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

	ESP_LOGI(TAG, "\n\t%.40s\n\t%.40s", line1, line2);

	SSD1306_Clear( &I2CDisplay, SSD_COLOR_BLACK );
	SSD1306_FontDrawAnchoredString( &I2CDisplay, TextAnchor_NorthWest, line1, SSD_COLOR_WHITE );
	SSD1306_FontDrawAnchoredString( &I2CDisplay, TextAnchor_SouthWest, line2, SSD_COLOR_WHITE );

	// check addressing mode by rows
	if (AddressMode != AddressMode_Horizontal) {
		AddressMode = AddressMode_Horizontal;
		SSD1306_SetDisplayAddressMode( &I2CDisplay, AddressMode );
	}

	SSD1306_Update( &I2CDisplay );
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
void grfe_handler( u8_t *data, int len) {
	data += 8;
	len -= 8;

	// to be verified, but this is as fast as using a pointer on data
	for (int i = len - 1; i >= 0; i--) data[i] = BitReverseTable256[data[i]];

	// check addressing mode by columns
	if (AddressMode != AddressMode_Vertical) {
		AddressMode = AddressMode_Vertical;
		SSD1306_SetDisplayAddressMode( &I2CDisplay, AddressMode );
	}
	
	SSD1306_WriteRawData( &I2CDisplay, data,  len);
}


