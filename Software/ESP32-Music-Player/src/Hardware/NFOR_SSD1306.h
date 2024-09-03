
#ifndef _NFOR_SSD1306_H_
#define _NFOR_SSD1306_H_

#include <stdint.h>
#include <Wire.h>
#include "NFOR_SSD1306_fonts.h"
#include "Board.h"

/***************************
 * Screen definitions      *
 ***************************/

#define SSD1306_LCDWIDTH    128
#define SSD1306_LCDHEIGHT   64
#define SSD1306_LCDROWS     (SSD1306_LCDHEIGHT / 8)
#define SSD1306_LCDMEMSIZE  (SSD1306_LCDWIDTH * SSD1306_LCDROWS)

class NFOR_SSD1306 {
  public:
    NFOR_SSD1306(uint8_t rows = SSD1306_LCDROWS, uint8_t tickers = 1);
    ~NFOR_SSD1306();
    void init(TwoWire &wire, uint8_t flags = 0);
    uint8_t loop(bool tick); // 10 ms tick is recommended
    void display(void) { while(_state || check_rowupdate()) loop(_timestamp); }

    void setmem(uint8_t x, uint8_t y, uint8_t v, uint8_t n);
    void setline(uint8_t y, uint8_t v);
    void setscreen(uint8_t v);
    void clearscreen(void) { setscreen(0); }
    void putch(uint8_t c, uint8_t font);
    void puts_xy(uint8_t x, uint8_t y, const char *s, uint8_t font);
    void puts_xyn(uint8_t x, uint8_t y, const char *s, int n, uint8_t font);
    void start_ticker(uint8_t id, uint8_t row, const char *s, uint8_t font, uint16_t speed = 3, uint16_t scrolldelay = 50);
    void stop_ticker(uint8_t id, bool id_is_y = true);
    
    void set_x_offset(uint8_t offset)  { _x_offset = offset; }
    void shift_row_down(uint8_t y, uint8_t shift = 1);
    
    void ActivateLine(uint8_t id, bool active) {
      if(id < _nr_rows) {
        _lines[id].active = active;
        if(active) _lines[id].update = true;
      }
    }
    void SwapLines(uint8_t old_id, uint8_t new_id) { ActivateLine(old_id, false); ActivateLine(new_id, true); }
    void SetLineRow(uint8_t id, uint8_t row) { if(id < _nr_rows) _lines[id].row = row; }      
    
    void Print(void);  // For debugging
  
  protected:
    void wire_command(unsigned char c);
    void wire_command_d1(unsigned char c, unsigned char d1);
    void wire_command_d2(unsigned char c, unsigned char d1, unsigned char d2);
    void clearlines(int y, int n);
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
    } ssd1306_line_t;
    
    typedef struct {
      uint32_t timestamp_prev;
      uint16_t timestamp_speed;
      uint16_t scroll_delay;
      uint16_t delay_counter;
      uint16_t len;
      uint16_t idx;
      uint8_t  row_id; 
      uint8_t *data;
    } ssd1306_ticker_t;

    TwoWire *_wire;
    uint32_t _timestamp;
    uint8_t  _x;
    uint8_t  _y;
    uint8_t  _x_offset;
    uint8_t  _state;
    uint8_t  _colcount;
    uint8_t  _rowcount;
    uint8_t  _nr_rows;
    uint8_t  _tickercount;
    uint8_t  _nr_tickers;
    uint8_t  _start_column;  // 0 for SSD1306, 2 for SH1106  
 
    ssd1306_line_t   *_lines;   // Display line, may contain more than SSD1306_LCDROWS
    ssd1306_ticker_t *_tickers;
};

extern NFOR_SSD1306 OLED;

#endif // _NFOR_SSD1306_H_
