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
#include "globdefs.h"

#include "ssd1306.h"
#include "ssd1306_draw.h"
#include "ssd1306_font.h"
#include "ssd1306_default_if.h"

#define I2C_ADDRESS	0x3C

static const char *TAG = "display";

#define max(a,b) (((a) > (b)) ? (a) : (b))

// handlers
static bool init(char *config, char *welcome);
static void clear(void);
static bool set_font(int num, enum display_font_e font, int space);
static void text(enum display_font_e font, enum display_pos_e pos, int attribute, char *text, ...);
static bool line(int num, int x, int attribute, char *text);
static int stretch(int num, char *string, int max);
static void draw_cbr(u8_t *data, int height);
static void draw(int x1, int y1, int x2, int y2, bool by_column, u8_t *data);
static void brightness(u8_t level);
static void on(bool state);
static void update(void);

// display structure for others to use
struct display_s SSD1306_display = { 0, 0, 
									init, clear, set_font, on, brightness, 
									text, line, stretch, update, draw, draw_cbr, NULL };

// SSD1306 specific function
static struct SSD1306_Device Display;
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

#define MAX_LINES	8

static struct {
	int y, space;
	const struct SSD1306_FontDef *font;
} lines[MAX_LINES];

/****************************************************************************************
 * 
 */
static bool init(char *config, char *welcome) {
	bool res = false;

	if (strstr(config, "I2C")) {
		int width = -1, height = -1, address = I2C_ADDRESS;
		char *p;
		ESP_LOGI(TAG, "Initializing I2C display with config: %s",config);
		// no time for smart parsing - this is for tinkerers
		if ((p = strcasestr(config, "width")) != NULL) width = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(config, "height")) != NULL) height = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(config, "address")) != NULL) address = atoi(strchr(p, '=') + 1);

		if (width != -1 && height != -1) {
			SSD1306_I2CMasterInitDefault( i2c_system_port, -1, -1 ) ;
			SSD1306_I2CMasterAttachDisplayDefault( &Display, width, height, address, -1 );
			SSD1306_SetHFlip( &Display, strcasestr(config, "HFlip") ? true : false);
			SSD1306_SetVFlip( &Display, strcasestr(config, "VFlip") ? true : false);
			SSD1306_SetFont( &Display, &Font_droid_sans_fallback_15x17 );
			SSD1306_display.width = width;
			SSD1306_display.height = height;
			text(DISPLAY_FONT_MEDIUM, DISPLAY_CENTERED, DISPLAY_CLEAR | DISPLAY_UPDATE, welcome);
			ESP_LOGI(TAG, "Initialized I2C display %dx%d", width, height);
			res = true;
		} else {
			ESP_LOGI(TAG, "Cannot initialized I2C display %s [%dx%d]", config, width, height);
		}
	} else {
		ESP_LOGE(TAG, "Non-I2C display not supported. Display config %s", config);
	}

	return res;
}

/****************************************************************************************
 * 
 */
static void clear(void) {
	SSD1306_Clear( &Display, SSD_COLOR_BLACK );
	SSD1306_Update( &Display );
}	

/****************************************************************************************
 *  Set fonts for each line in text mode
 */
static bool set_font(int num, enum display_font_e font, int space) {
	if (--num >= MAX_LINES) return false;
	
	switch(font) {
	case DISPLAY_FONT_LINE1:	
		lines[num].font = &Font_line1_11x13;
		break;
	case DISPLAY_FONT_TINY:
		lines[num].font = &Font_droid_sans_fallback_11x13;
		break;		
	case DISPLAY_FONT_SMALL:	
		lines[num].font = &Font_droid_sans_mono_7x13;	
		break;
	case DISPLAY_FONT_MEDIUM:			
		lines[num].font = &Font_liberation_mono_9x15;	
		break;
	case DISPLAY_FONT_LARGE:	
		lines[num].font = &Font_VDS_Compensated15x22;
		break;		
	case DISPLAY_FONT_HUGE:	
		lines[num].font = &Font_droid_sans_fallback_24x28;
		break;				
	case DISPLAY_FONT_SEGMENT:			
		if (Display.Height == 32) lines[num].font = &Font_Tarable7Seg_16x32;
		else lines[num].font = &Font_Tarable7Seg_32x64;
		break;		
	case DISPLAY_FONT_DEFAULT:
	default:	
		lines[num].font = &Font_droid_sans_fallback_15x17;
		break;
	}
	
	// re-calculate lines absolute position
	lines[num].space = space;
	lines[0].y = lines[0].space;
	for (int i = 1; i <= num; i++) lines[i].y = lines[i-1].y + lines[i-1].font->Height + lines[i].space;
		
	ESP_LOGI(TAG, "adding line %u at %u (height:%u)", num + 1, lines[num].y, lines[num].font->Height);
	
	if (lines[num].y + lines[num].font->Height > Display.Height) {
		ESP_LOGW(TAG, "line does not fit display");
		return false;
	}
	
	return true;
}

/****************************************************************************************
 * 
 */
static bool line(int num, int x, int attribute, char *text) {
	int width;
	
	// counting 1..n
	num--;
	
	// always horizontal mode for text display
	if (AddressMode != AddressMode_Horizontal) {
		AddressMode = AddressMode_Horizontal;
		SSD1306_SetDisplayAddressMode( &Display, AddressMode );
	}	
	
	SSD1306_SetFont( &Display, lines[num].font );	
	if (attribute & DISPLAY_MONOSPACE) SSD1306_FontForceMonospace( &Display, true );
	
	width = SSD1306_FontMeasureString( &Display, text );
	
	// adjusting position, erase only EoL for rigth-justified
	if (x == DISPLAY_RIGHT) x = Display.Width - width - 1;
	else if (x == DISPLAY_CENTER) x = (Display.Width - width) / 2;
	
	// erase if requested
	if (attribute & DISPLAY_CLEAR) {
		for (int c = (attribute & DISPLAY_ONLY_EOL) ? x : 0; c < Display.Width; c++) 
			for (int y = lines[num].y; y < lines[num].y + lines[num].font->Height; y++)
				SSD1306_DrawPixelFast( &Display, c, y, SSD_COLOR_BLACK );
	}	
		
	SSD1306_FontDrawString( &Display, x, lines[num].y, text, SSD_COLOR_WHITE );
	
	ESP_LOGD(TAG, "displaying %s line %u (x:%d, attr:%u)", text, num+1, x, attribute);
	
	// update whole display if requested
	if (attribute & DISPLAY_UPDATE) SSD1306_Update( &Display );
	
	return width + x < Display.Width;
}

/****************************************************************************************
 * Try to align string for better scrolling visual. there is probably much better to do
 */
static int stretch(int num, char *string, int max) {
	char space[] = "     ";
	int len = strlen(string), extra = 0, boundary;
	
	num--;
	
	// we might already fit
	SSD1306_SetFont( &Display, lines[num].font );	
	if (SSD1306_FontMeasureString( &Display, string ) <= Display.Width) return 0;
		
	// add some space for better visual 
	strncat(string, space, max-len);
	string[max] = '\0';
	len = strlen(string);
	
	// mark the end of the extended string
	boundary = SSD1306_FontMeasureString( &Display, string );
			
	// add a full display width	
	while (len < max && SSD1306_FontMeasureString( &Display, string ) - boundary < Display.Width) {
		string[len++] = string[extra++];
		string[len] = '\0';
	}
		
	return boundary;
}

/****************************************************************************************
 * 
 */
static void text(enum display_font_e font, enum display_pos_e pos, int attribute, char *text, ...) {
	va_list args;

	va_start(args, text);
	TextAnchor Anchor = TextAnchor_Center;	
	
	if (attribute & DISPLAY_CLEAR) SSD1306_Clear( &Display, SSD_COLOR_BLACK );
	
	if (!text) return;
	
	switch(font) {
	case DISPLAY_FONT_LINE1:	
		SSD1306_SetFont( &Display, &Font_line1_11x13 );
		break;
	case DISPLAY_FONT_TINY:	
		SSD1306_SetFont( &Display, &Font_droid_sans_fallback_11x13 );
		break;		
	case DISPLAY_FONT_SMALL:	
		SSD1306_SetFont( &Display, &Font_droid_sans_mono_7x13 );	
		break;
	case DISPLAY_FONT_LARGE:	
		SSD1306_SetFont( &Display, &Font_VDS_Compensated15x22 );
		break;		
	case DISPLAY_FONT_HUGE:	
		SSD1306_SetFont( &Display, &Font_droid_sans_fallback_24x28 );
		break;				
	case DISPLAY_FONT_MEDIUM:			
		SSD1306_SetFont( &Display, &Font_liberation_mono_9x15 );	
		break;
	case DISPLAY_FONT_SEGMENT:			
		if (Display.Height == 32) SSD1306_SetFont( &Display, &Font_Tarable7Seg_16x32 );
		else SSD1306_SetFont( &Display, &Font_Tarable7Seg_32x64 );
		break;		
	case DISPLAY_FONT_DEFAULT:
	default:	
		SSD1306_SetFont( &Display, &Font_droid_sans_fallback_15x17 );
		break;
	}

	switch(pos) {
	case DISPLAY_TOP_LEFT: 
	default:
		Anchor = TextAnchor_NorthWest; 
		break;
	case DISPLAY_MIDDLE_LEFT:
		Anchor = TextAnchor_West;
		break;
	case DISPLAY_BOTTOM_LEFT:
		Anchor = TextAnchor_SouthWest;
		break;
	case DISPLAY_CENTERED:
		Anchor = TextAnchor_Center;
		break;
	}	
	
	ESP_LOGD(TAG, "SSDD1306 displaying %s at %u with attribute %u", text, Anchor, attribute);
	
	if (AddressMode != AddressMode_Horizontal) {
		AddressMode = AddressMode_Horizontal;
		SSD1306_SetDisplayAddressMode( &Display, AddressMode );
	}	
	
	SSD1306_FontDrawAnchoredString( &Display, Anchor, text, SSD_COLOR_WHITE );
	if (attribute & DISPLAY_UPDATE) SSD1306_Update( &Display );
	
	va_end(args);
}

/****************************************************************************************
 * Process graphic display data from column-oriented bytes, MSbit first
 */
static void draw_cbr(u8_t *data, int height) {
#ifndef FULL_REFRESH
	// force addressing mode by rows
	if (AddressMode != AddressMode_Horizontal) {
		AddressMode = AddressMode_Horizontal;
		SSD1306_SetDisplayAddressMode( &Display, AddressMode );
	}
	
	// try to minimize I2C traffic which is very slow
	int rows = (height ? height : Display.Height) / 8;
	for (int r = 0; r < rows; r++) {
		uint8_t first = 0, last;	
		uint8_t *optr = Display.Framebuffer + r*Display.Width, *iptr = data + r;
		
		// row/col swap, frame buffer comparison and bit-reversing
		for (int c = 0; c < Display.Width; c++) {
			u8_t byte = BitReverseTable256[*iptr];
			if (byte != *optr) {
				if (!first) first = c + 1;
				last = c ;
			}	
			*optr++ = byte;
			iptr += rows;
		}
		
		// now update the display by "byte rows"
		if (first--) {
			SSD1306_SetColumnAddress( &Display, first, last );
			SSD1306_SetPageAddress( &Display, r, r);
			SSD1306_WriteRawData( &Display, Display.Framebuffer + r*Display.Width + first, last - first + 1);
		}
	}	
#else
	if (!height) height = Display->Height;

	// copy data in frame buffer	
	for (int c = 0; c < Display.Width; c++)
		for (int r = 0; r < height / 8; r++)
			Display.Framebuffer[c*Display.Height/8 + r] = BitReverseTable256[data[c*height/8 +r]];
		
	SSD1306_SetPageAddress( &Display, 0, height / 8 - 1);
	
	// force addressing mode by columns
	if (AddressMode != AddressMode_Vertical) {
		AddressMode = AddressMode_Vertical;
		SSD1306_SetDisplayAddressMode( &Display, AddressMode );
	}
	
	SSD1306_WriteRawData(&Display, Display.Framebuffer, Display.Width * Display.Height/8);
 #endif	
}

/****************************************************************************************
 * Process graphic display data MSBit first
 * WARNING: this has not been tested yet
 */
static void draw(int x1, int y1, int x2, int y2, bool by_column, u8_t *data) {
	
	if (y1 % 8 || y2 % 8) {
		ESP_LOGW(TAG, "must write rows on byte boundaries (%u,%u) to (%u,%u)", x1, y1, x2, y2);
		return;
	}
	
	// default end point to display size
	if (x2 == -1) x2 = Display.Width - 1;
	if (y2 == -1) y2 = Display.Height - 1;
	
	// set addressing mode to match data
	if (by_column) {
		
		if (AddressMode != AddressMode_Vertical) {
			AddressMode = AddressMode_Vertical;
			SSD1306_SetDisplayAddressMode( &Display, AddressMode );
		}	
		
		// copy the window and do row/col exchange
		for (int r = y1/8; r <=  y2/8; r++) {
			uint8_t *optr = Display.Framebuffer + r*Display.Width + x1, *iptr = data + r;
			for (int c = x1; c <= x2; c++) {
				*optr++ = *iptr;
				iptr += (y2-y1)/8 + 1;
			}	
		}	
	} else {
		// just copy the window inside the frame buffer
		for (int r = y1/8; r <= y2/8; r++) {
			uint8_t *optr = Display.Framebuffer + r*Display.Width + x1, *iptr = data + r*(x2-x1+1);
			for (int c = x1; c <= x2; c++) *optr++ = *iptr++;
		}	
	}
		
	SSD1306_SetColumnAddress( &Display, x1, x2);
	SSD1306_SetPageAddress( &Display, y1/8, y2/8);
	SSD1306_WriteRawData( &Display, data, (x2-x1 + 1) * ((y2-y1)/8 + 1));
}

/****************************************************************************************
 * Brightness
 */
static void brightness(u8_t level) {
	SSD1306_DisplayOn( &Display ); 
	SSD1306_SetContrast( &Display, (uint8_t) level);
}

/****************************************************************************************
 * Display On/Off
 */
static void on(bool state) {
	if (state) SSD1306_DisplayOn( &Display ); 
	else SSD1306_DisplayOff( &Display ); 
}

/****************************************************************************************
 * Update 
 */
static void update(void) {
	SSD1306_Update( &Display );
}



