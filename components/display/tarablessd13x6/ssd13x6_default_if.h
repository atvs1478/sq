#ifndef _SSD13x6_DEFAULT_IF_H_
#define _SSD13x6_DEFAULT_IF_H_

#ifdef __cplusplus
extern "C" {
#endif

bool SSD13x6_I2CMasterInitDefault( int PortNumber, int SDA, int SCL );
bool SSD13x6_I2CMasterAttachDisplayDefault( struct SSD13x6_Device* DisplayHandle, int Model, int Width, int Height, int I2CAddress, int RSTPin );

bool SSD13x6_SPIMasterInitDefault( int SPI, int DC);
bool SSD13x6_SPIMasterAttachDisplayDefault( struct SSD13x6_Device* DeviceHandle, int Model, int Width, int Height, int CSPin, int RSTPin, int Speed );

#ifdef __cplusplus
}
#endif

#endif
