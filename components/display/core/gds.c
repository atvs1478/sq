/**
 * Copyright (c) 2017-2018 Tara Keeling
 *				 2020 Philippe G. 
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "gds.h"
#include "gds_private.h"

static struct GDS_Device Display;

static char TAG[] = "gds";

struct GDS_Device*	GDS_AutoDetect( char *Driver, GDS_DetectFunc* DetectFunc[] ) {
	for (int i = 0; DetectFunc[i]; i++) {
		if (DetectFunc[i](Driver, &Display)) {
			ESP_LOGD(TAG, "Detected driver %p", &Display);
			return &Display;
		}	
	}
	
	return NULL;
}

void GDS_ClearExt(struct GDS_Device* Device, bool full, ...) {
	bool commit = true;
	
	if (full) {
		GDS_Clear( Device, GDS_COLOR_BLACK ); 
	} else {
		va_list args;
		va_start(args, full);
		commit = va_arg(args, int);
		int x1 = va_arg(args, int), y1 = va_arg(args, int), x2 = va_arg(args, int), y2 = va_arg(args, int);
		if (x2 < 0) x2 = Device->Width - 1;
		if (y2 < 0) y2 = Device->Height - 1;
		GDS_ClearWindow( Device, x1, y1, x2, y2, GDS_COLOR_BLACK );
		va_end(args);
	}
	
	Device->Dirty = true;
	if (commit)	GDS_Update(Device);		
}	

void GDS_Clear( struct GDS_Device* Device, int Color ) {
    memset( Device->Framebuffer, Color, Device->FramebufferSize );
	Device->Dirty = true;
}

void GDS_ClearWindow( struct GDS_Device* Device, int x1, int y1, int x2, int y2, int Color ) {
	// driver can provide onw optimized clear window
	if (Device->ClearWindow) {
		Device->ClearWindow( Device, x1, y1, x2, y2, Color );
	// cheap optimization on boundaries
	if (x1 == 0 && x2 == Device->Width - 1 && y1 % 8 == 0 && (y2 + 1) % 8 == 0) {
		memset( Device->Framebuffer + (y1 / 8) * Device->Width, 0, (y2 - y1 + 1) / 8 * Device->Width );
	} else {
		for (int y = y1; y <= y2; y++) {
			for (int x = x1; x <= x2; x++) {
				GDS_DrawPixelFast( Device, x, y, Color);
			}		
		}	
	}	
	
	Device->Dirty = true;
}

void GDS_Update( struct GDS_Device* Device ) {
	if (Device->Dirty) Device->Update( Device );
	Device->Dirty = false;
}

bool GDS_Reset( struct GDS_Device* Device ) {
	if ( Device->RSTPin >= 0 ) {
		gpio_set_level( Device->RSTPin, 0 );
		vTaskDelay( pdMS_TO_TICKS( 100 ) );
        gpio_set_level( Device->RSTPin, 1 );
    }
    return true;
}

void GDS_SetContrast( struct GDS_Device* Device, uint8_t Contrast ) { Device->SetContrast( Device, Contrast); }
void GDS_SetHFlip( struct GDS_Device* Device, bool On ) { Device->SetHFlip( Device, On ); }
void GDS_SetVFlip( struct GDS_Device* Device, bool On ) { Device->SetVFlip( Device, On ); }
int	GDS_GetWidth( struct GDS_Device* Device ) { return Device->Width; }
int	GDS_GetHeight( struct GDS_Device* Device ) { return Device->Height; }
void GDS_DisplayOn( struct GDS_Device* Device ) { Device->DisplayOn( Device ); }
void GDS_DisplayOff( struct GDS_Device* Device ) { Device->DisplayOff( Device ); }