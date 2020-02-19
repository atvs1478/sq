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

#include "ssd13x6.h"
#include "ssd13x6_draw.h"
#include "ssd13x6_font.h"
#include "ssd13x6_default_if.h"

#define I2C_ADDRESS	0x3C

static const char *TAG = "display";

#define max(a,b) (((a) > (b)) ? (a) : (b))

// handlers
static bool init(char *config, char *welcome);
static void clear(bool full, ...);
static bool set_font(int num, enum display_font_e font, int space);
static void text(enum display_font_e font, enum display_pos_e pos, int attribute, char *text, ...);
static bool line(int num, int x, int attribute, char *text);
static int stretch(int num, char *string, int max);
static void draw_cbr(u8_t *data, int width, int height);
static void draw_raw(int x1, int y1, int x2, int y2, bool by_column, bool MSb, u8_t *data);
static void draw_line(int x1, int y1, int x2, int y2);
static void draw_box( int x1, int y1, int x2, int y2, bool fill);
static void brightness(u8_t level);
static void on(bool state);
static void update(void);

// display structure for others to use
struct display_s SSD13x6_display = { 0, 0, true,
									init, clear, set_font, on, brightness, 
									text, line, stretch, update, draw_raw, draw_cbr, draw_line, draw_box };

// SSD13x6 specific function
static struct SSD13x6_Device Display;

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
	const struct SSD13x6_FontDef *font;
} lines[MAX_LINES];

/****************************************************************************************
 * 
 */
static bool init(char *config, char *welcome) {
	bool res = true;
	int width = -1, height = -1, model = SSD1306;
	char *p;
	
	ESP_LOGI(TAG, "Initializing display with config: %s",config);
	
	// no time for smart parsing - this is for tinkerers
	if ((p = strcasestr(config, "width")) != NULL) width = atoi(strchr(p, '=') + 1);
	if ((p = strcasestr(config, "height")) != NULL) height = atoi(strchr(p, '=') + 1);
	
	if (strcasestr(config, "ssd1326")) model = SSD1326;
	else if (strcasestr(config, "sh1106")) model = SH1106;
	
	if (width == -1 || height == -1) {
		ESP_LOGW(TAG, "No display configured %s [%d x %d]", config, width, height);
		return false;
	}	

	if (strstr(config, "I2C") && i2c_system_port != -1) {
		int address = I2C_ADDRESS;
				
		if ((p = strcasestr(config, "address")) != NULL) address = atoi(strchr(p, '=') + 1);
		
		SSD13x6_I2CMasterInitDefault( i2c_system_port, -1, -1 ) ;
		SSD13x6_I2CMasterAttachDisplayDefault( &Display, model, width, height, address, -1 );
		SSD13x6_SetHFlip( &Display, strcasestr(config, "HFlip") ? true : false);
		SSD13x6_SetVFlip( &Display, strcasestr(config, "VFlip") ? true : false);
		SSD13x6_SetFont( &Display, &Font_droid_sans_fallback_15x17 );
		SSD13x6_display.width = width;
		SSD13x6_display.height = height;
		
		text(DISPLAY_FONT_MEDIUM, DISPLAY_CENTERED, DISPLAY_CLEAR | DISPLAY_UPDATE, welcome);
		
		ESP_LOGI(TAG, "Display is I2C on port %u", address);
	} else if (strstr(config, "SPI") && spi_system_host != -1) {
		int CS_pin = -1, speed = 0;
		
		if ((p = strcasestr(config, "cs")) != NULL) CS_pin = atoi(strchr(p, '=') + 1);
		if ((p = strcasestr(config, "speed")) != NULL) speed = atoi(strchr(p, '=') + 1);
		
		SSD13x6_SPIMasterInitDefault( spi_system_host, spi_system_dc_gpio );
        SSD13x6_SPIMasterAttachDisplayDefault( &Display, model, width, height, CS_pin, -1, speed );
		SSD13x6_SetFont( &Display, &Font_droid_sans_fallback_15x17 );
		SSD13x6_display.width = width;
		SSD13x6_display.height = height;
		
		text(DISPLAY_FONT_MEDIUM, DISPLAY_CENTERED, DISPLAY_CLEAR | DISPLAY_UPDATE, welcome);
		
		ESP_LOGI(TAG, "Display is SPI host %u with cs:%d", spi_system_host, CS_pin);
		
	} else {
		res = false;
		ESP_LOGW(TAG, "Unknown display type or no serial interface configured");
	}

	return res;
}

/****************************************************************************************
 * 
 */
static void clear(bool full, ...) {
	bool commit = true;
	
	if (full) {
		SSD13x6_Clear( &Display, SSD_COLOR_BLACK ); 
	} else {
		va_list args;
		va_start(args, full);
		commit = va_arg(args, int);
		int x1 = va_arg(args, int), y1 = va_arg(args, int), x2 = va_arg(args, int), y2 = va_arg(args, int);
		if (x2 < 0) x2 = display->width - 1;
		if (y2 < 0) y2 = display->height - 1;
		SSD13x6_ClearWindow( &Display, x1, y1, x2, y2, SSD_COLOR_BLACK );
		va_end(args);
	}
	
	SSD13x6_display.dirty = true;
	if (commit)	update();		
}	

/****************************************************************************************
 *  Set fonts for each line in text mode
 */
static bool set_font(int num, enum display_font_e font, int space) {
	if (--num >= MAX_LINES) return false;
	
	switch(font) {
	case DISPLAY_FONT_LINE_1:	
		lines[num].font = &Font_line_1;
		break;
	case DISPLAY_FONT_LINE_2:	
		lines[num].font = &Font_line_2;
		break;		
	case DISPLAY_FONT_SMALL:	
		lines[num].font = &Font_droid_sans_fallback_11x13;	
		break;
	case DISPLAY_FONT_MEDIUM:			
	case DISPLAY_FONT_DEFAULT:
	default:		
		lines[num].font = &Font_droid_sans_fallback_15x17;	
		break;
	case DISPLAY_FONT_LARGE:	
		lines[num].font = &Font_droid_sans_fallback_24x28;
		break;		
	case DISPLAY_FONT_SEGMENT:			
		if (Display.Height == 32) lines[num].font = &Font_Tarable7Seg_16x32;
		else lines[num].font = &Font_Tarable7Seg_32x64;
		break;		
	}
	
	// re-calculate lines absolute position
	lines[num].space = space;
	lines[0].y = lines[0].space;
	for (int i = 1; i <= num; i++) lines[i].y = lines[i-1].y + lines[i-1].font->Height + lines[i].space;
		
	ESP_LOGI(TAG, "Adding line %u at %d (height:%u)", num + 1, lines[num].y, lines[num].font->Height);
	
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
	
	SSD13x6_SetFont( &Display, lines[num].font );	
	if (attribute & DISPLAY_MONOSPACE) SSD13x6_FontForceMonospace( &Display, true );
	
	width = SSD13x6_FontMeasureString( &Display, text );
	
	// adjusting position, erase only EoL for rigth-justified
	if (x == DISPLAY_RIGHT) x = Display.Width - width - 1;
	else if (x == DISPLAY_CENTER) x = (Display.Width - width) / 2;
	
	// erase if requested
	if (attribute & DISPLAY_CLEAR) {
		int y_min = max(0, lines[num].y), y_max = max(0, lines[num].y + lines[num].font->Height);
		for (int c = (attribute & DISPLAY_ONLY_EOL) ? x : 0; c < Display.Width; c++) 
			for (int y = y_min; y < y_max; y++)
				SSD13x6_DrawPixelFast( &Display, c, y, SSD_COLOR_BLACK );
	}
		
	SSD13x6_FontDrawString( &Display, x, lines[num].y, text, SSD_COLOR_WHITE );
	
	ESP_LOGD(TAG, "displaying %s line %u (x:%d, attr:%u)", text, num+1, x, attribute);
	
	// update whole display if requested
	SSD13x6_display.dirty = true;
	if (attribute & DISPLAY_UPDATE) update();
		
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
	SSD13x6_SetFont( &Display, lines[num].font );	
	if (SSD13x6_FontMeasureString( &Display, string ) <= Display.Width) return 0;
		
	// add some space for better visual 
	strncat(string, space, max-len);
	string[max] = '\0';
	len = strlen(string);
	
	// mark the end of the extended string
	boundary = SSD13x6_FontMeasureString( &Display, string );
			
	// add a full display width	
	while (len < max && SSD13x6_FontMeasureString( &Display, string ) - boundary < Display.Width) {
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

	TextAnchor Anchor = TextAnchor_Center;	
	
	if (attribute & DISPLAY_CLEAR) SSD13x6_Clear( &Display, SSD_COLOR_BLACK );
	
	if (!text) return;
	
	va_start(args, text);
	
	switch(font) {
	case DISPLAY_FONT_LINE_1:	
		SSD13x6_SetFont( &Display, &Font_line_1 );
		break;
	case DISPLAY_FONT_LINE_2:	
		SSD13x6_SetFont( &Display, &Font_line_2 );
		break;		
	case DISPLAY_FONT_SMALL:	
		SSD13x6_SetFont( &Display, &Font_droid_sans_fallback_11x13 );	
		break;	
	case DISPLAY_FONT_MEDIUM:			
	case DISPLAY_FONT_DEFAULT:
	default:
		SSD13x6_SetFont( &Display, &Font_droid_sans_fallback_15x17 );	
		break;		
	case DISPLAY_FONT_LARGE:	
		SSD13x6_SetFont( &Display, &Font_droid_sans_fallback_24x28 );
		break;		
	case DISPLAY_FONT_SEGMENT:			
		if (Display.Height == 32) SSD13x6_SetFont( &Display, &Font_Tarable7Seg_16x32 );
		else SSD13x6_SetFont( &Display, &Font_Tarable7Seg_32x64 );
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
	
	ESP_LOGD(TAG, "SSDD13x6 displaying %s at %u with attribute %u", text, Anchor, attribute);
	
	SSD13x6_FontDrawAnchoredString( &Display, Anchor, text, SSD_COLOR_WHITE );
	
	SSD13x6_display.dirty = true;
	if (attribute & DISPLAY_UPDATE) update();
	
	va_end(args);
}

/****************************************************************************************
 * Process graphic display data from column-oriented data (MSbit first)
 */
static void draw_cbr(u8_t *data, int width, int height) {
	if (!height) height = Display.Height;
	if (!width) width = Display.Width;

	// need to do row/col swap and bit-reverse
	int rows = height / 8;
	for (int r = 0; r < rows; r++) {
		uint8_t *optr = Display.Framebuffer + r*Display.Width, *iptr = data + r;
		for (int c = width; --c >= 0;) {
			*optr++ = BitReverseTable256[*iptr];;
			iptr += rows;
		}	
	}
	
	SSD13x6_display.dirty = true;
}

/****************************************************************************************
 * Process graphic display data MSBit first
 * WARNING: this has not been tested yet
 */
static void draw_raw(int x1, int y1, int x2, int y2, bool by_column, bool MSb, u8_t *data) {
	// default end point to display size
	if (x2 == -1) x2 = Display.Width - 1;
	if (y2 == -1) y2 = Display.Height - 1;
	
	display->dirty = true;
	
	//	not a boundary draw
	if (y1 % 8 || y2 % 8 || x1 % 8 | x2 % 8) {
		ESP_LOGW(TAG, "can't write on non cols/rows boundaries for now");
	} else {	
		// set addressing mode to match data
		if (by_column) {
			// copy the window and do row/col exchange
			for (int r = y1/8; r <=  y2/8; r++) {
				uint8_t *optr = Display.Framebuffer + r*Display.Width + x1, *iptr = data + r;
				for (int c = x1; c <= x2; c++) {
					*optr++ = MSb ? BitReverseTable256[*iptr] : *iptr;
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
	}	
}

/****************************************************************************************
 * Draw line
 */
static void draw_line( int x1, int y1, int x2, int y2) {
	SSD13x6_DrawLine( &Display, x1, y1, x2, y2, SSD_COLOR_WHITE );
	SSD13x6_display.dirty = true;
}

/****************************************************************************************
 * Draw Box
 */
static void draw_box( int x1, int y1, int x2, int y2, bool fill) {
	SSD13x6_DrawBox( &Display, x1, y1, x2, y2, SSD_COLOR_WHITE, fill );
	SSD13x6_display.dirty = true;
}

/****************************************************************************************
 * Brightness
 */
static void brightness(u8_t level) {
	SSD13x6_DisplayOn( &Display ); 
	SSD13x6_SetContrast( &Display, (uint8_t) level);
}

/****************************************************************************************
 * Display On/Off
 */
static void on(bool state) {
	if (state) SSD13x6_DisplayOn( &Display ); 
	else SSD13x6_DisplayOff( &Display ); 
}

/****************************************************************************************
 * Update 
 */
static void update(void) {
	if (SSD13x6_display.dirty) SSD13x6_Update( &Display );
	SSD13x6_display.dirty = false;
}



