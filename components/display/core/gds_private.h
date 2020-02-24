#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_attr.h"
#include "gds.h"
#include "gds_err.h"

#define GDS_CLIPDEBUG_NONE 0
#define GDS_CLIPDEBUG_WARNING 1
#define GDS_CLIPDEBUG_ERROR 2

#define SHADOW_BUFFER

#if CONFIG_GDS_CLIPDEBUG == GDS_CLIPDEBUG_NONE
    /*
     * Clip silently with no console output.
     */
    #define ClipDebug( x, y )
#elif CONFIG_GDS_CLIPDEBUG == GDS_CLIPDEBUG_WARNING
    /*
     * Log clipping to the console as a warning.
     */
    #define ClipDebug( x, y ) { \
        ESP_LOGW( __FUNCTION__, "Line %d: Pixel at %d, %d CLIPPED", __LINE__, x, y ); \
    }
#elif CONFIG_GDS_CLIPDEBUG == GDS_CLIPDEBUG_ERROR
    /*
     * Log clipping as an error to the console.
     * Also invokes an abort with stack trace.
     */
    #define ClipDebug( x, y ) { \
        ESP_LOGE( __FUNCTION__, "Line %d: Pixel at %d, %d CLIPPED, ABORT", __LINE__, x, y ); \
        abort( ); \
    }
#endif


#define GDS_ALWAYS_INLINE __attribute__( ( always_inline ) )

#define MAX_LINES	8

#if ! defined BIT
#define BIT( n ) ( 1 << n )
#endif

struct GDS_Device;
struct GDS_FontDef;

/*
 * These can optionally return a succeed/fail but are as of yet unused in the driver.
 */
typedef bool ( *WriteCommandProc ) ( struct GDS_Device* Device, uint8_t Command );
typedef bool ( *WriteDataProc ) ( struct GDS_Device* Device, const uint8_t* Data, size_t DataLength );

struct spi_device_t;
typedef struct spi_device_t* spi_device_handle_t;

#define IF_SPI	0
#define IF_I2C	1

struct GDS_Device {
	uint8_t IF;
	union {
		// I2C Specific
		struct {
			uint8_t Address;
		};
		// SPI specific
		struct {
			spi_device_handle_t SPIHandle;
			int8_t RSTPin;
			int8_t CSPin;
		};
	};	

    // cooked text mode
	struct {
		int16_t Y, Space;
		const struct GDS_FontDef* Font;
	} Lines[MAX_LINES];
	
	uint16_t Width;
    uint16_t Height;
	uint8_t Depth;
		
	uint8_t* Framebuffer, *Shadowbuffer;
    uint16_t FramebufferSize;
	bool Dirty;

	// default fonts when using direct draw	
	const struct GDS_FontDef* Font;
    bool FontForceProportional;
    bool FontForceMonospace;

	// various driver-specific method
	bool (*Init)( struct GDS_Device* Device);
	void (*SetContrast)( struct GDS_Device* Device, uint8_t Contrast );
	void (*DisplayOn)( struct GDS_Device* Device );
	void (*DisplayOff)( struct GDS_Device* Device );
	void (*Update)( struct GDS_Device* Device );
	void (*DrawPixelFast)( struct GDS_Device* Device, int X, int Y, int Color );
	void (*SetHFlip)( struct GDS_Device* Device, bool On );
	void (*SetVFlip)( struct GDS_Device* Device, bool On );
	    
	// interface-specific methods	
    WriteCommandProc WriteCommand;
    WriteDataProc WriteData;
};

bool GDS_Reset( struct GDS_Device* Device );

void IRAM_ATTR 	GDS_DrawPixelFast( struct GDS_Device* Device, int X, int Y, int Color );
void IRAM_ATTR 	GDS_DrawPixel4Fast( struct GDS_Device* Device, int X, int Y, int Color );

inline bool IsPixelVisible( struct GDS_Device* Device, int x, int y )  {
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

inline void IRAM_ATTR GDS_DrawPixel( struct GDS_Device* Device, int x, int y, int Color ) {
    if ( IsPixelVisible( Device, x, y ) == true ) {
        Device->DrawPixelFast( Device, x, y, Color );
    }
}

inline void IRAM_ATTR GDS_DrawPixel4( struct GDS_Device* Device, int x, int y, int Color ) {
    if ( IsPixelVisible( Device, x, y ) == true ) {
        Device->DrawPixelFast( Device, x, y, Color );
    }
}



