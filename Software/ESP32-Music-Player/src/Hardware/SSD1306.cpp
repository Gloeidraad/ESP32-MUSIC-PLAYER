#include <arduino.h>
#include "hal/gpio_ll.h"

#include "SSD1306.h"

#define font_t    ssd1306_font_t
#define font_id_t ssd1306_font_id_t

#define SH1106_CONTROLLER      1  // Set to 1 if you want to support SH1106 also, instead of SSD1306 only 
#define SH1106_LCDWIDTH      132  // Internally 132 pixels, visible 128 pixels

// SH1106 command definitions
#define SH1106_CMD_SETMUX    (uint8_t)0xA8 // Set multiplex ratio (N, number of lines active on display)
#define SH1106_CMD_SETOFFS   (uint8_t)0xD3 // Set display offset
#define SH1106_CMD_STARTLINE (uint8_t)0x40 // Set display start line
#define SH1106_CMD_SEG_NORM  (uint8_t)0xA0 // Column 0 is mapped to SEG0 (X coordinate normal)
#define SH1106_CMD_SEG_INV   (uint8_t)0xA1 // Column 127 is mapped to SEG0 (X coordinate inverted)
#define SH1106_CMD_COM_NORM  (uint8_t)0xC0 // Scan from COM0 to COM[N-1] (N - mux ratio, Y coordinate normal)
#define SH1106_CMD_COM_INV   (uint8_t)0xC8 // Scan from COM[N-1] to COM0 (N - mux ratio, Y coordinate inverted)
#define SH1106_CMD_COM_HW    (uint8_t)0xDA // Set COM pins hardware configuration
#define SH1106_CMD_CONTRAST  (uint8_t)0x81 // Contrast control
#define SH1106_CMD_EDON      (uint8_t)0xA5 // Entire display ON enabled (all pixels on, RAM content ignored)
#define SH1106_CMD_EDOFF     (uint8_t)0xA4 // Entire display ON disabled (output follows RAM content)
#define SH1106_CMD_INV_OFF   (uint8_t)0xA6 // Entire display inversion OFF (normal display)
#define SH1106_CMD_INV_ON    (uint8_t)0xA7 // Entire display inversion ON (all pixels inverted)
#define SH1106_CMD_CLOCKDIV  (uint8_t)0xD5 // Set display clock divide ratio/oscillator frequency
#define SH1106_CMD_DISP_ON   (uint8_t)0xAF // Display ON
#define SH1106_CMD_DISP_OFF  (uint8_t)0xAE // Display OFF (sleep mode)

#define SH1106_CMD_COL_LOW   (uint8_t)0x00 // Set Lower Column Address
#define SH1106_CMD_COL_HIGH  (uint8_t)0x10 // Set Higher Column Address
#define SH1106_CMD_PAGE_ADDR (uint8_t)0xB0 // Set Page Address

#define SH1106_CMD_CHARGE    (uint8_t)0x22 // Dis-charge / Pre-charge Period
#define SH1106_CMD_SCRL_HR   (uint8_t)0x26 // Setup continuous horizontal scroll right
#define SH1106_CMD_SCRL_HL   (uint8_t)0x27 // Setup continuous horizontal scroll left
#define SH1106_CMD_SCRL_VHR  (uint8_t)0x29 // Setup continuous vertical and horizontal scroll right
#define SH1106_CMD_SCRL_VHL  (uint8_t)0x2A // Setup continuous vertical and horizontal scroll left
#define SH1106_CMD_SCRL_STOP (uint8_t)0x2E // Deactivate scroll
#define SH1106_CMD_SCRL_ACT  (uint8_t)0x2F // Activate scroll

#define SSD1306_LCDMEMSIZE          (OLED_LCDWIDTH * OLED_LCDROWS)

#define SSD1306_SH1106_SET_COL_LOW(a)   (SH1106_CMD_COL_LOW | a)  
#define SSD1306_SH1106_SET_COL_HIGH(a)  (SH1106_CMD_COL_HIGH | a) 
#if !SH1106_CONTROLLER
  #define SSD1306_MEMORYMODE        0x20
  #define SSD1306_COLUMNADDR        0x21
  #define SSD1306_PAGEADDR          0x22
#endif

#define SSD1306_SETCONTRAST         0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON        0xA5
#define SSD1306_NORMALDISPLAY       0xA6
#define SSD1306_INVERTDISPLAY       0xA7
#define SSD1306_DISPLAYOFF          0xAE
#define SSD1306_DISPLAYON           0xAF

#define SSD1306_SCROLL_RIGHT        0x26
#define SSD1306_SCROLL_LEFT         0x27
#define SSD1306_SCROLL_OFF          0x2E
#define SSD1306_SCROLL_ON           0x2F

#define SSD1306_SETDISPLAYOFFSET    0xD3
#define SSD1306_SETCOMPINS          0xDA
#define SSD1306_SETVCOMDETECT       0xDB
#define SSD1306_SETDISPLAYCLOCKDIV  0xD5
#define SSD1306_SETPRECHARGE        0xD9
#define SSD1306_SETMULTIPLEX        0xA8
//#define SSD1306_SETLOWCOLUMN        0x00
//#define SSD1306_SETHIGHCOLUMN       0x10
#define SSD1306_SETSTARTLINE        0x40
#define SSD1306_COMSCANINC          0xC0
#define SSD1306_COMSCANDEC          0xC8
#define SSD1306_SEGREMAP            0xA0
//#if !SH1106_CONTROLLER
  #define SSD1306_CHARGEPUMP          0x8D
//#endif
//#define SSD1306_EXTERNALVCC         0x1
//#define SSD1306_SWITCHCAPVCC        0x2

#define SSD1306_SH1106_SETPAGEADDRESS(p)   (0xB0 | (p))  

#define SSD1306_I2C_ADDRESS   OLED_I2C_ADDRESS

// Default speeds
#define SSD1306_I2C_SPEED     400000    // 400 kHz according to dataheets
#if SH1106_CONTROLLER
  #define SSD1306_SPI_SPEED   4000000   // SH1106 = 4 MHz according to dataheets
#else
  #define SSD1306_SPI_SPEED   10000000  // SSD1306 = 10 MHz according to dataheets
#endif

// Some shorthand helper functions for font list

inline const uint8_t * GET_FONT_DATA(const font_t * f, uint8_t c) {
  c -= f->first_char;
  if(f->font_descriptor == NULL)
    return f->font_bitmap + c * f->char_size;
  return f->font_bitmap + f->font_descriptor[c].offset + f->font_descriptor[c].cshift;
}

inline uint8_t GET_FONT_CHAR_WIDTH(const font_t * f, uint8_t c) {
  if(f->font_descriptor == NULL)
    return f->char_width;
  return (c < f->first_char || c > f->last_char) ? f->char_width : f->font_descriptor[c-f->first_char].cw;
}

#define delay_us(us)  delayMicroseconds(us)
#define delay_ms(ms)  delay(ms)

//=====================
//==  Useful Macros  ==
//=====================

#define USE_ESP32_FAST_IO

#ifdef USE_ESP32_FAST_IO
  #define PIN_CLR(p) GPIO.out_w1tc = (uint32_t)1 << (p)  // "clear"
  #define PIN_SET(p) GPIO.out_w1ts = (uint32_t)1 << (p)  // "set"
#else
  #define PIN_CLR(p) digitalWrite(p, LOW)  // Standard clear (slow)
  #define PIN_SET(p) digitalWrite(p, HIGH) // Standard set (slow)
#endif

#define SOFT_SPI_BYTE(c,sck,mosi) \
    PIN_CLR(sck); if((c) & 0x80) PIN_SET(mosi); else PIN_CLR(mosi); PIN_SET(sck); \
    PIN_CLR(sck); if((c) & 0x40) PIN_SET(mosi); else PIN_CLR(mosi); PIN_SET(sck); \
    PIN_CLR(sck); if((c) & 0x20) PIN_SET(mosi); else PIN_CLR(mosi); PIN_SET(sck); \
    PIN_CLR(sck); if((c) & 0x10) PIN_SET(mosi); else PIN_CLR(mosi); PIN_SET(sck); \
    PIN_CLR(sck); if((c) & 0x08) PIN_SET(mosi); else PIN_CLR(mosi); PIN_SET(sck); \
    PIN_CLR(sck); if((c) & 0x04) PIN_SET(mosi); else PIN_CLR(mosi); PIN_SET(sck); \
    PIN_CLR(sck); if((c) & 0x02) PIN_SET(mosi); else PIN_CLR(mosi); PIN_SET(sck); \
    PIN_CLR(sck); if((c) & 0x01) PIN_SET(mosi); else PIN_CLR(mosi); PIN_SET(sck)
 
/////////////////////////////////////// 
//
void ssd1306_class::send_command(uint8_t c) {
  if(_wire != NULL) {
    _wire->beginTransmission(SSD1306_I2C_ADDRESS);
    _wire->write(0X00); // This is Command
    _wire->write(c);
    _wire->endTransmission();
  }
  else if(_spi != NULL) {
    PIN_CLR(_dc);
    PIN_CLR(_cs);
    _spi->write(c);
    PIN_SET(_cs);
  }
  else if(_softspi) {
    PIN_SET(_softsck);
    PIN_CLR(_dc);
    PIN_CLR(_cs);
    SOFT_SPI_BYTE(c,_softsck,_softmosi); 
    PIN_SET(_cs);
  }
}

void ssd1306_class::send_command_d1(uint8_t c, uint8_t d1) {
  send_command(c);
  send_command(d1);
}

void ssd1306_class::send_command_d2(uint8_t c, uint8_t d1, uint8_t d2) {
  send_command(c);
  send_command(d1);
  send_command(d2);
}

void ssd1306_class::send_data(uint8_t dat, uint16_t len) { 
  uint8_t x;
  if(len > OLED_LCDWIDTH)
    len = OLED_LCDWIDTH;
  if(_wire != NULL) {
    _wire->beginTransmission(SSD1306_I2C_ADDRESS);
    _wire->write(0X40); // data not command
    #if I2C_BUFFER_LENGTH < 136 // Default ESP32 size is 128
      for(x=0; x < OLED_LCDWIDTH/2 && x < len; x++)  // Break down in half lines if necessary
        _wire->write(dat);
      _wire->endTransmission();
      if(x < len) { // Still bytes to go?
        _wire->beginTransmission(SSD1306_I2C_ADDRESS); 
        _wire->write(0X40); // data not command
        for(; x < len; x++)
          _wire->write(dat);
        _wire->endTransmission();
      } 
    #else // Buffer is large enough to write whole line
      for(x=0; x < len; x++)
        _wire->write(dat);
      _wire->endTransmission();
    #endif
  }
  else if(_spi != NULL) {
    PIN_SET(_dc);
    PIN_CLR(_cs);
    _spi->writePattern(&dat, 1, len);
    PIN_SET(_cs);
  }
  else if(_softspi) {
    PIN_SET(_softsck);
    PIN_SET(_dc);
    PIN_CLR(_cs);
    for(uint16_t i=0; i<len; i++) {
      SOFT_SPI_BYTE(dat,_softsck,_softmosi); 
    }
    PIN_SET(_cs);
  }
}

void ssd1306_class::send_data(const uint8_t * dat, uint16_t len) {
  uint8_t x;
  if(len > OLED_LCDWIDTH)
    len = OLED_LCDWIDTH;
  if(_wire != NULL) {
    _wire->beginTransmission(SSD1306_I2C_ADDRESS);
    _wire->write(0X40); // data not command
    #if I2C_BUFFER_LENGTH < 136 // Default ESP32 size is 128
      for(x=0; x < OLED_LCDWIDTH/2 && x < len; x++)  // Break down in half lines if necessary
        _wire->write(*dat++);
      _wire->endTransmission();
      if(x < len) { // Still bytes to go?
        _wire->beginTransmission(SSD1306_I2C_ADDRESS); 
        _wire->write(0X40); // data not command
        for(; x < len; x++)
          _wire->write(*dat++);
        _wire->endTransmission();
      } 
    #else // Buffer is large enough to write whole line
      for(x=0; x < len; x++)
        _wire->write(*dat++);
      _wire->endTransmission();
    #endif
  }
  else if(_spi != NULL) {
    PIN_SET(_dc);
    PIN_CLR(_cs);
    _spi->writeBytes(dat, len);
    PIN_SET(_cs);
  }
  else if(_softspi) {
    PIN_SET(_softsck);
    PIN_SET(_dc);
    PIN_CLR(_cs);
    for(uint16_t i=0; i<len; i++) {
      SOFT_SPI_BYTE(*dat,_softsck,_softmosi); 
      dat++;
    }
    PIN_SET(_cs);
  }
}

///////////////////////////////////////////////////////////////////
// 

void ssd1306_class::gotox(uint16_t x, bool in_pixels) {
  const font_t & fnt= ssd1306_fonts[_font_id];
  _px = in_pixels ? x : x * (fnt.char_width + fnt.whitespace);
  if(_px >= OLED_LCDWIDTH - fnt.char_width) _px = OLED_LCDWIDTH - fnt.char_width;
}

void ssd1306_class::gotoy(uint16_t y, bool in_lines) {
  _y = in_lines ? y / 8 : y;
}

///////////////////////////////////////////////////////////
// Clears whole sceen directly 
// You may increase I2C_BUFFER_LENGTH in Wire.h 
// from 128 to at least 136 in order to write a whole line in one burst
// Location of Wire.h for ESP32 is:
// C:\Users\[user]\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.2\libraries\Wire\src
void ssd1306_class::hardware_clearscreen() { 
  #if !SH1106_CONTROLLER
    send_command_d2(SSD1306_COLUMNADDR, 0, OLED_LCDWIDTH-1);
  #endif
  for (int y=0; y < OLED_LCDROWS; y++) {
    send_command(SSD1306_SH1106_SETPAGEADDRESS(y));
    send_command(SSD1306_SH1106_SET_COL_LOW(_start_column)); // A3-A0
    send_command(SSD1306_SH1106_SET_COL_HIGH(0));            // A7-A4
    send_data((uint8_t)0,OLED_LCDWIDTH);
  }
}
///////////////////////////////////////////////////////////
// Clears the whole screen of the SSD1306
void ssd1306_class::clearscreen() {
  _px = 0;
  _y  = 0;
  #if !OLED_USE_BUFFER
    _y_skip = 0;
  #endif
  clearlines(0, OLED_LCDROWS);
}

///////////////////////////////////////////////////////////////////
//

void ssd1306_class::progress_bar(uint8_t x, uint8_t y, uint8_t val, uint8_t max, bool double_row) {
  uint8_t bar[max+2];
  uint8_t rows = double_row ? 2 : 1;
  #if !OLED_USE_BUFFER && !SH1106_CONTROLLER
    send_command_d2(SSD1306_COLUMNADDR, 0, OLED_LCDWIDTH-1);
  #endif
  for(uint8_t r = 0; r < rows; r++) {
    if(double_row) {
      for(int i = 0; i <= max; i++) {
        if(r) bar[i] = val >= i ? 0x7F : 0x40;
        else  bar[i] = val >= i ? 0xFF : 0x01;
      }
      bar[max+1] = r ? 0x7F : 0xFF;
    }
    else {
      for(int i = 0; i <= max; i++) {
        bar[i] = val >= i ? 0x7F : 0x41;
      }
      bar[max+1] = 0x7F;
    }
    #if OLED_USE_BUFFER
      int col = x;
      setmem(col, y+r, bar, max+2);
    #else
      int col = x + _start_column;
      send_command(SSD1306_SH1106_SETPAGEADDRESS(y+r));
      send_command(SSD1306_SH1106_SET_COL_LOW(col & 15));   // A3-A0
      send_command(SSD1306_SH1106_SET_COL_HIGH(col >> 4));  // A7-A4
      send_data(bar, max+2);
    #endif
  }
}

#if OLED_USE_BUFFER
ssd1306_class::ssd1306_class(uint8_t rows, uint8_t tickers) {
  memset(this, 0, sizeof(ssd1306_class));
  _device  = OLED_UNKNOWN;
  _font_id = SSD1306_FONT_8X8;
  _nr_rows    = rows;
  _nr_tickers = tickers;
  if(_nr_rows > 0) {
    _lines   = (line_t *)malloc(rows * sizeof(line_t));
    memset(_lines, 0, rows * sizeof(line_t));
    for(int i = 0; i < _nr_rows && i < OLED_LCDROWS; i++) {
      _lines[i].row    = i; 
      _lines[i].active = 1;
    }
  }
  if(_nr_tickers > 0) {
    _tickers = (ticker_t *)malloc(tickers * sizeof(ticker_t));
    memset(_tickers, 0, tickers * sizeof(ticker_t));
  }
}
#else
ssd1306_class::ssd1306_class() {
  memset(this, 0, sizeof(ssd1306_class));
  _device  = OLED_UNKNOWN;
  _font_id = SSD1306_FONT_8X8;
}
#endif

void ssd1306_class::setport(TwoWire & wire) {
  _wire     = &wire;
  _spi      = NULL;
  _softspi  = false;
}

void ssd1306_class::setport(SPIClass & spi, uint8_t cs, uint8_t dc) {
  _wire     = NULL;
  _spi      = &spi;
  _softspi  = false;
  _cs       = cs;
  _dc       = dc;
}

void ssd1306_class::setport(SoftSPI & softspi, uint8_t cs, uint8_t dc) {
  _wire     = NULL;
  _spi      = NULL;
  _softspi  = true;
  _cs       = cs;
  _dc       = dc;
  _softsck  = softspi._sck;
  _softmosi = softspi._mosi;
}

ssd1306_class::~ssd1306_class() {
  #if OLED_USE_BUFFER
    free(_lines);
    for(int i = 0; i < _nr_tickers; i++) {
      if(_tickers[i].data != NULL) free(_tickers[i].data);
    } 
    free(_tickers); 
  #endif
}

void ssd1306_class::suspend() {  
  if(_spi != NULL) {
    _spi->endTransaction(); // For some reason Wifi will not connect with open SPI port
  }
}

void ssd1306_class::resume() {
  if(_spi != NULL) {
    _spi->beginTransaction(SPISettings(_speed, SPI_MSBFIRST, SPI_MODE0));
  }
}

uint8_t ssd1306_class::initialize(uint8_t flags, uint32_t speed) {
  delay_ms(10);
  // check for SH1106 controller
  if(_wire != NULL) {
    _speed = (speed < 10000) ? SSD1306_I2C_SPEED : speed;
    _wire->setClock(_speed);
    _wire->beginTransmission(SSD1306_I2C_ADDRESS);
    _wire->write(0);
    if(_wire->endTransmission() != 0) {
       return OLED_UNKNOWN;
    }
    _wire->requestFrom(0x3C, 1);
    uint8_t c = _wire->read();    // Receive a byte as character
    c &= 0x0f; // mask off power on/off bit
    if(c == 0x8) {
      _device = OLED_SH1106;
      _start_column = 2;
    }
    else {
      _device = OLED_SSD1306;
      _start_column = 0;
    }
  }
  else if(_spi != NULL) {
    pinMode(_cs, OUTPUT);
    pinMode(_dc, OUTPUT);
    PIN_SET(_cs);
    PIN_SET(_dc);
    delay_ms(1);
    _speed = (speed < 10000) ? SSD1306_SPI_SPEED : speed;
    _spi->beginTransaction(SPISettings(_speed, SPI_MSBFIRST, SPI_MODE0));
  }
  else if(_softspi) {
    pinMode(_cs, OUTPUT);
    pinMode(_dc, OUTPUT);
    pinMode(_softsck, OUTPUT);
    pinMode(_softmosi, OUTPUT);
    PIN_SET(_cs);
    PIN_SET(_dc);
    PIN_SET(_softsck);
    PIN_SET(_softmosi);
  }
  if(flags & OLED_INIT_NO_REGS_MASK) return _device;
  delay(100);
  // Init sequence for 128x64 OLED module
  send_command(SSD1306_DISPLAYOFF);                  // 0xAE
  send_command_d1(SSD1306_SETDISPLAYCLOCKDIV, 0x80); // 0xD5, the suggested ratio 0x80
  send_command_d1(SSD1306_SETMULTIPLEX, 0x3F);       // 0xA8
  send_command_d1(SSD1306_SETDISPLAYOFFSET, 0x00);   // 0xD3, no offset
  send_command(SSD1306_SETSTARTLINE);                // line #0
  #if !SH1106_CONTROLLER
    send_command_d1(SSD1306_CHARGEPUMP, 0x14);       // 0x8D, using internal VCC
    send_command_d1(SSD1306_MEMORYMODE, 0x00);       // 0x20, 0x00 horizontal addressing
  #else
    send_command_d1(SSD1306_CHARGEPUMP, 0x14);       // Keep it for dual display types
  #endif
  if(flags & OLED_INIT_ROTATE180_MASK) {
    send_command(SSD1306_SEGREMAP | 0x1);            // rotate screen 180   
    send_command(SSD1306_COMSCANDEC);                // rotate screen 180
  }                  
  else {
    send_command(SSD1306_SEGREMAP | 0x0);            // rotate screen 0   
    send_command(SSD1306_COMSCANINC);                // rotate screen 0  
  }
  send_command_d1(SSD1306_SETCOMPINS, 0x12);         // 0xDA
  send_command_d1(SSD1306_SETCONTRAST, 0xFF);        // 0x81, max contrast
  send_command_d1(SSD1306_SETPRECHARGE, 0xF1);       // 0xd9
  send_command_d1(SSD1306_SETVCOMDETECT, 0x40);      // 0xDB
  send_command(SSD1306_DISPLAYALLON_RESUME);         // 0xA4
  send_command(SSD1306_NORMALDISPLAY);               // 0xA6
  send_command(SSD1306_DISPLAYON);                   // switch on OLED
  delay_ms(10);
  send_command(SSD1306_SCROLL_OFF);
  #if OLED_USE_BUFFER
    _state    = 0;
    _rowcount = 0;
    _colcount = 0;
    clearbuffer(0);
  #endif
  if((flags & OLED_INIT_NO_ERASE_MASK)) // Erase in case callback wil not be called soon
    hardware_clearscreen();
  return _device;
}

uint16_t ssd1306_class::getValidStringLength(const char *str, font_id_t font_id) {
  uint16_t maxlen = 0;
  uint16_t cn = 0;
  if(font_id == SSD1306_FONT_UNCHANGED) font_id = _font_id;
  const font_t * font= &ssd1306_fonts[font_id];
  char c = *str++;
  while(c) {
    if(c >= font->first_char && c <= font->last_char && font->font_descriptor != NULL)
      cn += font->font_descriptor[c - font->first_char].cw;
    else
      cn += font->char_width;
    if(cn > OLED_LCDWIDTH) return maxlen;
    maxlen++;
    cn += font->whitespace;
    c = *str++;
  }
  return maxlen;
}

uint16_t ssd1306_class::getStringWidth(const char *str, font_id_t font_id) {
  uint16_t cn = 0;
  if(font_id == SSD1306_FONT_UNCHANGED) font_id = _font_id;
  const font_t * font= &ssd1306_fonts[font_id];
  char c = *str++;
  while(c) {
    if(c >= font->first_char && c <= font->last_char && font->font_descriptor != NULL)
      cn += font->font_descriptor[c - font->first_char].cw;
    else
      cn += font->char_width;
    cn += font->whitespace;
    c = *str++;
  }
  if(cn > 0) cn -= font->whitespace; // strip whitespace of last char
  return cn;
}

uint16_t ssd1306_class::puts_right_aligned(const char * s, font_id_t font) {
  uint16_t x, w;
  if(font == SSD1306_FONT_UNCHANGED) font = _font_id;
  w = getStringWidth(s, font);
  if(w > OLED_LCDWIDTH) w = OLED_LCDWIDTH;
  _px = OLED_LCDWIDTH - w;
   puts(s, font);
   return _px;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if OLED_USE_BUFFER

void ssd1306_class::Print(void) {
  Serial.printf("OLED: width=%d, height=%d, text lines=%d, mem size=%d\n", OLED_LCDWIDTH,OLED_LCDHEIGHT,OLED_LCDROWS,SSD1306_LCDMEMSIZE);
  Serial.printf("      buffer lines=%d, buffer memory=%d\n", _nr_rows, _nr_rows * sizeof(line_t));
  Serial.printf("      tickers=%d, ticker memory=%d\n", _nr_tickers, _nr_tickers * sizeof(ticker_t));

  Serial.println("Buffer configuration");
  for(int i = 0; i <_nr_rows; i++) 
    Serial.printf("Line %d, row %d, active %d, update %d\n",i, _lines[i].row, _lines[i].active, _lines[i].update);

  Serial.println("Ticker configuration");
  for(int i = 0; i <_nr_tickers; i++) 
    Serial.printf("ID %d, row_id %d, scroll_delay %d, delay_counter %d, len %d, idx %d\n", i, _tickers[i].row_id, 
                   _tickers[i].scroll_delay, _tickers[i].delay_counter, _tickers[i].len, _tickers[i].idx);
}

void ssd1306_class::putch(uint8_t c, font_id_t font) {
  if(font == -1)
    font = _font_id;
  const font_t *f = &ssd1306_fonts[font];
  int col, w, cw, y, i, j;
  unsigned char * p;
  if(_y < _nr_rows) {
    y  = _y * f->char_height;
    cw = GET_FONT_CHAR_WIDTH(f, c);
    w  = cw + f->whitespace;
    col = _px;
    if(col + w > OLED_LCDWIDTH) 
      w = cw; // supress blank pixels after last character of line
    if(col + w > OLED_LCDWIDTH) return; // Still doesn't fit

    if(c < f->first_char || c > f->last_char) { // Character not in font data: write blank space
      for(i = 0; i < f->char_height; i++) {
        _lines[y+i].update = 1;
        p = _lines[y+i].row_data;
        for(j = 0; j < w; j++)
          p[col+j] = 0;
      }
    }
    else { // Character in font data: write it
      const unsigned char * fd = GET_FONT_DATA(f,c);
      for(i = 0; i < f->char_height; i++) {
        _lines[y+i].update = 1;
        p = _lines[y+i].row_data;
        for(j = 0; j < cw; j++)
          p[col+j] = fd[i * f->char_width + j];
        for(; j < w; j++)
          p[col+j] = 0;
      }
    }
    _px += w;
  }
}

void ssd1306_class::puts(const char *s, uint8_t n, font_id_t font) {
  stop_ticker(_y); // If a ticker is running on current line, stop it
  while(n && *s) { putch(*s++, font); n--; }
  while(n--)     { putch(' ', font); }
}

void ssd1306_class::clearlines(uint8_t y, uint8_t n) { for(int i = 0; i < n; i++) setline(y+i, 0); }
void ssd1306_class::clearbuffer(uint8_t v) { clearlines(0, _nr_rows); }

void ssd1306_class::display(bool force_all) {
  if(force_all) {
    for(int i = 0; i < _nr_rows; i++)
      _lines[i].update = true;
  }
  while(_state || check_rowupdate()) {
    loop(false);
  }
}

//=====================================================================
// News Ticker Routines
//=====================================================================

#define TY _tickers[id]

void ssd1306_class::update_ticker(uint8_t id) {
  if(TY.len > 0 && TY.data != NULL && _timestamp - TY.timestamp_prev > TY.timestamp_speed) {
    TY.timestamp_prev = _timestamp;
    if(TY.delay_counter) {
      TY.delay_counter--;
    }
    else {
      if(++TY.idx < TY.len) {
        memcpy(_lines[TY.row_id].row_data, &TY.data[TY.idx], OLED_LCDWIDTH); 
        _lines[TY.row_id].update = 1;
      }
      else {
        TY.delay_counter = TY.scroll_delay;
        TY.idx = 0;
        memcpy(_lines[TY.row_id].row_data, TY.data, OLED_LCDWIDTH); 
        _lines[TY.row_id].update = 1;
      }
    }
  }
}

void ssd1306_class::start_ticker(uint8_t id, uint8_t row_id, const char *s, font_id_t font, uint16_t speed, uint16_t scrolldelay) {
  const font_t *f = &ssd1306_fonts[font];
  int linelen = f->line_length;
  int len = strlen(s);
  int16_t datalen = getStringWidth(s, font);
  int8_t cw;
  Serial.printf("Start ticker: font: %d, char/line: %d, strlen: %d, datalen: %d\n", font, linelen, len, datalen);
  if(id >= _nr_tickers) {
    Serial.printf("Cannot create ticker id %d for row %d\n", id, row_id);
    return; // out of bounds
  }
  stop_ticker(id, false); // stop possible previous ticker
  if(datalen <= OLED_LCDWIDTH) {
    Serial.printf("No need to create ticker for: '%s'\n", s);
    setline(row_id, 0);
    puts(0, row_id, s, font); // show the line
  }
  else {
    datalen += 2 * OLED_LCDWIDTH; // reserve extra for nice flow
    Serial.printf("Alloc new text buffer: %d bytes\n", datalen + 1);
    TY.timestamp_prev  = _timestamp;
    TY.timestamp_speed = speed;
    TY.data = (uint8_t *)malloc(datalen + 1);
    memset(TY.data, 0, datalen);
    TY.scroll_delay  = scrolldelay;
    TY.delay_counter = scrolldelay;
    TY.idx = 0;
    TY.len = datalen - OLED_LCDWIDTH; // Including a full blank line
    uint8_t * mem = TY.data;
    for(int i = 0; i < len; i++) { // fill the data with graphic text
      cw = GET_FONT_CHAR_WIDTH(f, s[i]);
      if(s[i] >= f->first_char && s[i] <= f->last_char)
        memcpy(mem, GET_FONT_DATA(f,s[i]), cw);
      else 
        memset(mem, 0, cw);
      mem += cw + f->whitespace;
    }
    TY.row_id = row_id;
    memcpy(&TY.data[TY.len], TY.data, OLED_LCDWIDTH);
    memcpy(_lines[TY.row_id].row_data, TY.data, OLED_LCDWIDTH); // copy start of data to screen
    _lines[TY.row_id].update = 1;
  }
}

void ssd1306_class::stop_ticker(uint8_t id, bool id_is_y) {
  if(id_is_y) {
    for(int i = 0; i < _nr_tickers; i++) { 
      if(_tickers[i].row_id == id) {
        stop_ticker(i, false);
        return;
      }
    }
  }
  else {
    if(TY.data != NULL) {
      Serial.printf("Free ticker text buffer\n");
      free(TY.data);
      TY.data = NULL;
      TY.len  = 0;
    }
  }
}

//=====================================================================
// A lot of fonts uses only 7 vertical pixels, so moving down one pixel 
// doesn't change the text, but might result in nicer display. If more
// than 1 pixel shift is defined, then the text is distributed over two
// lines. 
//=====================================================================

void ssd1306_class::shift_row_down(uint8_t y, uint8_t shift) {
  if(shift == 0 || shift > 7) return; // illegal shift
  //Serial.printf("********************* ssd1306_class: shift_row_down = %d with %d\n", y, shift);
  if(shift == 1) {
    for(int i = 0; i < OLED_LCDWIDTH; i++) { 
      _lines[y].row_data[i] <<= 1;
      _lines[y].update = 1;
    }
  }
  else {
    if(y < _nr_rows - 1) { // Cannot shift last row
      for(int i = 0; i < OLED_LCDWIDTH; i++) {
        _lines[y + 1].row_data[i] = _lines[y].row_data[i] >> (8 - shift);
        _lines[y]    .row_data[i] <<= shift;
      }
      _lines[y].  update = 1;
      _lines[y+1].update = 1;
    }
  }
}

#define LOOP_START  6

uint8_t ssd1306_class::loop(bool tick) {
  if(tick) _timestamp++;
  if(_wire != NULL) { // break op I2C communicaion in smaller pieces
    switch(_state) {
      case 6: send_command(SH1106_CMD_COL_LOW + _start_column); // A3-A0
              //Serial.printf("SSD1306 row: %u\n", _lines[_rowcount].row);
              _state--;
              break;
      case 5: send_command(SH1106_CMD_COL_HIGH);                // A7-A4
              _state--;
              break;
      case 4: send_command(SSD1306_SH1106_SETPAGEADDRESS(_lines[_rowcount].row));
              _state--;
              break;
      case 3: send_data(_lines[_rowcount].row_data,OLED_LCDWIDTH/2); // first half of line
              _state--;
              break;   
      case 2: send_data(&_lines[_rowcount].row_data[OLED_LCDWIDTH/2],OLED_LCDWIDTH/2); // second half of line
              _state--;
              break;
      case 1: _state--;
              return 1; // I2C bus is now free, allow other devices to interact
              break;
 
      default: update_ticker(_tickercount);
               if(++_tickercount >= _nr_tickers) _tickercount = 0;
               if(++_rowcount    >= _nr_rows)    _rowcount    = 0;
               if(_lines[_rowcount].active && _lines[_rowcount].update) {
                 _lines[_rowcount].update = 0;
                 _state = LOOP_START;
               }
               break;
    }
  }
  else { // SPI is fast enough to be written at once
    update_ticker(_tickercount);
    if(++_tickercount >= _nr_tickers) _tickercount = 0;
    if(++_rowcount    >= _nr_rows)    _rowcount    = 0;
    if(_lines[_rowcount].active && _lines[_rowcount].update) {
      _lines[_rowcount].update = 0;
      send_command(SH1106_CMD_COL_LOW + _start_column); // A3-A0
      send_command(SH1106_CMD_COL_HIGH);                // A7-A4
      send_command(SSD1306_SH1106_SETPAGEADDRESS(_lines[_rowcount].row));
      send_data(_lines[_rowcount].row_data,OLED_LCDWIDTH); 
    }
  }
  return 0;
}

void ssd1306_class::setmem(uint8_t x, uint8_t y, uint8_t v, uint8_t n) {
  //stop_ticker(y); // If a ticker is running on current line, stop it
  if(y < _nr_rows) {
    memset(&_lines[y].row_data[x], v, n);
    _lines[y].update = 1;
  }
}

void ssd1306_class::setmem(uint8_t x, uint8_t y, const uint8_t *v, uint8_t n) {
  //stop_ticker(y); // If a ticker is running on current line, stop it
  if(y < _nr_rows) {
    memcpy(&_lines[y].row_data[x], v, n);
    _lines[y].update = 1;
  }
}

#else

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
// Clears lines of the SSD1306ssd1306_command
// You may increase I2C_BUFFER_LENGTH in Wire.h 
// from 128 to at least 136 in order to write a whole line in one burst
// Location of Wire.h for ESP32 is:
// C:\Users\[user]\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.2\libraries\Wire\src
void ssd1306_class::clearlines(uint8_t y, uint8_t n) { 
  #if !SH1106_CONTROLLER
    send_command_d2(SSD1306_COLUMNADDR, 0, OLED_LCDWIDTH-1);
  #endif
  for (int yy=0; yy<n; yy++) {
    send_command(SSD1306_SH1106_SETPAGEADDRESS(y+yy));
    send_command(SSD1306_SH1106_SET_COL_LOW(_start_column)); // A3-A0
    send_command(SSD1306_SH1106_SET_COL_HIGH(0));            // A7-A4
    send_data((uint8_t)0,OLED_LCDWIDTH);
  }
}

#if SH1106_CONTROLLER
void ssd1306_class::putch(uint8_t c, font_id_t font) {
  if(font == -1)
    font = _font_id;
  const font_t *f = &ssd1306_fonts[font];
  int col, w, cw, y, i, j;
  unsigned char * p;
  
  if(_y < OLED_LCDROWS) {
    y  = _y * f->char_height;
    cw = GET_FONT_CHAR_WIDTH(f, c);
    w  = cw + f->whitespace;
    col = _px;
    if(col + w > OLED_LCDWIDTH) 
      w = cw; // supress blank pixels after last character of line
    if(col + w > OLED_LCDWIDTH) return; // Still doesn't fit

    if(c < f->first_char || c > f->last_char) { // Character not in font data: write blank space
      for(uint8_t r = 0; r < f->char_height; r++) {
        send_command(SSD1306_SH1106_SETPAGEADDRESS(y+r));
        send_command(SSD1306_SH1106_SET_COL_LOW(col & 15));   // A3-A0
        send_command(SSD1306_SH1106_SET_COL_HIGH(col >> 4));  // A7-A4
        send_data((uint8_t)0, w);
      }
    }
    else { // Character in font data: write it
      const unsigned char * fd = GET_FONT_DATA(f,c);
      for(uint8_t r = 0; r < f->char_height; r++) {
        send_command(SSD1306_SH1106_SETPAGEADDRESS(y+r));
        send_command(SSD1306_SH1106_SET_COL_LOW(col & 15));   // A3-A0
        send_command(SSD1306_SH1106_SET_COL_HIGH(col >> 4));  // A7-A4
        send_data(&fd[r * f->char_width], cw); 
        if(w > cw)
          send_data((uint8_t)0, w - cw);
      }
    }
    _px += w;
  }
}

#else

void ssd1306_class::putch(unsigned char c, font_id_t font) {
  if(font == -1)
    font = _font_id;
  const font_t *f = &ssd1306_fonts[font];
  int col, w, cw, y, i, j;
  unsigned char * p;
  
  if(_y < OLED_LCDROWS) {
    y  = _y * f->char_height;
    cw = GET_FONT_CHAR_WIDTH(f, c);
    w  = cw + f->whitespace;
    col = _px;
    if(col + w > OLED_LCDWIDTH) 
      w = cw; // supress blank pixels after last character of line
    if(col + w > OLED_LCDWIDTH) return; // Still doesn't fit
    if(!_y_skip) 
      send_command_d2(SSD1306_PAGEADDR, y, y + f->char_height - 1);
    send_command_d2(SSD1306_COLUMNADDR, col, col + w - 1);
    if(c < f->first_char || c > f->last_char) { // Character not in font data: write blank space
      send_data((uint8_t)0, w * f->char_height);
    }
    else { // Character in font data: write it
      const unsigned char * fd = GET_FONT_DATA(f,c);
      for(uint8_t r = 0; r < f->char_height; r++) {
        send_data(&fd[r * f->char_width], cw); 
        if(w > cw)
          send_data((uint8_t)0, w - cw);
      }
    }
    _px += w;
  }
}
#endif

void ssd1306_class::puts(const char *s, uint8_t n, font_id_t font) {
  while(n && *s) { putch(*s++,font); _y_skip = 1; n--; }
  while(n--)     { putch(' ',font);  _y_skip = 1;      }
  _y_skip = 0;
}

#endif //OLED_USE_BUFFER
