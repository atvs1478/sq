#ifndef _GDS_H_
#define _GDS_H_

#include <stdint.h>
#include <stdbool.h>

#define GDS_COLOR_BLACK 0
#define GDS_COLOR_WHITE 1
#define GDS_COLOR_XOR 2

struct GDS_Device;
struct GDS_FontDef;

typedef struct GDS_Device* (*GDS_DetectFunc)(char *Driver, struct GDS_Device *Device);

struct GDS_Device*	GDS_AutoDetect( char *Driver, GDS_DetectFunc[] );

void 	GDS_SetContrast( struct GDS_Device* Device, uint8_t Contrast );
void 	GDS_DisplayOn( struct GDS_Device* Device );
void 	GDS_DisplayOff( struct GDS_Device* Device ); 
void 	GDS_Update( struct GDS_Device* Device );
void 	GDS_SetHFlip( struct GDS_Device* Device, bool On );
void 	GDS_SetVFlip( struct GDS_Device* Device, bool On );
int 	GDS_GetWidth( struct GDS_Device* Device );
int 	GDS_GetHeight( struct GDS_Device* Device );
void 	GDS_ClearExt( struct GDS_Device* Device, bool full, ...);
void 	GDS_Clear( struct GDS_Device* Device, int Color );
void 	GDS_ClearWindow( struct GDS_Device* Device, int x1, int y1, int x2, int y2, int Color );

#endif
