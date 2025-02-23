#ifndef _SSD1306_FONTS_H_
#define _SSD1306_FONTS_H_

#include <stdint.h>

typedef enum {
 SSD1306_FONT_4X10_NUM   = 0,
 SSD1306_FONT_5X10_NUM   = 1,
 SSD1306_FONT_6X10_NUM   = 2,
 SSD1306_FONT_8X8        = 3,
 SSD1306_FONT_5X8        = 4,
 SSD1306_FONT_5X8_NARROW = 5,
 SSD1306_FONT_8X16       = 6,
 SSD1306_FONT_5X8_PROP   = 7,
 SSD1306_FONT_8X8_PROP   = 8,
 SSD1306_FONT_UNCHANGED  = -1
} ssd1306_font_id_t;

#define NUMBER_OF_SSD1306_FONTS  9

typedef struct { 
  uint8_t  cw;      // Character width in pixels 
  uint8_t  cshift;  // Pixel shift (useful when sharing character data with non-poportional font)
  uint16_t offset;  // Offset to bitmap
} ssd1306_font_descriptor_t;

typedef struct  {
  uint8_t char_width;          // Character width in pixels (average width for proportional fonts)
  uint8_t whitespace;          // whitespace to next character
  uint8_t char_height;         // Character height in bytes
  uint8_t char_size;           // Character size in bytes (can be overruled in proportional fonts) 
  uint8_t first_char;          // First character in font (usually ' ' or '!') 
  uint8_t last_char;           // Last  character in font (usually '~')
  uint8_t line_length;         // Number of characters per line (average width for proportional fonts)
  //const char * font_name;    // To be specified
  const uint8_t *font_bitmap;  // Font table start address in memory
  const ssd1306_font_descriptor_t *font_descriptor; // discriptors for each character (only for proportional fonts)
} ssd1306_font_t;

extern const ssd1306_font_t ssd1306_fonts[NUMBER_OF_SSD1306_FONTS];

#endif // _SSD1306_FONTS_H_
