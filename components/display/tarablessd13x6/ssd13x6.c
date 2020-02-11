/**
 * Copyright (c) 2017-2018 Tara Keeling
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
#include <esp_heap_caps.h>

#include "ssd13x6.h"

// used by both but different
static uint8_t SSDCmd_Set_Display_Start_Line;
static uint8_t SSDCmd_Set_Display_Offset;
static uint8_t SSDCmd_Set_Column_Address;
static uint8_t SSDCmd_Set_Display_CLK;
static uint8_t SSDCmd_Set_Page_Address;

// misc boundaries
static uint8_t SSD13x6_Max_Col;
static const uint8_t SSD13x6_Max_Row = 7;

static bool SSD13x6_Init( struct SSD13x6_Device* DeviceHandle, int Width, int Height );

int  SSD13x6_GetCaps( struct SSD13x6_Device* DeviceHandle ) {
	if (DeviceHandle->Model == SH1106) return 0;
	else return CAPS_COLUMN_RANGE | CAPS_PAGE_RANGE	| CAPS_ADDRESS_VERTICAL;
}

bool SSD13x6_WriteCommand( struct SSD13x6_Device* DeviceHandle, SSDCmd SSDCommand ) {
    NullCheck( DeviceHandle->WriteCommand, return false );
    return ( DeviceHandle->WriteCommand ) ( DeviceHandle, SSDCommand );
}

bool SSD13x6_WriteData( struct SSD13x6_Device* DeviceHandle, uint8_t* Data, size_t DataLength ) {
    NullCheck( DeviceHandle->WriteData, return false );
    return ( DeviceHandle->WriteData ) ( DeviceHandle, Data, DataLength );
}

void SSD13x6_SetMuxRatio( struct SSD13x6_Device* DeviceHandle, uint8_t Ratio ) {
    SSD13x6_WriteCommand( DeviceHandle, 0xA8 );
    SSD13x6_WriteCommand( DeviceHandle, Ratio );
}

void SSD13x6_SetDisplayOffset( struct SSD13x6_Device* DeviceHandle, uint8_t Offset ) {
    SSD13x6_WriteCommand( DeviceHandle, SSDCmd_Set_Display_Offset );
    SSD13x6_WriteCommand( DeviceHandle, Offset );
}

void SSD13x6_SetDisplayStartLine( struct SSD13x6_Device* DeviceHandle, int Line ) {
    SSD13x6_WriteCommand( DeviceHandle, SSDCmd_Set_Display_Start_Line + ( uint32_t ) ( Line & 0x1F ) );
}

void SSD13x6_SetContrast( struct SSD13x6_Device* DeviceHandle, uint8_t Contrast ) {
    SSD13x6_WriteCommand( DeviceHandle, 0x81 );
    SSD13x6_WriteCommand( DeviceHandle, Contrast );
}

void SSD13x6_EnableDisplayRAM( struct SSD13x6_Device* DeviceHandle ) {
    SSD13x6_WriteCommand( DeviceHandle, 0xA4 );
}

void SSD13x6_DisableDisplayRAM( struct SSD13x6_Device* DeviceHandle ) {
    SSD13x6_WriteCommand( DeviceHandle, 0xA5 );
}

void SSD13x6_SetInverted( struct SSD13x6_Device* DeviceHandle, bool Inverted ) {
    SSD13x6_WriteCommand( DeviceHandle, Inverted ? 0xA7 : 0xA6 );
}

void SSD13x6_SetDisplayClocks( struct SSD13x6_Device* DeviceHandle, uint32_t DisplayClockDivider, uint32_t OSCFrequency ) {
    DisplayClockDivider&= 0x0F;
    OSCFrequency&= 0x0F;
    SSD13x6_WriteCommand( DeviceHandle, SSDCmd_Set_Display_CLK );
    SSD13x6_WriteCommand( DeviceHandle, ( ( OSCFrequency << 4 ) | DisplayClockDivider ) );
}

void SSD13x6_DisplayOn( struct SSD13x6_Device* DeviceHandle ) {
    SSD13x6_WriteCommand( DeviceHandle, 0xAF );
}

void SSD13x6_DisplayOff( struct SSD13x6_Device* DeviceHandle ) {
    SSD13x6_WriteCommand( DeviceHandle, 0xAE );
}

void SSD132x_ReMap( struct SSD13x6_Device* DeviceHandle ) {
	SSD13x6_WriteCommand( DeviceHandle, 0xA0 );
	SSD13x6_WriteCommand( DeviceHandle, DeviceHandle->ReMap );
}

void SSD13x6_SetDisplayAddressMode( struct SSD13x6_Device* DeviceHandle, SSD13x6_AddressMode AddressMode ) {
	switch (DeviceHandle->Model) {
	case SH1106:
		// does not exist on SH1106
		break;
	case SSD1306:
		SSD13x6_WriteCommand( DeviceHandle, 0x20 );
		SSD13x6_WriteCommand( DeviceHandle, AddressMode );
		break;
	case SSD1326:
		DeviceHandle->ReMap = (AddressMode == AddressMode_Horizontal) ? (DeviceHandle->ReMap & ~0x80) : (DeviceHandle->ReMap | 0x80);
		SSD132x_ReMap(DeviceHandle);
		break;
	}
}

void SSD13x6_Update( struct SSD13x6_Device* DeviceHandle ) {
	if (DeviceHandle->Model == SH1106) {
		// SH1106 requires a page-by-page update and ahs no end Page/Column
		for (int i = 0; i < DeviceHandle->Height / 8 ; i++) {
			SSD13x6_SetPageAddress( DeviceHandle, i, 0);
			SSD13x6_SetColumnAddress( DeviceHandle, 0, 0);			
			SSD13x6_WriteData( DeviceHandle, DeviceHandle->Framebuffer + i*DeviceHandle->Width, DeviceHandle->Width );
		}	
	} else {	
		// others have an automatic counter and end Page/Column
		SSD13x6_SetColumnAddress( DeviceHandle, 0, DeviceHandle->Width - 1);
		SSD13x6_SetPageAddress( DeviceHandle, 0, DeviceHandle->Height / 8 - 1);
		SSD13x6_WriteData( DeviceHandle, DeviceHandle->Framebuffer, DeviceHandle->FramebufferSize );
	}	
}

void SSD13x6_WriteRawData( struct SSD13x6_Device* DeviceHandle, uint8_t* Data, size_t DataLength ) {
    NullCheck( Data, return );
    DataLength = DataLength > DeviceHandle->FramebufferSize ? DeviceHandle->FramebufferSize : DataLength;
    if ( DataLength > 0 ) SSD13x6_WriteData( DeviceHandle, Data, DataLength );
}

void SSD13x6_SetHFlip( struct SSD13x6_Device* DeviceHandle, bool On ) {
	switch (DeviceHandle->Model) {
	case SH1106:		
	case SSD1306:	
		SSD13x6_WriteCommand( DeviceHandle, On ? 0xA1 : 0xA0 );
		break;
	case SSD1326:
		DeviceHandle->ReMap = On ? (DeviceHandle->ReMap | 0x01) : (DeviceHandle->ReMap & ~0x01);
		SSD132x_ReMap(DeviceHandle);
		break;
	}	
}

void SSD13x6_SetVFlip( struct SSD13x6_Device* DeviceHandle, bool On ) {
	switch (DeviceHandle->Model) {
	case SH1106:	
	case SSD1306:	
		SSD13x6_WriteCommand( DeviceHandle, On ? 0xC8 : 0xC0 );
		break;
	case SSD1326:
		DeviceHandle->ReMap = On ? (DeviceHandle->ReMap | 0x05) : (DeviceHandle->ReMap & ~0x05);
		SSD132x_ReMap( DeviceHandle );
		break;
	}	
}

void SSD13x6_SetColumnAddress( struct SSD13x6_Device* DeviceHandle, uint8_t Start, uint8_t End ) {
    CheckBounds( Start > SSD13x6_Max_Col, return );
    CheckBounds( End > SSD13x6_Max_Col, return );
	
	// on SH1106, there is no "end column"
	if (DeviceHandle->Model == SH1106) {
		// well, unfortunately this driver is 132 colums but most displays are 128...
		if (DeviceHandle->Width != 132) Start += 2;
		SSD13x6_WriteCommand( DeviceHandle, 0x10 | (Start >> 4) );
		SSD13x6_WriteCommand( DeviceHandle, 0x00 | (Start & 0x0f) );
	} else {	
		SSD13x6_WriteCommand( DeviceHandle, SSDCmd_Set_Column_Address );
		SSD13x6_WriteCommand( DeviceHandle, Start );
		SSD13x6_WriteCommand( DeviceHandle, End );
	}	
}

void SSD13x6_SetPageAddress( struct SSD13x6_Device* DeviceHandle, uint8_t Start, uint8_t End ) {
    NullCheck( DeviceHandle, return );

    CheckBounds( Start > SSD13x6_Max_Row, return );
    CheckBounds( End > SSD13x6_Max_Row, return );
	
	// on SH1106, there is no "end page"
	if (DeviceHandle->Model == SH1106) {
			SSD13x6_WriteCommand( DeviceHandle, 0xB0 | Start );			
	} else {	
		// in case of SSD1326, this is sub-optimal as it can address by line, not by page
		if (DeviceHandle->Model != SSD1306) {
			Start *= 8;
			End = (End + 1) * 8 - 1;
		}
	
		SSD13x6_WriteCommand( DeviceHandle, SSDCmd_Set_Page_Address );	
		SSD13x6_WriteCommand( DeviceHandle, Start );
		SSD13x6_WriteCommand( DeviceHandle, End );
	}	
}

bool SSD13x6_HWReset( struct SSD13x6_Device* DeviceHandle ) {
    NullCheck( DeviceHandle, return 0 );

    if ( DeviceHandle->Reset != NULL ) {
        return ( DeviceHandle->Reset ) ( DeviceHandle );
    }

    /* This should always return true if there is no reset callback as
     * no error would have occurred during the non existant reset.
     */
    return true;
}

static bool SSD13x6_Init( struct SSD13x6_Device* DeviceHandle, int Width, int Height ) {
    DeviceHandle->Width = Width;
    DeviceHandle->Height = Height;
	
	SSD13x6_HWReset( DeviceHandle );
	SSD13x6_DisplayOff( DeviceHandle );

	if (DeviceHandle->Model == SSD1306 || DeviceHandle->Model == SH1106) {
		SSDCmd_Set_Display_Start_Line = 0x40;
		SSDCmd_Set_Display_Offset = 0xD3;
		SSDCmd_Set_Column_Address = 0x21,
		SSDCmd_Set_Display_CLK = 0xD5;
		SSDCmd_Set_Page_Address = 0x22;		
		SSD13x6_Max_Col = 127;
		
		if (DeviceHandle->Model == SSD1306) {
			// charge pump regulator, do direct init
			SSD13x6_WriteCommand( DeviceHandle, 0x8D );
			SSD13x6_WriteCommand( DeviceHandle, 0x14 ); 
			
			// COM pins HW config (alternative:EN, remap:DIS) - some display might need something difference
			SSD13x6_WriteCommand( DeviceHandle, 0xDA );
			SSD13x6_WriteCommand( DeviceHandle, (1 << 4) | (0 < 5) );

		} else {
			// charge pump regulator, do direct init
			SSD13x6_WriteCommand( DeviceHandle, 0xAD );
			SSD13x6_WriteCommand( DeviceHandle, 0x8B ); 
			
			// COM pins HW config (alternative:EN) - some display might need something difference
			SSD13x6_WriteCommand( DeviceHandle, 0xDA );
			SSD13x6_WriteCommand( DeviceHandle, 1 << 4);
		}

	} else if (DeviceHandle->Model == SSD1326) {
		SSDCmd_Set_Display_Start_Line = 0xA1;
		SSDCmd_Set_Display_Offset = 0xA2;
		SSDCmd_Set_Column_Address = 0x15;
		SSDCmd_Set_Display_CLK = 0xB3;
		SSDCmd_Set_Page_Address = 0x75;		// not really a page but a row
		
		SSD13x6_Max_Col = 255;
		
		// no gray scale
		DeviceHandle->ReMap |= 0x10;
		SSD132x_ReMap( DeviceHandle );
		
		SSD13x6_SetHFlip( DeviceHandle, false );
		SSD13x6_SetVFlip( DeviceHandle, false );
	}

	SSD13x6_SetMuxRatio( DeviceHandle, Height - 1 );
    SSD13x6_SetDisplayOffset( DeviceHandle, 0x00 );
	SSD13x6_SetDisplayStartLine( DeviceHandle, 0 );
	SSD13x6_SetContrast( DeviceHandle, 0x7F );
    SSD13x6_DisableDisplayRAM( DeviceHandle );
	SSD13x6_SetVFlip( DeviceHandle, false );
	SSD13x6_SetHFlip( DeviceHandle, false );
	SSD13x6_SetInverted( DeviceHandle, false );
    SSD13x6_SetDisplayClocks( DeviceHandle, 0, 8 );
	SSD13x6_SetDisplayAddressMode( DeviceHandle, AddressMode_Horizontal );
    SSD13x6_SetColumnAddress( DeviceHandle, 0, DeviceHandle->Width - 1 );
    SSD13x6_SetPageAddress( DeviceHandle, 0, ( DeviceHandle->Height / 8 ) - 1 );
	SSD13x6_EnableDisplayRAM( DeviceHandle );
    SSD13x6_DisplayOn( DeviceHandle );
	SSD13x6_Update( DeviceHandle );

    return true;
}

bool SSD13x6_Init_I2C( struct SSD13x6_Device* DeviceHandle, int Width, int Height, int I2CAddress, int ResetPin, WriteCommandProc WriteCommand, WriteDataProc WriteData, ResetProc Reset ) {
    NullCheck( DeviceHandle, return false );
    NullCheck( WriteCommand, return false );
    NullCheck( WriteData, return false );

    DeviceHandle->WriteCommand = WriteCommand;
    DeviceHandle->WriteData = WriteData;
    DeviceHandle->Reset = Reset;
    DeviceHandle->Address = I2CAddress;
    DeviceHandle->RSTPin = ResetPin;
	
	DeviceHandle->FramebufferSize = ( Width * Height ) / 8;
	DeviceHandle->Framebuffer = calloc( 1, DeviceHandle->FramebufferSize );
    NullCheck( DeviceHandle->Framebuffer, return false );
    
    return SSD13x6_Init( DeviceHandle, Width, Height );
}

bool SSD13x6_Init_SPI( struct SSD13x6_Device* DeviceHandle, int Width, int Height, int ResetPin, int CSPin, spi_device_handle_t SPIHandle, WriteCommandProc WriteCommand, WriteDataProc WriteData, ResetProc Reset ) {
    NullCheck( DeviceHandle, return false );
    NullCheck( WriteCommand, return false );
    NullCheck( WriteData, return false );

    DeviceHandle->WriteCommand = WriteCommand;
    DeviceHandle->WriteData = WriteData;
    DeviceHandle->Reset = Reset;
    DeviceHandle->SPIHandle = SPIHandle;
    DeviceHandle->RSTPin = ResetPin;
    DeviceHandle->CSPin = CSPin;
	
	DeviceHandle->FramebufferSize = ( Width * Height ) / 8;
    DeviceHandle->Framebuffer = heap_caps_calloc( 1, DeviceHandle->FramebufferSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA );
    NullCheck( DeviceHandle->Framebuffer, return false );
	
    return SSD13x6_Init( DeviceHandle, Width, Height );
}
