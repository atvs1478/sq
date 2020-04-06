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
	GDS_DrawPixel(  Device, X, Y, Color );
}
void IRAM_ATTR GDS_DrawPixelFastExt( struct GDS_Device* Device, int X, int Y, int Color ){
	GDS_DrawPixelFast( Device, X, Y, Color );
}
