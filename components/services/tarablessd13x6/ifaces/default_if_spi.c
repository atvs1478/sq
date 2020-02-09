
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
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <freertos/task.h>
#include "ssd13x6.h"
#include "ssd13x6_default_if.h"

static const int SSD13x6_SPI_Command_Mode = 0;
static const int SSD13x6_SPI_Data_Mode = 1;

static spi_host_device_t SPIHost;
static int DCPin;

static bool SPIDefaultWriteBytes( spi_device_handle_t SPIHandle, int WriteMode, const uint8_t* Data, size_t DataLength );
static bool SPIDefaultWriteCommand( struct SSD13x6_Device* DeviceHandle, SSDCmd Command );
static bool SPIDefaultWriteData( struct SSD13x6_Device* DeviceHandle, const uint8_t* Data, size_t DataLength );
static bool SPIDefaultReset( struct SSD13x6_Device* DeviceHandle );

bool SSD13x6_SPIMasterInitDefault( int SPI, int DC ) {
	SPIHost = SPI;
	DCPin = DC;
    return true;
}

bool SSD13x6_SPIMasterAttachDisplayDefault( struct SSD13x6_Device* DeviceHandle, int Model, int Width, int Height, int CSPin, int RSTPin ) {
    spi_device_interface_config_t SPIDeviceConfig;
    spi_device_handle_t SPIDeviceHandle;

    NullCheck( DeviceHandle, return false );
	
	if (CSPin >= 0) {
		ESP_ERROR_CHECK_NONFATAL( gpio_set_direction( CSPin, GPIO_MODE_OUTPUT ), return false );
		ESP_ERROR_CHECK_NONFATAL( gpio_set_level( CSPin, 0 ), return false );
	}	

    memset( &SPIDeviceConfig, 0, sizeof( spi_device_interface_config_t ) );

    SPIDeviceConfig.clock_speed_hz = SPI_MASTER_FREQ_8M;
    SPIDeviceConfig.spics_io_num = CSPin;
    SPIDeviceConfig.queue_size = 1;

    if ( RSTPin >= 0 ) {
        ESP_ERROR_CHECK_NONFATAL( gpio_set_direction( RSTPin, GPIO_MODE_OUTPUT ), return false );
        ESP_ERROR_CHECK_NONFATAL( gpio_set_level( RSTPin, 0 ), return false );
    }

    ESP_ERROR_CHECK_NONFATAL( spi_bus_add_device( SPIHost, &SPIDeviceConfig, &SPIDeviceHandle ), return false );
	
	DeviceHandle->Model = Model;
	
    return SSD13x6_Init_SPI( DeviceHandle,
        Width,
        Height,
        RSTPin,
        CSPin,
        SPIDeviceHandle,
        SPIDefaultWriteCommand,
        SPIDefaultWriteData,
        SPIDefaultReset
    );
}

static bool SPIDefaultWriteBytes( spi_device_handle_t SPIHandle, int WriteMode, const uint8_t* Data, size_t DataLength ) {
    spi_transaction_t SPITransaction;

    NullCheck( SPIHandle, return false );
    NullCheck( Data, return false );

    if ( DataLength > 0 ) {
        memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );

        SPITransaction.length = DataLength * 8;
        SPITransaction.tx_buffer = Data;

        gpio_set_level( DCPin, WriteMode );
        ESP_ERROR_CHECK_NONFATAL( spi_device_transmit( SPIHandle, &SPITransaction ), return false );
    }

    return true;
}

static bool SPIDefaultWriteCommand( struct SSD13x6_Device* DeviceHandle, SSDCmd Command ) {
    static uint8_t CommandByte = 0;

    NullCheck( DeviceHandle, return false );
    NullCheck( DeviceHandle->SPIHandle, return false );

    CommandByte = Command;

    return SPIDefaultWriteBytes( DeviceHandle->SPIHandle, SSD13x6_SPI_Command_Mode, &CommandByte, 1 );
}

static bool SPIDefaultWriteData( struct SSD13x6_Device* DeviceHandle, const uint8_t* Data, size_t DataLength ) {
    NullCheck( DeviceHandle, return false );
    NullCheck( DeviceHandle->SPIHandle, return false );

    return SPIDefaultWriteBytes( DeviceHandle->SPIHandle, SSD13x6_SPI_Data_Mode, Data, DataLength );
}

static bool SPIDefaultReset( struct SSD13x6_Device* DeviceHandle ) {
    NullCheck( DeviceHandle, return false );

    if ( DeviceHandle->RSTPin >= 0 ) {
        ESP_ERROR_CHECK_NONFATAL( gpio_set_level( DeviceHandle->RSTPin, 0 ), return false );
            vTaskDelay( pdMS_TO_TICKS( 100 ) );
        ESP_ERROR_CHECK_NONFATAL( gpio_set_level( DeviceHandle->RSTPin, 1 ), return false );
    }

    return true;
}

