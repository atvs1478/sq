/**
 * Copyright (c) 2017-2018 Tara Keeling
 *				 2020 Philippe G.
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "gds_private.h"
#include "gds.h"
#include "gds_draw.h"

void IRAM_ATTR GDS_DrawPixelExt( struct GDS_Device* Device, int X, int Y, int Color ){
	if ( IsPixelVisible( Device, X, Y ) == true ) {
	        GDS_DrawPixelFast( Device, X, Y, Color );
	    }
}
void IRAM_ATTR GDS_DrawPixelFastExt( struct GDS_Device* Device, int X, int Y, int Color ){
	if (Device->DrawPixelFast) Device->DrawPixelFast( Device, X, Y, Color );
		else if (Device->Depth == 4) GDS_DrawPixel4Fast( Device, X, Y, Color);
		else if (Device->Depth == 1) GDS_DrawPixel1Fast( Device, X, Y, Color);
}
