
#include "NFOR_SSD1306.h"

//=====================================================================
//== Converted from IAR ARM to EPS32 Arduino                         ==
//== Assumes wire buffer lenght of 128 bytes or higher               ==
//=====================================================================
/************************************************************************
FILE: NFOR_SSD1306.cpp
DESC: SSD1306 OLED display driver for ESP32. Driver uses display
      buffer, which will be transferred to the physical display in
      subsequent calls to loop(). Hence, loop() must be called as
      often as possible
BY:   OMT
************************************************************************/

#if !defined(ESP32)
  #warning: This code is for ESP32. Please check for usability on other targets
#endif

#define SUPPORT_LH1106   1

#include <arduino.h>
#include <wire.h>

#include "NFOR_SSD1306_fonts.h"
#include "NFOR_SSD1306.h"

//************************************************************************//
// SSD1306 hardware driver                                                //
//************************************************************************//

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
#define SSD1306_SETLOWCOLUMN        0x00
#define SSD1306_SETHIGHCOLUMN       0x10
#define SSD1306_SETSTARTLINE        0x40
#define SSD1306_MEMORYMODE          0x20
#define SSD1306_COLUMNADDR          0x21
#define SSD1306_PAGEADDR            0x22
#define SSD1306_COMSCANINC          0xC0
#define SSD1306_COMSCANDEC          0xC8
#define SSD1306_SEGREMAP            0xA0
#define SSD1306_CHARGEPUMP          0x8D
#define SSD1306_PAGESTARTADDR       0xB0

#define SSD1306_EXTERNALVCC         0x1
#define SSD1306_SWITCHCAPVCC        0x2

#define SSD1306_I2C_ADDRESS         0x3C  // on PIC: 0x78

///////////////////////////////////////////////////////////////////
// Low level (private) proutines

// Arduino doesn't allow large packets, so break down in smaller ones
void NFOR_SSD1306::clearlines(int y, int n) { // Break down in half lines
  int xx,yy;
  for (yy=0; yy<8; yy++) { // This works on LH1106 too
    wire_command(SSD1306_PAGESTARTADDR + y + yy);
    wire_command(SSD1306_SETLOWCOLUMN + _start_column); // A3-A0
    wire_command(SSD1306_SETHIGHCOLUMN);                // A7-A4
    _wire->beginTransmission(SSD1306_I2C_ADDRESS);
    _wire->write(0X40); // data not command
    #if I2C_BUFFER_LENGTH < 136 
      for(xx=0; xx < SSD1306_LCDWIDTH/2; xx++)  // Break down in half lines
        _wire->write(0);
      _wire->endTransmission();
      _wire->beginTransmission(SSD1306_I2C_ADDRESS);
      _wire->write(0X40); // data not command
      for(xx=0; xx < SSD1306_LCDWIDTH/2; xx++) 
        _wire->write(0);
    #else
      for(xx=0; xx < SSD1306_LCDWIDTH; xx++)
        _wire->write(0);
    #endif
    _wire->endTransmission(); 
  }
}

void NFOR_SSD1306::wire_command(unsigned char c) {
  _wire->beginTransmission(SSD1306_I2C_ADDRESS);
  _wire->write(0X00); // This is Command
  _wire->write(c);
  _wire->endTransmission();
}

void NFOR_SSD1306::wire_command_d1(unsigned char c, unsigned char d1) {
  wire_command(c);
  wire_command(d1);
}

void NFOR_SSD1306::wire_command_d2(unsigned char c, unsigned char d1, unsigned char d2) {
  wire_command(c);
  wire_command(d1);
  wire_command(d2);
}

/**********************************************************************************************************************************/

NFOR_SSD1306::NFOR_SSD1306(uint8_t rows, uint8_t tickers) {
  memset(this, 0, sizeof(NFOR_SSD1306));
  _nr_rows    = rows;
  _nr_tickers = tickers;
  _lines   = (ssd1306_line_t   *)malloc(rows    * sizeof(ssd1306_line_t));
  _tickers = (ssd1306_ticker_t *)malloc(tickers * sizeof(ssd1306_ticker_t));
  memset(_lines, 0, rows * sizeof(ssd1306_line_t));
  memset(_tickers, 0, tickers * sizeof(ssd1306_ticker_t));
  for(int i = 0; i < _nr_rows && i < SSD1306_LCDROWS; i++) {
    _lines[i].row    = i; 
    _lines[i].active = 1;
  }
  //_lines[0].row = 2;  // test swapping rows
  //_lines[2].row = 0;
  _start_column = 0; 
}

NFOR_SSD1306::~NFOR_SSD1306() {
  free(_lines);
  for(int i = 0; i < _nr_tickers; i++) {
    if(_tickers->data != NULL) free(_tickers->data);
  } 
  free(_tickers); 
}

void NFOR_SSD1306::init(TwoWire &wire, uint8_t flags) {  // I2C (Wire) port is expected to be properly initiazed
  _wire = &wire;
  _wire->setBufferSize(256); // Default size (128 on ESP32) is not enough for one data line.
  #if SUPPORT_LH1106
    // check for SH1106 controller 
    _wire->beginTransmission(SSD1306_I2C_ADDRESS);
    _wire->write(0);
    if(_wire->endTransmission() != 0)
       return; // Device not found
    _wire->requestFrom(0x3C, 1);
    uint8_t c = _wire->read() & 0x0F;   // Receive a byte as character and mask off power on/off bit
    if(c == 0x8) {
      _start_column = 2; // Needed for LH1106
    }
  #endif
  
  if(flags & 1) return;
  delay(100);
  // Init sequence for 128x64 OLED module
  wire_command(SSD1306_DISPLAYOFF);                  // 0xAE
  wire_command_d1(SSD1306_SETDISPLAYCLOCKDIV, 0x80); // 0xD5, the suggested ratio 0x80
  wire_command_d1(SSD1306_SETMULTIPLEX, 0x3F);       // 0xA8
  wire_command_d1(SSD1306_SETDISPLAYOFFSET, 0x00);   // 0xD3, no offset
  wire_command(SSD1306_SETSTARTLINE);                // line #0
  wire_command_d1(SSD1306_CHARGEPUMP, 0x14);         // 0x8D, using internal VCC
  wire_command_d1(SSD1306_MEMORYMODE, 0x00);         // 0x20, 0x00 horizontal addressing
  if(flags & 4) {
    wire_command(SSD1306_SEGREMAP | 0x1);            // rotate screen 180
    wire_command(SSD1306_COMSCANDEC);                // rotate screen 180
  }
  else {
    wire_command(SSD1306_SEGREMAP);                  // rotate screen 0
    wire_command(SSD1306_COMSCANINC);                // rotate screen 0
  }
  wire_command_d1(SSD1306_SETCOMPINS, 0x12);         // 0xDA
  wire_command_d1(SSD1306_SETCONTRAST, 0xFF);        // 0x81, max contrast
  wire_command_d1(SSD1306_SETPRECHARGE, 0xF1);       // 0xd9
  wire_command_d1(SSD1306_SETVCOMDETECT, 0x40);      // 0xDB
  wire_command(SSD1306_DISPLAYALLON_RESUME);         // 0xA4
  wire_command(SSD1306_NORMALDISPLAY);               // 0xA6
  wire_command(SSD1306_DISPLAYON);                   // switch on OLED
  delay(1);
  wire_command(SSD1306_SCROLL_OFF);
  _state    = 0;
  _rowcount = 0;
  _colcount = 0;
  if(flags & 2) // Erase in case callback wil not be called soon
    clearlines(0, SSD1306_LCDROWS);
  setscreen(0);
}  

#define LOOP_START  6

uint8_t NFOR_SSD1306::loop(bool tick) {
  if(tick) _timestamp++;
  switch(_state) {
    case 6: wire_command(SSD1306_SETLOWCOLUMN + _start_column); // A3-A0
            //Serial.printf("SSD1306 row: %u\n", _lines[_rowcount].row);
            _state--;
            break;
    case 5: wire_command(SSD1306_SETHIGHCOLUMN);                // A7-A4
            _state--;
            break;
    case 4: wire_command(SSD1306_PAGESTARTADDR + _lines[_rowcount].row);
           _state--;
            break;
    case 3: _wire->beginTransmission(SSD1306_I2C_ADDRESS);
            _wire->write(0X40); // data not command 
            for(_colcount = 0; _colcount < SSD1306_LCDWIDTH/2; _colcount++) // first half of line
              _wire->write(_lines[_rowcount].row_data[_colcount]);
            _wire->endTransmission(); 
            _state--;
            break;   
    case 2: _wire->beginTransmission(SSD1306_I2C_ADDRESS);
            _wire->write(0X40); // data not command 
            for(; _colcount < SSD1306_LCDWIDTH; _colcount++) // second half of line
              _wire->write(_lines[_rowcount].row_data[_colcount]);
            _wire->endTransmission(); 
            _state = 1;//--;
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
  return 0;
}

void NFOR_SSD1306::setmem(uint8_t x, uint8_t y, uint8_t v, uint8_t n) {
  memset(&_lines[y].row_data[x], v, n);
  _lines[y].update = 1;
}

void NFOR_SSD1306::setmem(uint8_t x, uint8_t y, const uint8_t *v, uint8_t n) {
  memcpy(&_lines[y].row_data[x], v, n);
  _lines[y].update = 1;
}

void NFOR_SSD1306::setline(uint8_t y, uint8_t v) {
  memset(&_lines[y].row_data, v, SSD1306_LCDWIDTH);
  _lines[y].update   = 1;
}

void NFOR_SSD1306::setscreen(uint8_t v) {
  int i;
  _x        = 0;
  _y        = 0;
  _x_offset = 0;
  for(i = 0; i < SSD1306_LCDROWS; i++) {
    setline(i, v); 
  }
}

///////////////////////////////////////////////////////////////////
// Writes a single character to the OLED buffer

#define GET_FONT_DATA(c) (f->au8FontTable + (c - f->u8FirstChar) * f->u8BytesPerChar)

void NFOR_SSD1306::putch(uint8_t c, uint8_t font) {
  const SSD1306_Font_t *f = &SSD1306_Fonts[font];
  int col, w, y, i, j;
  unsigned char * p;

  w   = f->u8FontWidth;
  y   = _y * f->u8DataRows;
  col = _x_offset ? _x_offset : _x * w;
  if(col + w > SSD1306_LCDWIDTH)
    w = f->u8DataCols; // supress blank pixels after last character of line
  p = _lines[y].row_data;
  if(c < f->u8FirstChar || c > f->u8LastChar) {
    for(i = 0; i < f->u8DataRows; i++) {
      _lines[y+i].update = 1;
      for(j = 0; j < w; j++)
        p[col+j] = 0;
      p += SSD1306_LCDWIDTH;
    }
  }
  else {
    const unsigned char * fd = GET_FONT_DATA(c);
    for(i = 0; i < f->u8DataRows; i++) {
      _lines[y+i].update = 1;
      for(j = 0; j < f->u8DataCols; j++)
        p[col+j] = fd[i * f->u8DataCols + j];
      for(; j < w; j++)
        p[col+j] = 0;
      p += SSD1306_LCDWIDTH;
    }
  }
  if(_x_offset)
    _x_offset += w;
  else
    _x++; // advance cursor
}

void NFOR_SSD1306::puts_xy(uint8_t x, uint8_t y, const char *s, uint8_t font) {
  stop_ticker(y); // If a ticker is running on this line, stop it
  _x = x;
  _y = y;
  
  while(*s) putch(*s++, font);
}

void NFOR_SSD1306::puts_xyn(uint8_t x, uint8_t y, const char *s, int n, uint8_t font) {
  stop_ticker(y); // If a ticker is running on this line, stop it
  _x = x;
  _y = y;
  while(n && *s) { putch(*s++, font); n--; }
  while(n--)     { putch(' ', font); }
}

//=====================================================================
// News Ticker Routines
//=====================================================================

#define TY _tickers[id]

void NFOR_SSD1306::update_ticker(uint8_t id) {
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

void NFOR_SSD1306::start_ticker(uint8_t id, uint8_t row_id, const char *s, uint8_t font, uint16_t speed, uint16_t scrolldelay) {
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
    puts_xy(0, row_id, s, font); // show the line
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
        memcpy(mem, GET_FONT_DATA(s[i]), f->u8DataCols); 
      mem += w;
    }
    TY.row_id = row_id;
    memcpy(&TY.data[TY.len], TY.data, SSD1306_LCDWIDTH);
    memcpy(_lines[TY.row_id].row_data, TY.data, SSD1306_LCDWIDTH); // copy start of data to screen
    _lines[TY.row_id].update = 1;
  }
}

void NFOR_SSD1306::stop_ticker(uint8_t id, bool id_is_y) {
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

void NFOR_SSD1306::shift_row_down(uint8_t y, uint8_t shift) {
  if(shift == 0 || shift > 7) return; // illegal shift
//Serial.printf("********************* NFOR_SSD1306: shift_row_down = %d with %d\n", y, shift);
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

void NFOR_SSD1306::Print(void) {
  Serial.printf("OLED: width=%d, height=%d, text lines=%d, mem size=%d\n", SSD1306_LCDWIDTH,SSD1306_LCDHEIGHT,SSD1306_LCDROWS,SSD1306_LCDMEMSIZE);
  Serial.printf("      buffer lines=%d, buffer memory=%d\n", _nr_rows, _nr_rows * sizeof(ssd1306_line_t));
  Serial.printf("      tickers=%d, ticker memory=%d\n", _nr_tickers, _nr_tickers * sizeof(ssd1306_ticker_t));

  Serial.println("Buffer configuration");
  for(int i = 0; i <_nr_rows; i++) 
    Serial.printf("Line %d, row %d, active %d, update %d\n",i, _lines[i].row, _lines[i].active, _lines[i].update);

  Serial.println("Ticker configuration");
  for(int i = 0; i <_nr_tickers; i++) 
    Serial.printf("ID %d, row_id %d, scroll_delay %d, delay_counter %d, len %d, idx %d\n", i, _tickers[i].row_id, 
                   _tickers[i].scroll_delay, _tickers[i].delay_counter, _tickers[i].len, _tickers[i].idx);
}

