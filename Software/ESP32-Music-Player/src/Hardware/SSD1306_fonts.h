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
 SSD1306_FONT_UNCHANGED  = -1
} SSD1306_FontId_t;

#define NUMBER_OF_SSD1306_FONTS  7

typedef struct  {
  uint8_t u8DataCols;     /* Character width in pixels               */
  uint8_t u8FontWidth;    /* Character width to next character       */
  uint8_t u8DataRows;     /* Character height in bytes               */
  uint8_t u8BytesPerChar; /* Character size in bytes                 */
  uint8_t u8FirstChar;    /* First character in font (usually '!')   */
  uint8_t u8LastChar;     /* Last  character in font (usually '~')   */
  uint8_t u8LineLength;   /* Number of characters per line           */
  const uint8_t *au8FontTable; /* Font table start address in memory */
} SSD1306_Font_t;

extern const SSD1306_Font_t SSD1306_Fonts[NUMBER_OF_SSD1306_FONTS];

#endif // _SSD1306_FONTS_H_
