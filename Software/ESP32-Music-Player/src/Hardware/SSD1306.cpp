#include <arduino.h>

#include "SSD1306.h"

#define font_id SSD1306_FontId_t

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

// 
#define SSD1306_LCDMEMSIZE          (SSD1306_LCDWIDTH * SSD1306_LCDROWS)

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
 
#define SSD1306_I2C_ADDRESS   0x3C  // on PIC: 0x78
#if SH1106_CONTROLLER
  #define SSD1306_SPI_SPEED   4000000  // SH1106 = 4 MHz according to dataheets
#else
  #define SSD1306_SPI_SPEED   20000000 // SSD1306 = 10 MHz according to dataheets
#endif

// Some shorthand helpers for font list
#define FONT               SSD1306_Fonts[_font]
#define FONT_FIRST_CHAR    FONT.u8FirstChar
#define FONT_LAST_CHAR     FONT.u8LastChar
#define FONT_DATA_COLS     FONT.u8DataCols
#define FONT_DATA_ROWS     FONT.u8DataRows
#define FONT_DATA_LAST_COL (FONT_DATA_COLS - 1)
#define FONT_DATA_LAST_ROW (FONT_DATA_ROWS - 1)
#define FONT_CHAR_SIZE     (FONT_DATA_COLS * FONT_DATA_ROWS)
#define FONT_WIDTH         FONT.u8FontWidth
#define FONT_HEIGHT        (FONT_DATA_ROWS * 8)
#define FONT_LINE_LENGTH   FONT.u8LineLength
#define FONT_DATA          FONT.au8FontTable

#define GET_FONT_DATA(f,c) (f->au8FontTable + (c - f->u8FirstChar) * f->u8BytesPerChar)

#define delay_us(us)  delayMicroseconds(us)
#define delay_ms(ms)  delay(ms)

/////////////////////////////////////// 
//
void ssd1306_class::send_command(unsigned char c) {
  if(_wire != NULL) {
    _wire->beginTransmission(SSD1306_I2C_ADDRESS);
    _wire->write(0X00); // This is Command
    _wire->write(c);
    _wire->endTransmission();
  }
  else if(_spi != NULL) {
    digitalWrite(_dc, LOW);
    digitalWrite(_cs, LOW);
    _spi->write(c);
    digitalWrite(_cs, HIGH);
  }
}

void ssd1306_class::send_command_d1(unsigned char c, unsigned char d1) {
  send_command(c);
  send_command(d1);
}

void ssd1306_class::send_command_d2(unsigned char c, unsigned char d1, unsigned char d2) {
  send_command(c);
  send_command(d1);
  send_command(d2);
}

void ssd1306_class::send_data(const uint8_t dat, uint16_t len) { 
  uint8_t x;
  if(len > SSD1306_LCDWIDTH)
    len = SSD1306_LCDWIDTH;
  if(_wire != NULL) {
    _wire->beginTransmission(SSD1306_I2C_ADDRESS);
    _wire->write(0X40); // data not command
    #if I2C_BUFFER_LENGTH < 136 // Default ESP32 size is 128
      for(x=0; x < SSD1306_LCDWIDTH/2 && x < len; x++)  // Break down in half lines if necessary
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
    digitalWrite(_dc, HIGH);
    digitalWrite(_cs, LOW);
    _spi->writePattern(&dat, 1, len);
    digitalWrite(_cs, HIGH);
  }
}

void ssd1306_class::send_data(const uint8_t * dat, uint16_t len) {
  uint8_t x;
  if(len > SSD1306_LCDWIDTH)
    len = SSD1306_LCDWIDTH;
  if(_wire != NULL) {
    _wire->beginTransmission(SSD1306_I2C_ADDRESS);
    _wire->write(0X40); // data not command
    #if I2C_BUFFER_LENGTH < 136 // Default ESP32 size is 128
      for(x=0; x < SSD1306_LCDWIDTH/2 && x < len; x++)  // Break down in half lines if necessary
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
    digitalWrite(_dc, HIGH);
    digitalWrite(_cs, LOW);
    _spi->writeBytes(dat, len);
    digitalWrite(_cs, HIGH);
  }
}

///////////////////////////////////////////////////////////////////
// 

#define SSD1306_SETXY(x,y) \
  _x = x;           \
  _y = y;       

void ssd1306_class::gotoxy(uint8_t x, uint8_t y) {
  SSD1306_SETXY(x,y)
}

///////////////////////////////////////////////////////////
// Clears whole sceen directly 
// You may increase I2C_BUFFER_LENGTH in Wire.h 
// from 128 to at least 136 in order to write a whole line in one burst
// Location of Wire.h for ESP32 is:
// C:\Users\[user]\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.2\libraries\Wire\src
void ssd1306_class::hardware_clearscreen() { 
  #if !SH1106_CONTROLLER
    send_command_d2(SSD1306_COLUMNADDR, 0, SSD1306_LCDWIDTH-1);
  #endif
  for (int y=0; y<SSD1306_LCDROWS; y++) {
    send_command(SSD1306_SH1106_SETPAGEADDRESS(y));
    send_command(SSD1306_SH1106_SET_COL_LOW(_start_column)); // A3-A0
    send_command(SSD1306_SH1106_SET_COL_HIGH(0));            // A7-A4
    send_data((uint8_t)0,SSD1306_LCDWIDTH);
  }
}
///////////////////////////////////////////////////////////
// Clears the whole screen of the SSD1306
void ssd1306_class::clearscreen() {
  _x        = 0;
  _y        = 0;
  _x_offset = 0;
  #if !OLED_USE_BUFFER
    _y_skip   = 0;
  #endif
  clearlines(0, SSD1306_LCDROWS);
}

///////////////////////////////////////////////////////////////////
//

void ssd1306_class::progress_bar(uint8_t x, uint8_t y, uint8_t val, uint8_t max, bool double_row) {
  uint8_t bar[max+2];
  uint8_t rows = double_row ? 2 : 1;
  #if !OLED_USE_BUFFER && !SH1106_CONTROLLER
    send_command_d2(SSD1306_COLUMNADDR, 0, SSD1306_LCDWIDTH-1);nn
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
      int col = x + _x_offset;
      setmem(col, y+r, bar, max+2);
    #else
      int col = x + _x_offset + _start_column;
      send_command(SSD1306_SH1106_SETPAGEADDRESS(y+r));
      send_command(SSD1306_SH1106_SET_COL_LOW(col & 15));   // A3-A0
      send_command(SSD1306_SH1106_SET_COL_HIGH(col >> 4));  // A7-A4
      send_data(bar, max+2);
    #endif
  }
}

ssd1306_class::ssd1306_class(TwoWire * wire, SPIClass * spi, uint8_t cs, uint8_t dc, uint8_t rows, uint8_t tickers) {
  memset(this, 0, sizeof(ssd1306_class));
  _wire       = wire;
  _spi        = spi;
  _cs         = cs;
  _dc         = dc;
  _device     = OLED_UNKNOWN;
  _font       = SSD1306_FONT_8X8;
  #if OLED_USE_BUFFER
    _nr_rows    = rows;
    _nr_tickers = tickers;
    if(_nr_rows > 0 && _nr_tickers > 0) {
      _lines   = (line_t   *)malloc(rows    * sizeof(line_t));
      _tickers = (ticker_t *)malloc(tickers * sizeof(ticker_t));
      memset(_lines, 0, rows * sizeof(line_t));
      memset(_tickers, 0, tickers * sizeof(ticker_t));
      for(int i = 0; i < _nr_rows && i < SSD1306_LCDROWS; i++) {
        _lines[i].row    = i; 
        _lines[i].active = 1;
      }
    }
  #endif
}

ssd1306_class::~ssd1306_class() {
  #if OLED_USE_BUFFER
    free(_lines);
    for(int i = 0; i < _nr_tickers; i++) {
      if(_tickers->data != NULL) free(_tickers->data);
    } 
    free(_tickers); 
  #endif
}

uint8_t ssd1306_class::initialize(uint8_t flags) {
  delay_ms(10);
  // check for SH1106 controller
  if(_wire != NULL) {
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
    digitalWrite(_cs, HIGH);
    digitalWrite(_dc, HIGH);
    delay_ms(10);
    digitalWrite(_cs, LOW);
    _spi->beginTransaction(SPISettings(SSD1306_SPI_SPEED, SPI_MSBFIRST, SPI_MODE0));
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if OLED_USE_BUFFER

void ssd1306_class::Print(void) {
  Serial.printf("OLED: width=%d, height=%d, text lines=%d, mem size=%d\n", SSD1306_LCDWIDTH,SSD1306_LCDHEIGHT,SSD1306_LCDROWS,SSD1306_LCDMEMSIZE);
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

void ssd1306_class::putch(uint8_t c, font_id font) {
  if(font == -1)
    font = _font;
  const SSD1306_Font_t *f = &SSD1306_Fonts[font];
  int col, w, y, i, j;
  unsigned char * p;
  if(_y < _nr_rows) {
    w   = f->u8FontWidth;
    y   = _y * f->u8DataRows;
    col = _x_offset ? _x_offset : _x * w;
    if(col + w > SSD1306_LCDWIDTH)
      w = f->u8DataCols; // supress blank pixels after last character of line
    if(c < f->u8FirstChar || c > f->u8LastChar) {
      for(i = 0; i < f->u8DataRows; i++) {
        _lines[y+i].update = 1;
        p = _lines[y+i].row_data;
        for(j = 0; j < w; j++)
          p[col+j] = 0;
      }
    }
    else {
      const unsigned char * fd = GET_FONT_DATA(f,c);
      for(i = 0; i < f->u8DataRows; i++) {
        _lines[y+i].update = 1;
        p = _lines[y+i].row_data;
        for(j = 0; j < f->u8DataCols; j++)
          p[col+j] = fd[i * f->u8DataCols + j];
        for(; j < w; j++)
          p[col+j] = 0;
      }
    }
    if(_x_offset)
      _x_offset += w;
    else
      _x++; // advance cursor
  }
}

void ssd1306_class::puts(const char *s, uint8_t n, font_id font) {
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
        memcpy(_lines[TY.row_id].row_data, &TY.data[TY.idx], SSD1306_LCDWIDTH); 
        _lines[TY.row_id].update = 1;
      }
      else {
        TY.delay_counter = TY.scroll_delay;
        TY.idx = 0;
        memcpy(_lines[TY.row_id].row_data, TY.data, SSD1306_LCDWIDTH); 
        _lines[TY.row_id].update = 1;
      }
    }
  }
}

void ssd1306_class::start_ticker(uint8_t id, uint8_t row_id, const char *s, uint8_t font, uint16_t speed, uint16_t scrolldelay) {
  const SSD1306_Font_t *f = &SSD1306_Fonts[font];
  int ll = f->u8LineLength;
  int len = strlen(s);
  int16_t datalen;
  int8_t  w;
  Serial.printf("Start ticker: font: %d, char/line: %d, strlen: %d\n", font, ll, len);
  if(id >= _nr_tickers) {
    Serial.printf("Cannot create ticker id %d for row %d\n", id, row_id);
    return; // out of bounds
  }
  stop_ticker(id, false); // stop possible previous ticker
  if(len <= ll) {
    Serial.printf("No need to create ticker for: '%s'\n", s);
    setline(row_id, 0);
    puts(0, row_id, s, font); // show the line
  }
  else {
    w = f->u8FontWidth;
    datalen = len * w + 2 * SSD1306_LCDWIDTH; // reserve extra for nice flow
    Serial.printf("Alloc new text buffer: %d bytes\n", datalen + 1);
    TY.timestamp_prev  = _timestamp;
    TY.timestamp_speed = speed;
    TY.data = (uint8_t *)malloc(datalen + 1);
    memset(TY.data, 0, datalen);
    TY.scroll_delay  = scrolldelay;
    TY.delay_counter = scrolldelay;
    TY.idx = 0;
    TY.len = datalen - SSD1306_LCDWIDTH; // Including a full blank line
    uint8_t * mem = TY.data;
    for(int i = 0; i < len; i++) { // fill the data with graphic text
      if(s[i] >= f->u8FirstChar && s[i] <= f->u8LastChar)
        memcpy(mem, GET_FONT_DATA(f,s[i]), f->u8DataCols); 
      mem += w;
    }
    TY.row_id = row_id;
    memcpy(&TY.data[TY.len], TY.data, SSD1306_LCDWIDTH);
    memcpy(_lines[TY.row_id].row_data, TY.data, SSD1306_LCDWIDTH); // copy start of data to screen
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
    for(int i = 0; i < SSD1306_LCDWIDTH; i++) { 
      _lines[y].row_data[i] <<= 1;
      _lines[y].update = 1;
    }
  }
  else {
    if(y < _nr_rows - 1) { // Cannot shift last row
      for(int i = 0; i < SSD1306_LCDWIDTH; i++) {
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
      case 3: send_data(_lines[_rowcount].row_data,SSD1306_LCDWIDTH/2); // first half of line
              _state--;
              break;   
      case 2: send_data(&_lines[_rowcount].row_data[SSD1306_LCDWIDTH/2],SSD1306_LCDWIDTH/2); // second half of line
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
      send_data(_lines[_rowcount].row_data,SSD1306_LCDWIDTH); 
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
// Clears lines of the SSD1306ssd1306_command
// You may increase I2C_BUFFER_LENGTH in Wire.h 
// from 128 to at least 136 in order to write a whole line in one burst
// Location of Wire.h for ESP32 is:
// C:\Users\[user]\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.2\libraries\Wire\src
void ssd1306_class::clearlines(uint8_t y, uint8_t n) { 
  #if !SH1106_CONTROLLER
    send_command_d2(SSD1306_COLUMNADDR, 0, SSD1306_LCDWIDTH-1);
  #endif
  for (int yy=0; yy<n; yy++) {
    send_command(SSD1306_SH1106_SETPAGEADDRESS(y+yy));
    send_command(SSD1306_SH1106_SET_COL_LOW(_start_column)); // A3-A0
    send_command(SSD1306_SH1106_SET_COL_HIGH(0));            // A7-A4
    send_data((uint8_t)0,SSD1306_LCDWIDTH);
  }
}

#if SH1106_CONTROLLER
void ssd1306_class::putch(unsigned char c, font_id font) {
  if(font != SSD1306_FONT_UNCHANGED)
    _font = font;
  if(_x >= FONT_LINE_LENGTH) return;
  int col = _x * FONT_WIDTH + _x_offset + _start_column;
  int row = _y * FONT_DATA_ROWS;
  for(uint8_t r = 0; r < FONT_DATA_ROWS; r++) {
    send_command(SSD1306_SH1106_SETPAGEADDRESS(row+r));
    send_command(SSD1306_SH1106_SET_COL_LOW(col & 15));   // A3-A0
    send_command(SSD1306_SH1106_SET_COL_HIGH(col >> 4));  // A7-A4
    if(c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR) {
      send_data((uint8_t)0, FONT_WIDTH);
    }
    else {
      const uint8_t * p = &FONT_DATA[(c - FONT_FIRST_CHAR) * FONT_CHAR_SIZE + r * FONT_DATA_COLS];
      send_data(p, FONT_DATA_COLS);
      if(FONT_DATA_COLS < FONT_WIDTH && _x < FONT_LINE_LENGTH - 1)
        send_data((uint8_t)0, FONT_WIDTH - FONT_DATA_COLS);
    }
  }
  _x++; // advance cursor
}
#else

void ssd1306_class::putch(unsigned char c, font_id font) {
  if(font != SSD1306_FONT_UNCHANGED)
    _font = font;
  if(_x >= FONT_LINE_LENGTH) return;
  int col = _x * FONT_WIDTH + _x_offset + _start_column;
  int row = _y * FONT_DATA_ROWS;
  if(!_y_skip) 
    send_command_d2(SSD1306_PAGEADDR, row, row + FONT_DATA_LAST_ROW);
  if(c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR) {
    send_command_d2(SSD1306_COLUMNADDR, col, col + FONT_WIDTH - 1);
    send_data((uint8_t)0x0, FONT_WIDTH * FONT_DATA_ROWS);
  }
  else {
    send_command_d2(SSD1306_COLUMNADDR, col, col + FONT_DATA_LAST_COL);
    const uint8_t * p = &FONT_DATA[(c - FONT_FIRST_CHAR) * FONT_CHAR_SIZE];
    send_data(p, FONT_CHAR_SIZE);
    if(FONT_DATA_COLS < FONT_WIDTH && _x < FONT_LINE_LENGTH - 1) {
      send_command_d2(SSD1306_COLUMNADDR, col + FONT_DATA_COLS, col + FONT_WIDTH - 1);
      send_data((uint8_t)0x0, (FONT_WIDTH - FONT_DATA_COLS) * FONT_DATA_ROWS);
    }
  }
  _x++; // advance cursor
}
#endif

void ssd1306_class::puts(const char *s, uint8_t n, font_id font) {
  while(n && *s) { putch(*s++,font); _y_skip = 1; n--; }
  while(n--)     { putch(' ',font);  _y_skip = 1;      }
  _y_skip = 0;
}

#endif //OLED_USE_BUFFER
