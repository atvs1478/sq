#ifndef _SSD13x6_FONT_H_
#define _SSD13x6_FONT_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SSD13x6_Device;

/* 
 * X-GLCD Font format:
 *
 * First byte of glyph is it's width in pixels.
 * Each data byte represents 8 pixels going down from top to bottom.
 * 
 * Example glyph layout for a 16x16 font
 * 'a': [Glyph width][Pixel column 0][Pixel column 1] where the number of pixel columns is the font height divided by 8
 * 'b': [Glyph width][Pixel column 0][Pixel column 1]...
 * 'c': And so on...
 */

struct SSD13x6_FontDef {
    const uint8_t* FontData;

    int Width;
    int Height;

    int StartChar;
    int EndChar;

    bool Monospace;
};

typedef enum {
    TextAnchor_East = 0,
    TextAnchor_West,
    TextAnchor_North,
    TextAnchor_South,
    TextAnchor_NorthEast,
    TextAnchor_NorthWest,
    TextAnchor_SouthEast,
    TextAnchor_SouthWest,
    TextAnchor_Center
} TextAnchor;

bool SSD13x6_SetFont( struct SSD13x6_Device* Display, const struct SSD13x6_FontDef* Font );

void SSD13x6_FontForceProportional( struct SSD13x6_Device* Display, bool Force );
void SSD13x6_FontForceMonospace( struct SSD13x6_Device* Display, bool Force );

int SSD13x6_FontGetWidth( struct SSD13x6_Device* Display );
int SSD13x6_FontGetHeight( struct SSD13x6_Device* Display );

int SSD13x6_FontGetMaxCharsPerRow( struct SSD13x6_Device* Display );
int SSD13x6_FontGetMaxCharsPerColumn( struct SSD13x6_Device* Display );

int SSD13x6_FontGetCharWidth( struct SSD13x6_Device* Display, char Character );
int SSD13x6_FontGetCharHeight( struct SSD13x6_Device* Display );
int SSD13x6_FontMeasureString( struct SSD13x6_Device* Display, const char* Text );\

void SSD13x6_FontDrawChar( struct SSD13x6_Device* Display, char Character, int x, int y, int Color );
void SSD13x6_FontDrawString( struct SSD13x6_Device* Display, int x, int y, const char* Text, int Color );
void SSD13x6_FontDrawAnchoredString( struct SSD13x6_Device* Display, TextAnchor Anchor, const char* Text, int Color );
void SSD13x6_FontGetAnchoredStringCoords( struct SSD13x6_Device* Display, int* OutX, int* OutY, TextAnchor Anchor, const char* Text );

extern const struct SSD13x6_FontDef Font_droid_sans_fallback_11x13;
extern const struct SSD13x6_FontDef Font_droid_sans_fallback_15x17;
extern const struct SSD13x6_FontDef Font_droid_sans_fallback_24x28;

extern const struct SSD13x6_FontDef Font_droid_sans_mono_7x13;
extern const struct SSD13x6_FontDef Font_droid_sans_mono_13x24;
extern const struct SSD13x6_FontDef Font_droid_sans_mono_16x31;

extern const struct SSD13x6_FontDef Font_liberation_mono_9x15;
extern const struct SSD13x6_FontDef Font_liberation_mono_13x21;
extern const struct SSD13x6_FontDef Font_liberation_mono_17x30;

extern const struct SSD13x6_FontDef Font_Tarable7Seg_16x32;
extern const struct SSD13x6_FontDef Font_Tarable7Seg_32x64;

extern const struct SSD13x6_FontDef Font_line_1;
extern const struct SSD13x6_FontDef Font_line_2;

#ifdef __cplusplus
}
#endif

#endif
