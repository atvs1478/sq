#ifndef _SSD13X6_H_
#define _SSD13X6_H_

/* For uint(X)_t */
#include <stdint.h>

/* For booooool */
#include <stdbool.h>

#include "sdkconfig.h"
#include "ssd13x6_err.h"

#define SSD_ALWAYS_INLINE __attribute__( ( always_inline ) )

#define CAPS_COLUMN_RANGE		0x01
#define CAPS_PAGE_RANGE			0x02
#define CAPS_ADDRESS_VERTICAL	0x04

#if ! defined BIT
#define BIT( n ) ( 1 << n )
#endif

typedef uint8_t SSDCmd;

typedef enum {
    AddressMode_Horizontal = 0,
    AddressMode_Vertical,
    AddressMode_Page,
    AddressMode_Invalid
} SSD13x6_AddressMode;

struct SSD13x6_Device;

/*
 * These can optionally return a succeed/fail but are as of yet unused in the driver.
 */
typedef bool ( *WriteCommandProc ) ( struct SSD13x6_Device* DeviceHandle, SSDCmd Command );
typedef bool ( *WriteDataProc ) ( struct SSD13x6_Device* DeviceHandle, const uint8_t* Data, size_t DataLength );
typedef bool ( *ResetProc ) ( struct SSD13x6_Device* DeviceHandle );

struct spi_device_t;
typedef struct spi_device_t* spi_device_handle_t;

struct SSD13x6_FontDef;

struct SSD13x6_Device {
    /* I2C Specific */
    int Address;

    /* SPI Specific */
    spi_device_handle_t SPIHandle;
    int RSTPin;
    int CSPin;

    /* Everything else */
    int Width;
    int Height;

	enum { SSD1306, SSD1326, SH1106 } Model;
	uint8_t ReMap;
    uint8_t* Framebuffer;
    int FramebufferSize;

    WriteCommandProc WriteCommand;
    WriteDataProc WriteData;
    ResetProc Reset;

    const struct SSD13x6_FontDef* Font;
    bool FontForceProportional;
    bool FontForceMonospace;
};

void SSD13x6_SetMuxRatio( struct SSD13x6_Device* DeviceHandle, uint8_t Ratio );
void SSD13x6_SetDisplayOffset( struct SSD13x6_Device* DeviceHandle, uint8_t Offset );
void SSD13x6_SetDisplayStartLines( struct SSD13x6_Device* DeviceHandle );

void SSD13x6_SetSegmentRemap( struct SSD13x6_Device* DeviceHandle, bool Remap );

void SSD13x6_SetContrast( struct SSD13x6_Device* DeviceHandle, uint8_t Contrast );
void SSD13x6_EnableDisplayRAM( struct SSD13x6_Device* DeviceHandle );
void SSD13x6_DisableDisplayRAM( struct SSD13x6_Device* DeviceHandle );
void SSD13x6_SetInverted( struct SSD13x6_Device* DeviceHandle, bool Inverted );
void SSD13x6_SetHFlip( struct SSD13x6_Device* DeviceHandle, bool On );
void SSD13x6_SetVFlip( struct SSD13x6_Device* DeviceHandle, bool On );
void SSD13x6_DisplayOn( struct SSD13x6_Device* DeviceHandle );
void SSD13x6_DisplayOff( struct SSD13x6_Device* DeviceHandle ); 
void SSD13x6_SetDisplayAddressMode( struct SSD13x6_Device* DeviceHandle, SSD13x6_AddressMode AddressMode );
void SSD13x6_Update( struct SSD13x6_Device* DeviceHandle );
void SSD13x6_SetDisplayClocks( struct SSD13x6_Device* DeviceHandle, uint32_t DisplayClockDivider, uint32_t OSCFrequency );
void SSD13x6_SetColumnAddress( struct SSD13x6_Device* DeviceHandle, uint8_t Start, uint8_t End );
void SSD13x6_SetPageAddress( struct SSD13x6_Device* DeviceHandle, uint8_t Start, uint8_t End );
bool SSD13x6_HWReset( struct SSD13x6_Device* DeviceHandle );
bool SSD13x6_Init_I2C( struct SSD13x6_Device* DeviceHandle, int Width, int Height, int I2CAddress, int ResetPin, WriteCommandProc WriteCommand, WriteDataProc WriteData, ResetProc Reset );
bool SSD13x6_Init_SPI( struct SSD13x6_Device* DeviceHandle, int Width, int Height, int ResetPin, int CSPin, spi_device_handle_t SPIHandle, WriteCommandProc WriteCommand, WriteDataProc WriteData, ResetProc Reset );
int  SSD13x6_GetCaps( struct SSD13x6_Device* DeviceHandle );

void SSD13x6_WriteRawData( struct SSD13x6_Device* DeviceHandle, uint8_t* Data, size_t DataLength );

#endif
