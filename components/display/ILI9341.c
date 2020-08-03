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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include <esp_heap_caps.h>
#include <esp_log.h>

#include "gds.h"
#include "gds_private.h"

//#define SHADOW_BUFFER
#define PAGE_BLOCK	1024

#define min(a,b) (((a) < (b)) ? (a) : (b))

static char TAG[] = "ILI9341";


#define L1_CMD_NOP 0X00
#define L1_CMD_SOFTWARE_RESET 0X01
#define L1_CMD_READ_DISPLAY_IDENTIFICATION_INFORMATION 0X04
#define L1_CMD_READ_DISPLAY_STATUS 0X09
#define L1_CMD_READ_DISPLAY_POWER_MODE 0X0A
#define L1_CMD_READ_DISPLAY_MADCTL 0X0B
#define L1_CMD_READ_DISPLAY_PIXEL_FORMAT 0X0C
#define L1_CMD_READ_DISPLAY_IMAGE_FORMAT 0X0D
#define L1_CMD_READ_DISPLAY_SIGNAL_MODE 0X0E
#define L1_CMD_READ_DISPLAY_SELF_DIAGNOSTIC_RESULT 0X0F
#define L1_CMD_ENTER_SLEEP_MODE 0X10
#define L1_CMD_SLEEP_OUT 0X11
#define L1_CMD_PARTIAL_MODE_ON 0X12
#define L1_CMD_NORMAL_DISPLAY_MODE_ON 0X13
#define L1_CMD_DISPLAY_INVERSION_OFF 0X20
#define L1_CMD_DISPLAY_INVERSION_ON 0X21
#define L1_CMD_GAMMA_SET 0X26
#define L1_CMD_DISPLAY_OFF 0X28
#define L1_CMD_DISPLAY_ON 0X29
#define L1_CMD_COLUMN_ADDRESS_SET 0X2A
#define L1_CMD_PAGE_ADDRESS_SET 0X2B
#define L1_CMD_MEMORY_WRITE 0X2C
#define L1_CMD_COLOR_SET 0X2D
#define L1_CMD_MEMORY_READ 0X2E
#define L1_CMD_PARTIAL_AREA 0X30
#define L1_CMD_VERTICAL_SCROLLING_DEFINITION 0X33
#define L1_CMD_TEARING_EFFECT_LINE_OFF 0X34
#define L1_CMD_TEARING_EFFECT_LINE_ON 0X35
#define L1_CMD_MEMORY_ACCESS_CONTROL 0X36
#define L1_CMD_VERTICAL_SCROLLING_START_ADDRESS 0X37
#define L1_CMD_IDLE_MODE_OFF 0X38
#define L1_CMD_IDLE_MODE_ON 0X39
#define L1_CMD_COLMOD_PIXEL_FORMAT_SET 0X3A
#define L1_CMD_WRITE_MEMORY_CONTINUE 0X3C
#define L1_CMD_READ_MEMORY_CONTINUE 0X3E
#define L1_CMD_SET_TEAR_SCANLINE 0X44
#define L1_CMD_GET_SCANLINE 0X45
#define L1_CMD_WRITE_DISPLAY_BRIGHTNESS 0X51
#define L1_CMD_READ_DISPLAY_BRIGHTNESS 0X52
#define L1_CMD_WRITE_CTRL_DISPLAY 0X53
#define L1_CMD_READ_CTRL_DISPLAY 0X54
#define L1_CMD_WRITE_CONTENT_ADAPTIVE_BRIGHTNESS_CONTROL 0X55
#define L1_CMD_READ_CONTENT_ADAPTIVE_BRIGHTNESS_CONTROL 0X56
#define L1_CMD_WRITE_CABC_MINIMUM_BRIGHTNESS 0X5E
#define L1_CMD_READ_CABC_MINIMUM_BRIGHTNESS 0X5F
#define L1_CMD_READ_ID1 0XDA
#define L1_CMD_READ_ID2 0XDB
#define L1_CMD_READ_ID3 0XDC

#define L2_CMD_RGB_INTERFACE_SIGNAL_CONTROL 0XB0
#define L2_CMD_FRAME_RATE_CONTROL_IN_NORMAL_MODE_FULL_COLORS 0XB1
#define L2_CMD_FRAME_RATE_CONTROL_IN_IDLE_MODE_8_COLORS 0XB2
#define L2_CMD_FRAME_RATE_CONTROL_IN_PARTIAL_MODE_FULL_COLORS 0XB3
#define L2_CMD_DISPLAY_INVERSION_CONTROL 0XB4
#define L2_CMD_BLANKING_PORCH_CONTROL 0XB5
#define L2_CMD_DISPLAY_FUNCTION_CONTROL 0XB6
#define L2_CMD_ENTRY_MODE_SET 0XB7
#define L2_CMD_BACKLIGHT_CONTROL_1 0XB8
#define L2_CMD_BACKLIGHT_CONTROL_2 0XB9
#define L2_CMD_BACKLIGHT_CONTROL_3 0XBA
#define L2_CMD_BACKLIGHT_CONTROL_4 0XBB
#define L2_CMD_BACKLIGHT_CONTROL_5 0XBC
#define L2_CMD_BACKLIGHT_CONTROL_7 0XBE
#define L2_CMD_BACKLIGHT_CONTROL_8 0XBF
#define L2_CMD_POWER_CONTROL_1 0XC0
#define L2_CMD_POWER_CONTROL_2 0XC1
#define L2_CMD_VCOM_CONTROL_1 0XC5
#define L2_CMD_VCOM_CONTROL_2 0XC7
#define L2_CMD_NV_MEMORY_WRITE 0XD0
#define L2_CMD_NV_MEMORY_PROTECTION_KEY 0XD1
#define L2_CMD_NV_MEMORY_STATUS_READ 0XD2
#define L2_CMD_READ_ID4 0XD3
#define L2_CMD_POSITIVE_GAMMA_CORRECTION 0XE0
#define L2_CMD_NEGATIVE_GAMMA_CORRECTION 0XE1
#define L2_CMD_DIGITAL_GAMMA_CONTROL_1 0XE2
#define L2_CMD_DIGITAL_GAMMA_CONTROL_2 0XE3
#define L2_CMD_INTERFACE_CONTROL 0XF6



/*
 The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct.
*/
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;



static const lcd_init_cmd_t ili_init_cmds[]={
    /* Power contorl B, power control = 0, DC_ENA = 1 */
    {0xCF, {0x00, 0x83, 0X30}, 3},
    /* Power on sequence control,
     * cp1 keeps 1 frame, 1st frame enable
     * vcl = 0, ddvdh=3, vgh=1, vgl=2
     * DDVDH_ENH=1
     */
    {0xED, {0x64, 0x03, 0X12, 0X81}, 4},
    /* Driver timing control A,
     * non-overlap=default +1
     * EQ=default - 1, CR=default
     * pre-charge=default - 1
     */
    {0xE8, {0x85, 0x01, 0x79}, 3},
    /* Power control A, Vcore=1.6V, DDVDH=5.6V */
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    /* Pump ratio control, DDVDH=2xVCl */
    {0xF7, {0x20}, 1},
    /* Driver timing control, all=0 unit */
    {0xEA, {0x00, 0x00}, 2},
    /* Power control 1, GVDD=4.75V */
    {0xC0, {0x26}, 1},
    /* Power control 2, DDVDH=VCl*2, VGH=VCl*7, VGL=-VCl*3 */
    {0xC1, {0x11}, 1},
    /* VCOM control 1, VCOMH=4.025V, VCOML=-0.950V */
    {0xC5, {0x35, 0x3E}, 2},
    /* VCOM control 2, VCOMH=VMH-2, VCOML=VML-2 */
    {0xC7, {0xBE}, 1},
    /* Memory access contorl, MX=MY=0, MV=1, ML=0, BGR=1, MH=0 */
    {0x36, {0x28}, 1},
    /* Pixel format, 16bits/pixel for RGB/MCU interface */
    {0x3A, {0x55}, 1},
    /* Frame rate control, f=fosc, 70Hz fps */
    {0xB1, {0x00, 0x1B}, 2},
    /* Enable 3G, disabled */
    {0xF2, {0x08}, 1},
    /* Gamma set, curve 1 */
    {0x26, {0x01}, 1},
    /* Positive gamma correction */
    {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
    /* Negative gamma correction */
    {0XE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
    /* Column address set, SC=0, EC=0xEF */
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    /* Page address set, SP=0, EP=0x013F */
    {0x2B, {0x00, 0x00, 0x01, 0x3f}, 4},
    /* Memory write */
    {0x2C, {0}, 0},
    /* Entry mode set, Low vol detect disabled, normal display */
    {0xB7, {0x07}, 1},
    /* Display function control */
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
    /* Sleep out */
    {0x11, {0}, 0x80},
    /* Display on */
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};

//To speed up transfers, every SPI transfer sends a bunch of lines. This define specifies how many. More means more memory use,
//but less overhead for setting up / finishing transfers. Make sure 240 is dividable by this.
#define PARALLEL_LINES 16

struct PrivateSpace {
	uint8_t *iRAM, *Shadowbuffer;
	uint8_t ReMap, PageSize;
	uint8_t Offset;
};

// Functions are not declared to minimize # of lines

static void WriteDataByte( struct GDS_Device* Device, uint8_t Data ) {
	Device->WriteData( Device, &Data, 1);
}

static void SetColumnAddress( struct GDS_Device* Device, uint8_t Start, uint8_t End ) {
	Device->WriteCommand( Device, L1_CMD_COLUMN_ADDRESS_SET );
	Device->WriteData( Device, &Start, 1 );
	Device->WriteData( Device, &End, 1 );
}
static void SetRowAddress( struct GDS_Device* Device, uint8_t Start, uint8_t End ) {
	Device->WriteCommand( Device, L1_CMD_PAGE_ADDRESS_SET );
	Device->WriteData( Device, &Start, 1 );
	Device->WriteData( Device, &End, 1 );
}



static void Update( struct GDS_Device* Device ) {
	struct PrivateSpace *Private = (struct PrivateSpace*) Device->Private;
		
	//SetColumnAddress( Device, Private->Offset, Private->Offset + Device->Width / 4 - 1);
	SetColumnAddress( Device, Private->Offset, Private->Offset + Device->Width - 1);
	
#ifdef SHADOW_BUFFER
	uint16_t *optr = (uint16_t*) Private->Shadowbuffer, *iptr = (uint16_t*) Device->Framebuffer;
	bool dirty = false;
	
	for (int r = 0, page = 0; r < Device->Height; r++) {
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
				uint16_t *optr = (uint16_t*) Private->iRAM, *iptr = (uint16_t*) (Private->Shadowbuffer + (r - page + 1) * Device->Width / 2);
				SetRowAddress( Device, r - page + 1, r );
				for (int i = page * Device->Width / 2 / 2; --i >= 0; iptr++) *optr++ = (*iptr >> 8) | (*iptr << 8);
				//memcpy(Private->iRAM, Private->Shadowbuffer + (r - page + 1) * Device->Width / 2, page * Device->Width / 2 );
				Device->WriteCommand( Device, 0x5c );
				Device->WriteData( Device, Private->iRAM, Device->Width * page / 2 );
				dirty = false;
			}	
			page = 0;
		}	
	}	
#else
	for (int r = 0; r < Device->Height; r += Private->PageSize) {
		SetRowAddress( Device, r, r + Private->PageSize - 1 );
		Device->WriteCommand( Device, L1_CMD_MEMORY_WRITE );
		if (Private->iRAM) {
			uint16_t *optr = (uint16_t*) Private->iRAM, *iptr = (uint16_t*) (Device->Framebuffer + r * Device->Width / 2);
			for (int i = Private->PageSize * Device->Width / 2 / 2; --i >= 0; iptr++) *optr++ = (*iptr >> 8) | (*iptr << 8);
			//memcpy(Private->iRAM, Device->Framebuffer + r * Device->Width / 2, Private->PageSize * Device->Width / 2 );
			Device->WriteData( Device, Private->iRAM, Private->PageSize * Device->Width / 2 );
		} else	{
			Device->WriteData( Device, Device->Framebuffer + r * Device->Width / 2, Private->PageSize * Device->Width / 2 );
		}	
	}	
#endif	
}

//Bit 	Name 						Description
//---	---------------------------	------------------------------------------------------
//MY  	Row Address Order 	 		MCU to memory write/read direction.
//MX 	Column Address Order 		MCU to memory write/read direction.
//MV 	Row / Column Exchange 		MCU to memory write/read direction.
//ML 	Vertical Refresh Order 		LCD vertical refresh direction control.
//BGR 	RGB-BGR Order 				Color selector switch control
//									(0=RGB color filter panel, 1=BGR color filter panel)
//MH 	Horizontal Refresh ORDER 	LCD horizontal refreshing direction control.
// Bits 17-0
//		XX XX XX XX XX XX XX XX XX XX MY MX MV ML BGR MH 0 0
typedef enum  {
	MAC_BIT_MH=2,
	MAC_BIT_BGR,
	MAC_BIT_ML,
	MAC_BIT_MV,
	MAC_BIT_MX,
	MAC_BIT_MY,
} mac_bits;

uint16_t set_mac_bit(mac_bits bit, uint16_t val){
	return (1 << bit) | val;
}
uint16_t unset_mac_bit(mac_bits bit, uint16_t val){
	return ~(1 << bit) & val;
}

static void SetLayout( struct GDS_Device* Device, bool HFlip, bool VFlip, bool Rotate ) { 
	struct PrivateSpace *Private = (struct PrivateSpace*) Device->Private;
	Private->ReMap = HFlip ? (Private->ReMap & ~(1 << MAC_BIT_MX)) : (Private->ReMap | (1 << MAC_BIT_MX));
	Private->ReMap = VFlip ? (Private->ReMap | (1 << MAC_BIT_MY)) : (Private->ReMap & ~(1 << MAC_BIT_MY));
	Device->WriteCommand( Device, L1_CMD_MEMORY_ACCESS_CONTROL );
	Device->WriteData( Device, &Private->ReMap, 1 );
	WriteDataByte(Device,0x00);
}	

static void DisplayOn( struct GDS_Device* Device ) { Device->WriteCommand( Device, L1_CMD_DISPLAY_ON ); }
static void DisplayOff( struct GDS_Device* Device ) { Device->WriteCommand( Device, L1_CMD_DISPLAY_OFF ); }

static void SetContrast( struct GDS_Device* Device, uint8_t Contrast ) {
    Device->WriteCommand( Device, L1_CMD_WRITE_DISPLAY_BRIGHTNESS );
    uint8_t loc_contrast =  (uint8_t)((float)Contrast/5.0f*  255.0f);
    Device->WriteData( Device, &loc_contrast , 1 );
    WriteDataByte(Device,0x00);
}

static bool Init( struct GDS_Device* Device ) {
	struct PrivateSpace *Private = (struct PrivateSpace*) Device->Private;
	

//	Private->Offset = (480 - Device->Width) / 4 / 2;
	
	// find a page size that is not too small is an integer of height
	Private->PageSize = min(8, PAGE_BLOCK / (Device->Width / 2));
	Private->PageSize = Device->Height / (Device->Height / Private->PageSize) ;
	
#ifdef SHADOW_BUFFER	
//	Private->Shadowbuffer = malloc( Device->FramebufferSize );
//	memset(Private->Shadowbuffer, 0xFF, Device->FramebufferSize);
#endif
	Private->iRAM =NULL;
	//Private->iRAM =heap_caps_malloc(320*PARALLEL_LINES*sizeof(uint16_t), MALLOC_CAP_DMA);


	//ESP_LOGI(TAG, "ILI9341 with offset %u, page %u, iRAM %p", Private->Offset, Private->PageSize, Private->iRAM);
	ESP_LOGI(TAG, "ILI9341 ");

	// need to be off and disable display RAM
	Device->DisplayOff( Device );
	int cmd=0;
	//Send all the commands
	while (ili_init_cmds[cmd].databytes!=0xff) {
		Device->WriteCommand( Device, ili_init_cmds[cmd].cmd );
		Device->WriteData(Device,ili_init_cmds[cmd].data,ili_init_cmds[cmd].databytes&0x1F);
		if (ili_init_cmds[cmd].databytes&0x80) {
			vTaskDelay(100 / portTICK_RATE_MS);
		}
		cmd++;
	}
	
	// gone with the wind
	Device->DisplayOn( Device );
	Device->Update( Device );
	
	return true;
}	

static const struct GDS_Device ILI9341 = {
	.DisplayOn = DisplayOn, .DisplayOff = DisplayOff, .SetContrast = SetContrast,
	.SetLayout = SetLayout,
	.Update = Update, .Init = Init,
};	

struct GDS_Device* ILI9341_Detect(char *Driver, struct GDS_Device* Device) {
	if (!strcasestr(Driver, "ILI9341")) return NULL;
		
	if (!Device) Device = calloc(1, sizeof(struct GDS_Device));
	
	*Device = ILI9341;
	Device->Depth = 4;
		
	return Device;
}


