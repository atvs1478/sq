#ifndef _SSD13x6_DRAW_H_
#define _SSD13x6_DRAW_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "sdkconfig.h"

#define SSD13x6_CLIPDEBUG_NONE 0
#define SSD13x6_CLIPDEBUG_WARNING 1
#define SSD13x6_CLIPDEBUG_ERROR 2

#if CONFIG_SSD13x6_CLIPDEBUG == SSD13x6_CLIPDEBUG_NONE
    /*
     * Clip silently with no console output.
     */
    #define ClipDebug( x, y )
#elif CONFIG_SSD13x6_CLIPDEBUG == SSD13x6_CLIPDEBUG_WARNING
    /*
     * Log clipping to the console as a warning.
     */
    #define ClipDebug( x, y ) { \
        ESP_LOGW( __FUNCTION__, "Line %d: Pixel at %d, %d CLIPPED", __LINE__, x, y ); \
    }
#elif CONFIG_SSD13x6_CLIPDEBUG == SSD13x6_CLIPDEBUG_ERROR
    /*
     * Log clipping as an error to the console.
     * Also invokes an abort with stack trace.
     */
    #define ClipDebug( x, y ) { \
        ESP_LOGE( __FUNCTION__, "Line %d: Pixel at %d, %d CLIPPED, ABORT", __LINE__, x, y ); \
        abort( ); \
    }
#endif

#define SSD_COLOR_BLACK 0
#define SSD_COLOR_WHITE 1
#define SSD_COLOR_XOR 2

void SSD13x6_Clear( struct SSD13x6_Device* DeviceHandle, int Color );
void SSD13x6_ClearWindow( struct SSD13x6_Device* DeviceHandle, int x1, int y1, int x2, int y2, int Color );
void SSD13x6_DrawPixel( struct SSD13x6_Device* DeviceHandle, int X, int Y, int Color );
void SSD13x6_DrawPixelFast( struct SSD13x6_Device* DeviceHandle, int X, int Y, int Color );
void SSD13x6_DrawHLine( struct SSD13x6_Device* DeviceHandle, int x, int y, int Width, int Color );
void SSD13x6_DrawVLine( struct SSD13x6_Device* DeviceHandle, int x, int y, int Height, int Color );
void SSD13x6_DrawLine( struct SSD13x6_Device* DeviceHandle, int x0, int y0, int x1, int y1, int Color );
void SSD13x6_DrawBox( struct SSD13x6_Device* DeviceHandle, int x1, int y1, int x2, int y2, int Color, bool Fill );

#ifdef __cplusplus
}
#endif

#endif
