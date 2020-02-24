#ifndef _GDS_DRAW_H_
#define _GDS_DRAW_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_attr.h"

#ifdef __cplusplus
extern "C" {
#endif

struct GDS_Device;

void IRAM_ATTR GDS_DrawHLine( struct GDS_Device* Device, int x, int y, int Width, int Color );
void IRAM_ATTR GDS_DrawVLine( struct GDS_Device* Device, int x, int y, int Height, int Color );
void IRAM_ATTR GDS_DrawLine( struct GDS_Device* Device, int x0, int y0, int x1, int y1, int Color );
void IRAM_ATTR GDS_DrawBox( struct GDS_Device* Device, int x1, int y1, int x2, int y2, int Color, bool Fill );

// draw a bitmap with source 1-bit depth organized in column and col0 = bit7 of byte 0
void IRAM_ATTR GDS_DrawBitmapCBR( struct GDS_Device* Device, uint8_t *Data, int Width, int Height);

#ifdef __cplusplus
}
#endif

#endif
