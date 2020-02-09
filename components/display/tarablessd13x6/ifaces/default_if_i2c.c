/**
 * Copyright (c) 2017-2018 Tara Keeling
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <driver/i2c.h>
#include <driver/gpio.h>
#include "ssd13x6.h"
#include "ssd13x6_default_if.h"

static int I2CPortNumber;

static const int SSD13x6_I2C_COMMAND_MODE = 0x80;
static const int SSD13x6_I2C_DATA_MODE = 0x40;

static bool I2CDefaultWriteBytes( int Address, bool IsCommand, const uint8_t* Data, size_t DataLength );
static bool I2CDefaultWriteCommand( struct SSD13x6_Device* Display, SSDCmd Command );
static bool I2CDefaultWriteData( struct SSD13x6_Device* Display, const uint8_t* Data, size_t DataLength );
static bool I2CDefaultReset( struct SSD13x6_Device* Display );

/*
 * Initializes the i2c master with the parameters specified
 * in the component configuration in sdkconfig.h.
 * 
 * Returns true on successful init of the i2c bus.
 */
bool SSD13x6_I2CMasterInitDefault( int PortNumber, int SDA, int SCL ) {
	I2CPortNumber = PortNumber;
	
	if (SDA != -1 && SCL != -1) {
		i2c_config_t Config = { 0 };

        Config.mode = I2C_MODE_MASTER;
		Config.sda_io_num = SDA;
		Config.sda_pullup_en = GPIO_PULLUP_ENABLE;
		Config.scl_io_num = SCL;
		Config.scl_pullup_en = GPIO_PULLUP_ENABLE;
		Config.master.clk_speed = 250000;

		ESP_ERROR_CHECK_NONFATAL( i2c_param_config( I2CPortNumber, &Config ), return false );
		ESP_ERROR_CHECK_NONFATAL( i2c_driver_install( I2CPortNumber, Config.mode, 0, 0, 0 ), return false );
	}	

    return true;
}

/*
 * Attaches a display to the I2C bus using default communication functions.
 * 
 * Params:
 * DisplayHandle: Pointer to your SSD13x6_Device object
 * Width: Width of display
 * Height: Height of display
 * I2CAddress: Address of your display
 * RSTPin: Optional GPIO pin to use for hardware reset, if none pass -1 for this parameter.
 * 
 * Returns true on successful init of display.
 */
bool SSD13x6_I2CMasterAttachDisplayDefault( struct SSD13x6_Device* DeviceHandle, int Model, int Width, int Height, int I2CAddress, int RSTPin ) {
    NullCheck( DeviceHandle, return false );

    if ( RSTPin >= 0 ) {
        ESP_ERROR_CHECK_NONFATAL( gpio_set_direction( RSTPin, GPIO_MODE_OUTPUT ), return false );
        ESP_ERROR_CHECK_NONFATAL( gpio_set_level( RSTPin, 1 ), return false );
    }
	
	DeviceHandle->Model = Model;
	
    return SSD13x6_Init_I2C( DeviceHandle,
        Width,
        Height,
        I2CAddress,
        RSTPin,
        I2CDefaultWriteCommand,
        I2CDefaultWriteData,
        I2CDefaultReset
    );
}

static bool I2CDefaultWriteBytes( int Address, bool IsCommand, const uint8_t* Data, size_t DataLength ) {
    i2c_cmd_handle_t* CommandHandle = NULL;
    static uint8_t ModeByte = 0;

    NullCheck( Data, return false );

    if ( ( CommandHandle = i2c_cmd_link_create( ) ) != NULL ) {
        ModeByte = ( IsCommand == true ) ? SSD13x6_I2C_COMMAND_MODE: SSD13x6_I2C_DATA_MODE;

        ESP_ERROR_CHECK_NONFATAL( i2c_master_start( CommandHandle ), goto error );
        ESP_ERROR_CHECK_NONFATAL( i2c_master_write_byte( CommandHandle, ( Address << 1 ) | I2C_MASTER_WRITE, true ), goto error );
        ESP_ERROR_CHECK_NONFATAL( i2c_master_write_byte( CommandHandle, ModeByte, true ), goto error );
        ESP_ERROR_CHECK_NONFATAL( i2c_master_write( CommandHandle, ( uint8_t* ) Data, DataLength, true ), goto error );
        ESP_ERROR_CHECK_NONFATAL( i2c_master_stop( CommandHandle ), goto error );

        ESP_ERROR_CHECK_NONFATAL( i2c_master_cmd_begin( I2CPortNumber, CommandHandle, pdMS_TO_TICKS( 1000 ) ), goto error );
        i2c_cmd_link_delete( CommandHandle );
    }

    return true;
	
error:
	if (CommandHandle) i2c_cmd_link_delete( CommandHandle );
	return false;
}

static bool I2CDefaultWriteCommand( struct SSD13x6_Device* Display, SSDCmd Command ) {
    uint8_t CommandByte = ( uint8_t ) Command;

    NullCheck( Display, return false );
    return I2CDefaultWriteBytes( Display->Address, true, ( const uint8_t* ) &CommandByte, 1 );
}

static bool I2CDefaultWriteData( struct SSD13x6_Device* Display, const uint8_t* Data, size_t DataLength ) {
    NullCheck( Display, return false );
    NullCheck( Data, return false );

    return I2CDefaultWriteBytes( Display->Address, false, Data, DataLength );
}

static bool I2CDefaultReset( struct SSD13x6_Device* Display ) {
    NullCheck( Display, return false );

    if ( Display->RSTPin >= 0 ) {
        ESP_ERROR_CHECK_NONFATAL( gpio_set_level( Display->RSTPin, 0 ), return true );
            vTaskDelay( pdMS_TO_TICKS( 100 ) );
        ESP_ERROR_CHECK_NONFATAL( gpio_set_level( Display->RSTPin, 1 ), return true );
    }

    return true;
}

