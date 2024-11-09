#ifndef _SSD1306_H_
#define _SSD1306_H_

#include <Wire.h>
#include <SPI.h>
#include "SSD1306_fonts.h"

#define SSD1306_LCDWIDTH     128
#define SSD1306_LCDHEIGHT     64
#define SSD1306_LCDROWS      (SSD1306_LCDHEIGHT / 8)

#define OLED_USE_BUFFER        1  // change to 0 for writing directly to module

#define OLED_UNKNOWN           0
#define OLED_SH1107            1
#define OLED_SH1106            2
#define OLED_SSD1306           3

#define OLED_INIT_NO_REGS_MASK    0x01  // Do not write OLED registers. (Usefull for making soft reboot unnoticed)
#define OLED_INIT_NO_ERASE_MASK   0x02  // Prevents erasing screen (usefull during soft reboot)
#define OLED_INIT_ROTATE180_MASK  0x04  // Rotates the screen by 180 degrees

#define font_id SSD1306_FontId_t

class ssd1306_class {
  public:
    class SoftSPI;
    ssd1306_class(TwoWire & wire) : ssd1306_class(&wire, NULL, NULL,0, 0, 0, 0) {}
    ssd1306_class(SPIClass & spi, uint8_t cs, uint8_t dc) : ssd1306_class(NULL, &spi, NULL, cs, dc, 0, 0) {}
    ssd1306_class(SoftSPI & softspi, uint8_t cs, uint8_t dc) : ssd1306_class(NULL, NULL, &softspi, cs, dc, 0, 0) {}
    #if OLED_USE_BUFFER
      ssd1306_class(TwoWire & wire, uint8_t rows, uint8_t tickers) : ssd1306_class(&wire, NULL, NULL,0, 0, rows, tickers) {}
      ssd1306_class(SPIClass & spi, uint8_t cs, uint8_t dc, uint8_t rows, uint8_t tickers) : ssd1306_class(NULL, &spi, NULL, cs, dc, rows, tickers) {}
      ssd1306_class(SoftSPI & softspi, uint8_t cs, uint8_t dc, uint8_t rows, uint8_t tickers) : ssd1306_class(NULL, NULL, &softspi, cs, dc, rows, tickers) {}
    #endif
    ~ssd1306_class();
    uint8_t initialize(uint8_t flags = 0, uint32_t speed = 0);
    void suspend(); // An open SPI port can interfere with other hardware (esp. Wifi), so suspend before initialisingn wifi
    void resume();  // and resume after wifi connects...
    void clearscreen();
    void progress_bar(uint8_t x, uint8_t y, uint8_t val, uint8_t max, bool double_row = false);
    void setfont(font_id font) { _font = font; }
    void putch(uint8_t c, font_id font = SSD1306_FONT_UNCHANGED);
    void puts(const char *s, uint8_t n, font_id font = SSD1306_FONT_UNCHANGED);
    void putch(uint8_t x, uint8_t y, unsigned char c, font_id font = SSD1306_FONT_UNCHANGED)         { _x = x; _y = y; putch(c,font); }
    void puts(uint8_t x, uint8_t y, const char *s, font_id font = SSD1306_FONT_UNCHANGED)            { puts(x,y,s,strlen(s),font);    }
    void puts(const char *s, font_id font = SSD1306_FONT_UNCHANGED)                                  { puts(s,strlen(s),font);        }
    void puts(uint8_t x, uint8_t y, const char *s, uint8_t n, font_id font = SSD1306_FONT_UNCHANGED) { _x = x; _y = y; puts(s,n,font);  }
    #if OLED_USE_BUFFER
      void clearbuffer(uint8_t v = 0);
      void start_ticker(uint8_t id, uint8_t row, const char *s, uint8_t font, uint16_t speed = 3, uint16_t scrolldelay = 50);
      void stop_ticker(uint8_t id, bool id_is_y = true);
      void setmem(uint8_t x, uint8_t y, uint8_t v, uint8_t n);
      void setmem(uint8_t x, uint8_t y, const uint8_t * v, uint8_t n);
      void setline(uint8_t y, uint8_t v) { setmem(0, y, v, SSD1306_LCDWIDTH); }
      void shift_row_down(uint8_t y, uint8_t shift = 1);
      void display(bool force_all = false);
      uint8_t loop(bool tick); // 10 ms tick is recommended
      void ActivateLine(uint8_t id, bool active) {
        if(id < _nr_rows) {
          _lines[id].active = active;
          if(active) _lines[id].update = true;
        }
      }
      void SwapLines(uint8_t old_id, uint8_t new_id) { ActivateLine(old_id, false); ActivateLine(new_id, true); }
      void SetLineRow(uint8_t id, uint8_t row) { if(id < _nr_rows) _lines[id].row = row; }  
      void Print();    
    #else // Some dummy routines to programming with less "#if OLED_USE_BUFFER"
      void clearbuffer(uint8_t v = 0) {}
      void display(bool force_all = false) {}
      void stop_ticker(uint8_t id, bool id_is_y = true) {}
    #endif
    void set_x_offset(uint8_t offset)  { _x_offset = offset; }

    class SoftSPI { 
      public:
        SoftSPI(uint8_t sck, uint8_t mosi) { _sck = sck; _mosi = mosi; }
        uint8_t _sck;
        uint8_t _mosi;
   } ;

  private:
    ssd1306_class(TwoWire * wire, SPIClass * spi, SoftSPI * softspi, uint8_t cs, uint8_t dc, uint8_t rows, uint8_t tickers);
    void hardware_clearscreen();
    void clearlines(uint8_t y, uint8_t n);
    void gotoxy(uint8_t x, uint8_t y);
    void send_command(uint8_t c);
    void send_command_d1(uint8_t c, uint8_t d1);
    void send_command_d2(uint8_t c, uint8_t d1, uint8_t d2);
    void send_data(uint8_t dat , uint16_t len = 1);
    void send_data(const uint8_t * dat, uint16_t len);
    #if OLED_USE_BUFFER
      void update_ticker(uint8_t y);
      bool check_rowupdate(void) {
        for(int i = 0; i < _nr_rows; i++)
          if(_lines[i].active && _lines[i].update) return true;
        return false;
      }

      typedef struct {
        uint8_t  row;     // Row where to display, should in in range 0 .. SSD1306_LCDROWS-1
        uint8_t  active;  // 0 = row will NOT be displayed, 1 = row will be displayed
        uint8_t  update;  // 0 = is sent to display, 1 = is queued for being displayed
        uint8_t  row_data[SSD1306_LCDWIDTH];
      } line_t;
    
      typedef struct {
        uint32_t timestamp_prev;
        uint16_t timestamp_speed;
        uint16_t scroll_delay;
        uint16_t delay_counter;
        uint16_t len;
        uint16_t idx;
        uint8_t  row_id; 
        uint8_t *data;
      } ticker_t;
    #endif

    TwoWire *_wire;
    SPIClass*_spi;
    bool     _softspi;
    uint8_t  _device;
    uint32_t _speed;
    uint8_t  _x, _y;
    uint8_t  _cs, _dc;
    uint8_t  _softsck;
    uint8_t  _softmosi;
    uint8_t  _x_offset;
    uint8_t  _start_column;  // 0 for SSD1306, 2 for SH1106
    font_id  _font;

    #if OLED_USE_BUFFER
      uint32_t  _timestamp;
      uint8_t   _state;
      uint8_t   _colcount;
      uint8_t   _rowcount;
      uint8_t   _nr_rows;
      uint8_t   _tickercount;
      uint8_t   _nr_tickers;
      line_t   *_lines;   // Display line, may contain more than SSD1306_LCDROWS
      ticker_t *_tickers;
    #else
      uint8_t   _y_skip;
    #endif
};

#undef font_id

extern ssd1306_class OLED;

#endif // _SSD1306_H_
