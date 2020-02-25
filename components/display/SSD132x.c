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
#include <esp_heap_caps.h>
#include <esp_log.h>

#include "gds.h"
#include "gds_private.h"

#define SHADOW_BUFFER
#define USE_IRAM
#define PAGE_BLOCK	1024

#define min(a,b) (((a) < (b)) ? (a) : (b))

static char TAG[] = "SSD132x";

enum { SSD1326, SSD1327 };

struct SSD132x_Private {
	uint8_t *iRAM;
	uint8_t ReMap, PageSize;
	uint8_t Model;
};

// Functions are not deckared to minimize # of lines

static void SetColumnAddress( struct GDS_Device* Device, uint8_t Start, uint8_t End ) {
	Device->WriteCommand( Device, 0x15 );
	Device->WriteCommand( Device, Start );
	Device->WriteCommand( Device, End );
}
static void SetRowAddress( struct GDS_Device* Device, uint8_t Start, uint8_t End ) {
	// can be by row, not by page (see Update Optimization)
	Device->WriteCommand( Device, 0x75 );
	Device->WriteCommand( Device, Start );
	Device->WriteCommand( Device, End );
}

static void Update4( struct GDS_Device* Device ) {
	struct SSD132x_Private *Private = (struct SSD132x_Private*) Device->Private;
	int r;
	
	// always update by full lines
	SetColumnAddress( Device, 0, Device->Width / 2 - 1);
	
#ifdef SHADOW_BUFFER
	uint16_t *optr = (uint16_t*) Device->Shadowbuffer, *iptr = (uint16_t*) Device->Framebuffer;
	bool dirty = false;
	int page;
	
	for (r = 0, page = 0; r < Device->Height; r++) {
		// look for change and update shadow (cheap optimization = width always / by 2)
		for (int c = Device->Width / 2 / 2; --c >= 0;) {
			if (*optr != *iptr) {
				dirty = true;
				*optr = *iptr;
			}
			iptr++; optr++;
		}
		
		// one line done, check for page boundary
		if (++page == Private->PageSize) {
			if (dirty) {
				SetRowAddress( Device, r - page + 1, r );
				// own use of IRAM has not proven to be much better than letting SPI do its copy
				if (Private->iRAM) {
					memcpy(Private->iRAM, Device->Shadowbuffer + (r - page + 1) * Device->Width / 2, Device->Width * page / 2 );
					Device->WriteData( Device, Private->iRAM, Device->Width * page / 2 );
				} else	{
					Device->WriteData( Device, Device->Shadowbuffer + (r - page + 1) * Device->Width / 2, Device->Width * page / 2 );					
				}	
				dirty = false;
			}	
			page = 0;
		}	
	}	
#else
	for (r = 0; r < Device->Height; r += Private->PageSize) {
		SetRowAddress( Device, r, r + Private->PageSize - 1 );
		Device->WriteData( Device, Device->Framebuffer + r * Device->Width / 2, Device->Width * Private->PageSize / 2 );
	}	
#endif	
}

static void Update1( struct GDS_Device* Device ) {
#ifdef SHADOW_BUFFER
	// not sure the compiler does not have to redo all calculation in for loops, so local it is
	int width = Device->Width, rows = Device->Height / 8;
	uint8_t *optr = Device->Shadowbuffer, *iptr = Device->Framebuffer;
	
	// by row, find first and last columns that have been updated
	for (int r = 0; r < rows; r++) {
		uint8_t first = 0, last;	
		for (int c = 0; c < width; c++) {
			if (*iptr != *optr) {
				if (!first) first = c + 1;
				last = c ;
			}	
			*optr++ = *iptr++;
		}
		
		// now update the display by "byte rows"
		if (first--) {
			SetColumnAddress( Device, first, last );
			SetRowAddress( Device, r, r);
			Device->WriteData( Device, Device->Shadowbuffer + r*width + first, last - first + 1);
		}
	}	
#else	
	// automatic counter and end Page/Column
	SetColumnAddress( Device, 0, Device->Width - 1);
	SetPageAddress( Device, 0, Device->Height / 8 - 1);
	Device->WriteData( Device, Device->Framebuffer, Device->FramebufferSize );
#endif	
}

static void SetHFlip( struct GDS_Device* Device, bool On ) { 
	struct SSD132x_Private *Private = (struct SSD132x_Private*) Device->Private;
	if (Private->Model == SSD1326) Private->ReMap = On ? (Private->ReMap | ((1 << 0) | (1 << 2))) : (Private->ReMap & ~((1 << 0) | (1 << 2)));
	else Private->ReMap = On ? (Private->ReMap | ((1 << 0) | (1 << 1))) : (Private->ReMap & ~((1 << 0) | (1 << 1)));
	Device->WriteCommand( Device, 0xA0 );
	Device->WriteCommand( Device, Private->ReMap );
}	

static void SetVFlip( struct GDS_Device *Device, bool On ) { 
	struct SSD132x_Private *Private = (struct SSD132x_Private*) Device->Private;
	if (Private->Model == SSD1326) Private->ReMap = On ? (Private->ReMap | (1 << 1)) : (Private->ReMap & ~(1 << 1));
	else Private->ReMap = On ? (Private->ReMap | (1 << 4)) : (Private->ReMap & ~(1 << 4));
	Device->WriteCommand( Device, 0xA0 );
	Device->WriteCommand( Device, Private->ReMap );
}	
	
static void DisplayOn( struct GDS_Device* Device ) { Device->WriteCommand( Device, 0xAF ); }
static void DisplayOff( struct GDS_Device* Device ) { Device->WriteCommand( Device, 0xAE ); }

static void SetContrast( struct GDS_Device* Device, uint8_t Contrast ) {
    Device->WriteCommand( Device, 0x81 );
    Device->WriteCommand( Device, Contrast );
}

static bool Init( struct GDS_Device* Device ) {
	struct SSD132x_Private *Private = (struct SSD132x_Private*) Device->Private;
	
	// find a page size that is not too small is an integer of height
	Private->PageSize = min(8, PAGE_BLOCK / (Device->Width / 2));
	Private->PageSize = Device->Height / (Device->Height / Private->PageSize) ;	
	
#ifdef USE_IRAM	
	// let SPI driver allocate memory, it has not proven to be more efficient
	if (Device->IF == IF_SPI) Private->iRAM = heap_caps_malloc( Private->PageSize * Device->Width / 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA );
#endif	
	Device->FramebufferSize = ( Device->Width * Device->Height ) / 2;	
	Device->Framebuffer = calloc( 1, Device->FramebufferSize );
    NullCheck( Device->Framebuffer, return false );
	
#ifdef SHADOW_BUFFER	
	Device->Shadowbuffer = malloc( Device->FramebufferSize );
	NullCheck( Device->Shadowbuffer, return false );
	memset(Device->Shadowbuffer, 0xFF, Device->FramebufferSize);
#endif	

	ESP_LOGI(TAG, "SSD1326/7 with bit depth %u, page %u, iRAM %p", Device->Depth, Private->PageSize, Private->iRAM);
		
	// need to be off and disable display RAM
	Device->DisplayOff( Device );
    Device->WriteCommand( Device, 0xA5 );
	
	// need COM split (6)
	Private->ReMap = 1 << 6;
	// MUX Ratio
    Device->WriteCommand( Device, 0xA8 );
    Device->WriteCommand( Device, Device->Height - 1);
	// Display Offset
    Device->WriteCommand( Device, 0xA2 );
    Device->WriteCommand( Device, 0 );
	// Display Start Line
    Device->WriteCommand( Device, 0xA1 );
	Device->WriteCommand( Device, 0x00 );
	Device->SetContrast( Device, 0x7F );
	// set flip modes
	Device->SetVFlip( Device, false );
	Device->SetHFlip( Device, false );
	// no Display Inversion
    Device->WriteCommand( Device, 0xA6 );
	// set Clocks
    Device->WriteCommand( Device, 0xB3 );
    Device->WriteCommand( Device, ( 0x08 << 4 ) | 0x00 );
	// set Adressing Mode Horizontal
	Private->ReMap |= (0 << 2);
	// set monotchrome mode if required
	if (Device->Depth == 1) Private->ReMap |= (1 << 4);
	// write ReMap byte
	Device->WriteCommand( Device, 0xA0 );
	Device->WriteCommand( Device, Private->ReMap );		
		
	// gone with the wind
	Device->WriteCommand( Device, 0xA4 );
	Device->DisplayOn( Device );
	Device->Update( Device );
	
	return true;
}	

static const struct GDS_Device SSD132x = {
	.DisplayOn = DisplayOn, .DisplayOff = DisplayOff, .SetContrast = SetContrast,
	.SetVFlip = SetVFlip, .SetHFlip = SetHFlip,
	.Update = Update4, .Init = Init,
};	

struct GDS_Device* SSD132x_Detect(char *Driver, struct GDS_Device* Device) {
	uint8_t Model;
	
	if (!strcasestr(Driver, "SSD1326")) Model = SSD1326;
	else if (!strcasestr(Driver, "SSD1327")) Model = SSD1327;
	else return NULL;
	
	if (!Device) Device = calloc(1, sizeof(struct GDS_Device));
	*Device = SSD132x;	
	((struct SSD132x_Private*) Device->Private)->Model = Model;
	sscanf(Driver, "%*[^:]:%c", &Device->Depth);
	if (Model == SSD1326 && Device->Depth == 1) Device->Update = Update1;
	else Device->Depth = 4;
		
	return Device;
}