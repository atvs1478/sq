/**
 * Copyright (c) 2017-2018 Tara Keeling
 *				 2020 Philippe G.
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <esp_attr.h>

#include "gds.h"
#include "gds_private.h"
#include "gds_draw.h"

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

__attribute__( ( always_inline ) ) static inline bool IsPixelVisible( struct GDS_Device* Device, int x, int y )  {
    bool Result = (
        ( x >= 0 ) &&
        ( x < Device->Width ) &&
        ( y >= 0 ) &&
        ( y < Device->Height )
    ) ? true : false;

#if CONFIG_GDS_CLIPDEBUG > 0
    if ( Result == false ) {
        ClipDebug( x, y );
    }
#endif

    return Result;
}

__attribute__( ( always_inline ) ) static inline void SwapInt( int* a, int* b ) {
    int Temp = *b;

    *b = *a;
    *a = Temp;
}

inline void IRAM_ATTR GDS_DrawPixelFast( struct GDS_Device* Device, int X, int Y, int Color ) {
    uint32_t YBit = ( Y & 0x07 );
    uint8_t* FBOffset = NULL;

    /* 
     * We only need to modify the Y coordinate since the pitch
     * of the screen is the same as the width.
     * Dividing Y by 8 gives us which row the pixel is in but not
     * the bit position.
     */
    Y>>= 3;

    FBOffset = Device->Framebuffer + ( ( Y * Device->Width ) + X );

    if ( Color == GDS_COLOR_XOR ) {
        *FBOffset ^= BIT( YBit );
    } else {
        *FBOffset = ( Color == GDS_COLOR_WHITE ) ? *FBOffset | BIT( YBit ) : *FBOffset & ~BIT( YBit );
    }
}

void IRAM_ATTR GDS_DrawPixel( struct GDS_Device* Device, int x, int y, int Color ) {
    if ( IsPixelVisible( Device, x, y ) == true ) {
        Device->DrawPixelFast( Device, x, y, Color );
    }
}

inline void IRAM_ATTR GDS_DrawPixel4Fast( struct GDS_Device* Device, int X, int Y, int Color ) {
    uint32_t YBit = ( Y & 0x07 );
    uint8_t* FBOffset = NULL;

    /* 
     * We only need to modify the Y coordinate since the pitch
     * of the screen is the same as the width.
     * Dividing Y by 8 gives us which row the pixel is in but not
     * the bit position.
     */
    Y>>= 3;

    FBOffset = Device->Framebuffer + ( ( Y * Device->Width ) + X );

    if ( Color == GDS_COLOR_XOR ) {
        *FBOffset ^= BIT( YBit );
    } else {
        *FBOffset = ( Color == GDS_COLOR_WHITE ) ? *FBOffset | BIT( YBit ) : *FBOffset & ~BIT( YBit );
    }
}

void IRAM_ATTR GDS_DrawPixel4( struct GDS_Device* Device, int x, int y, int Color ) {
    if ( IsPixelVisible( Device, x, y ) == true ) {
        Device->DrawPixelFast( Device, x, y, Color );
    }
}

void IRAM_ATTR GDS_DrawHLine( struct GDS_Device* Device, int x, int y, int Width, int Color ) {
    int XEnd = x + Width;

	Device->Dirty = true;
	
	if (x < 0) x = 0;
	if (XEnd >= Device->Width) XEnd = Device->Width - 1;
	
	if (y < 0) y = 0;
	else if (y >= Device->Height) x = Device->Height - 1;

    for ( ; x < XEnd; x++ ) {
    //    if ( IsPixelVisible( Device, x, y ) == true ) {
            Device->DrawPixelFast( Device, x, y, Color );
      //  } else {
       //     break;
       // }
    }
}

void IRAM_ATTR GDS_DrawVLine( struct GDS_Device* Device, int x, int y, int Height, int Color ) {
    int YEnd = y + Height;

	Device->Dirty = true;

    for ( ; y < YEnd; y++ ) {
        if ( IsPixelVisible( Device, x, y ) == true ) {
            Device->DrawPixel( Device, x, y, Color );
        } else {
            break;
        }
    }
}

static inline void IRAM_ATTR DrawWideLine( struct GDS_Device* Device, int x0, int y0, int x1, int y1, int Color ) {
    int dx = ( x1 - x0 );
    int dy = ( y1 - y0 );
    int Error = 0;
    int Incr = 1;
    int x = x0;
    int y = y0;

    if ( dy < 0 ) {
        Incr = -1;
        dy = -dy;
    }

    Error = ( dy * 2 ) - dx;

    for ( ; x < x1; x++ ) {
        if ( IsPixelVisible( Device, x, y ) == true ) {
            Device->DrawPixelFast( Device, x, y, Color );
        }

        if ( Error > 0 ) {
            Error-= ( dx * 2 );
            y+= Incr;
        }

        Error+= ( dy * 2 );
    }
}

static inline void IRAM_ATTR DrawTallLine( struct GDS_Device* Device, int x0, int y0, int x1, int y1, int Color ) {
    int dx = ( x1 - x0 );
    int dy = ( y1 - y0 );
    int Error = 0;
    int Incr = 1;
    int x = x0;
    int y = y0;

    if ( dx < 0 ) {
        Incr = -1;
        dx = -dx;
    }

    Error = ( dx * 2 ) - dy;

    for ( ; y < y1; y++ ) {
        if ( IsPixelVisible( Device, x, y ) == true ) {
            Device->DrawPixelFast( Device, x, y, Color );
        }

        if ( Error > 0 ) {
            Error-= ( dy * 2 );
            x+= Incr;
        }

        Error+= ( dx * 2 );
    }
}

void IRAM_ATTR GDS_DrawLine( struct GDS_Device* Device, int x0, int y0, int x1, int y1, int Color ) {
    if ( x0 == x1 ) {
        GDS_DrawVLine( Device, x0, y0, ( y1 - y0 ), Color );
    } else if ( y0 == y1 ) {
        GDS_DrawHLine( Device, x0, y0, ( x1 - x0 ), Color );
    } else {
		Device->Dirty = true;
        if ( abs( x1 - x0 ) > abs( y1 - y0 ) ) {
            /* Wide ( run > rise ) */
            if ( x0 > x1 ) {
                SwapInt( &x0, &x1 );
                SwapInt( &y0, &y1 );
            }

            DrawWideLine( Device, x0, y0, x1, y1, Color );
        } else {
            /* Tall ( rise > run ) */
            if ( y0 > y1 ) {
                SwapInt( &y0, &y1 );
                SwapInt( &x0, &x1 );
            }

            DrawTallLine( Device, x0, y0, x1, y1, Color );
        }
    }
}

void IRAM_ATTR GDS_DrawBox( struct GDS_Device* Device, int x1, int y1, int x2, int y2, int Color, bool Fill ) {
    int Width = ( x2 - x1 );
    int Height = ( y2 - y1 );

	Device->Dirty = true;
	
    if ( Fill == false ) {
        /* Top side */
        GDS_DrawHLine( Device, x1, y1, Width, Color );

        /* Bottom side */
        GDS_DrawHLine( Device, x1, y1 + Height, Width, Color );

        /* Left side */
        GDS_DrawVLine( Device, x1, y1, Height, Color );

        /* Right side */
        GDS_DrawVLine( Device, x1 + Width, y1, Height, Color );
    } else {
        /* Fill the box by drawing horizontal lines */
        for ( ; y1 <= y2; y1++ ) {
            GDS_DrawHLine( Device, x1, y1, Width, Color );
        }
    }
}


/****************************************************************************************
 * Process graphic display data from column-oriented data (MSbit first)
 */
void GDS_DrawBitmapCBR(struct GDS_Device* Device, uint8_t *Data, int Width, int Height) {
	if (!Height) Height = Device->Height;
	if (!Width) Width = Device->Width;

	// need to do row/col swap and bit-reverse
	int Rows = Height / 8;
	for (int r = 0; r < Rows; r++) {
		uint8_t *optr = Device->Framebuffer + r*Device->Width, *iptr = Data + r;
		for (int c = Width; --c >= 0;) {
			*optr++ = BitReverseTable256[*iptr];;
			iptr += Rows;
		}	
	}
	
	Device->Dirty = true;
}

/****************************************************************************************
 * Process graphic display data MSBit first
 * WARNING: this has not been tested yet
 */
 /*
static void draw_raw(int x1, int y1, int x2, int y2, bool by_column, bool MSb, u8_t *data) {
	// default end point to display size
	if (x2 == -1) x2 = Display.Width - 1;
	if (y2 == -1) y2 = Display.Height - 1;
	
	display->dirty = true;
	
	//	not a boundary draw
	// same comment about bit depth
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
*/
