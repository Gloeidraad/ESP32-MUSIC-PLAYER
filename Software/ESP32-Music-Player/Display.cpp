// Remark:  Please check in "Hardware/Board.h" that:
// I2C_PORT_OLED is defined when you are using the I2C version of the OLED display.
// Doe not define I2C_PORT_OLED (comment out the define) when using the SPI version.

#include "Display.h"

#include "src/Version.h"
#include "src/Hardware/SSD1306.h"
#include "src/settings.h"
#include "src/I2C_Scanner.h"
#include "SimpleWaveGenerator.h"

#define FONT_LARGE_DEFAULT  SSD1306_FONT_8X8_PROP
#define FONT_SMALL_DEFAULT  SSD1306_FONT_5X8_PROP

//******************************************************//
// Local funtions & Constants                           //
//******************************************************//

#define D_STRING static const char * const

D_STRING s_Help[] = {
  "",
  "WiFi Connect failed!",
  "WiFi Connected!",
  "Press \"Play\" to start",
  "Please Wait.....",
  "Initializing Webradio",
  "Reconnect Webradio",
  "Card Failure",
  "No Tracks Found",
  "Cannot connect to Wifi network: ", 
  "No Web Stations Found",                       
};

static void SerialPrint(const char *s1, const char *s2) {
  if(s1) Serial.print(s1);
  if(s2) Serial.println(s2);
}

static int PlayTimeToString(char * s, unsigned sec) {
  unsigned hrs = sec / 3600;
  sec          = sec - hrs * 3600;
  unsigned min = sec / 60;
  sec          = sec - min * 60;
  if(hrs)
    return sprintf(s, "%d:%02d:%02d", (int)hrs, (int)min, (int)sec);
  return sprintf(s, "%d:%02d", (int)min, (int)sec);
}

//******************************************************//
// Private funtions                                     //
//******************************************************//

//#define TRACK_TEST  "23/45"
//#define TRACK_TEST  "23/456"
//#define TRACK_TEST  "123/456"
//#define TRACK_TEST  "234/5678"
//#define TRACK_TEST  "1234/5678"

void DisplayClass::MakeTrackString(bool web) {
  #ifdef TRACK_TEST
    strcpy(_track_string, TRACK_TEST);
    _track_string_len = strlen(TRACK_TEST);
  #else
    if(web)
      _track_string_len = sprintf(_track_string, "%d/%d",Settings.NV().WebRadioCurrentStation,Settings.NV().WebRadioTotalStations);
    else
      _track_string_len = sprintf(_track_string, "%d/%d",Settings.NV().DiskCurrentTrack,Settings.NV().DiskTotalTracks);
  #endif
}

//#define CURRENT_TIME_TEST  "33:12"
//#define CURRENT_TIME_TEST  "5:44:12"
//#define CURRENT_TIME_TEST  "25:44:12"
//#define TRACK_TIME_TEST    "33:45"
//#define TRACK_TIME_TEST    "6:33:45"
//#define TRACK_TIME_TEST    "26:33:45/28:45:56"

uint16_t DisplayClass::MakeTimeString(char * s, uint16_t s_len, uint32_t time, uint32_t totaltime) {
  uint16_t len;
  char     tt[12];
  #ifdef CURRENT_TIME_TEST
    strcpy(s, CURRENT_TIME_TEST);
    len = strlen(CURRENT_TIME_TEST);
  #else
    len = PlayTimeToString(tt, time);
    if(len > s_len) return 0;
    strcpy(s, tt);
    if(totaltime == 0xFFFF || len >= s_len) 
      return len;
    s[len++] = '/';
    s[len]   = 0;
    uint16_t tt_l = PlayTimeToString(tt, totaltime);
    if(len + tt_l <= s_len) {
      strcpy(s+len, tt);
      len += tt_l;
    }
  #endif
  return len;
}

void DisplayClass::MakeTimeString(void) {
    _time_string_len = MakeTimeString(_time_string, sizeof(_time_string), Settings.V().CurrentTrackTime, Settings.V().TotalTrackTime);
}

void DisplayClass::SetTrackString(const char *s) { strcpy(_track_string, s); _track_string_len = strlen(s); }
void DisplayClass::SetTimeString(const char *s)  { strcpy(_time_string, s);  _time_string_len  = strlen(s); }

#ifdef _SSD1306_H_

  #define TICKER_ID           0

  #define TRACK_LINE          0
  #define TIME_LINE           TRACK_LINE
  #define TICKER_TOP_LINE     1
  #define TICKER_LINE         2
  #define TICKER_BOTTOM_LINE  3
  #define PLAYER_NAME_LINE    4
  #define PLAYER_PLAY_LINE    5
  #define STATUS_LINE1        6
  #define STATUS_LINE2        7

  #define MENU_LINE_2_SHIFT   4
  #define MENU_LINE_4_SHIFT 
  #define MENU_LINE_1         8
  #define MENU_LINE_2         9
  #define MENU_LINE_3         10
  #define MENU_LINE_4         11

  ssd1306_class OLED(OLED_LCDROWS + 4, 2); // Four extra rows for menus, two tickers

                          //0        1     1   2
                          //123456789012345678901
  D_STRING s_Welcome[] = { "  Welcome to",
                           "  ESP32",
                           "  Music Player",
                           SOFTWARE_VERSION SOFTWARE_SUBVERSION
                         }; 

//******************************************************//
// Public funtions                                      //
//******************************************************//

DisplayClass::DisplayClass() { 
 _track_string[0]  = '\0';
 _time_string[0]   = '\0';
 _track_string_len = 0;
 _time_string_len  = 0;
}

void DisplayClass::Initialize(bool reboot, bool use_spi) {
  if(use_spi) {
    #ifdef SPI_PORT_OLED
      OLED.setport(SPI_PORT_OLED, PIN_CS, PIN_DC);
      OLED.initialize(reboot ? OLED_INIT_NO_REGS_MASK : SPI_ROTATE_DISPLAY, SPI_OLED_CLOCK_SPEED, true);
    #endif
  }
  else {
    #ifdef I2C_PORT_OLED
      OLED.setport(I2C_PORT_OLED);
      OLED.initialize(reboot ? OLED_INIT_NO_REGS_MASK : I2C_ROTATE_DISPLAY, I2C_HIGH_CLOCK_SPEED, true);
    #endif
  }
  OLED.SetLineRow(MENU_LINE_1, PLAYER_NAME_LINE);
  OLED.SetLineRow(MENU_LINE_2, PLAYER_PLAY_LINE);
  OLED.SetLineRow(MENU_LINE_3, STATUS_LINE1);
  OLED.SetLineRow(MENU_LINE_4, STATUS_LINE2);
}

void DisplayClass::Suspend() { OLED.suspend(); }
void DisplayClass::Resume()  { OLED.resume();  }

void DisplayClass::ShowWelcome(void) {
  //int x;
  OLED.puts(0, 1, s_Welcome[0], FONT_LARGE_DEFAULT);
  OLED.puts(0, 3, s_Welcome[1], FONT_LARGE_DEFAULT);
  OLED.puts(0, 5, s_Welcome[2], FONT_LARGE_DEFAULT);
  OLED.puts_right_aligned(7, s_Welcome[3], FONT_SMALL_DEFAULT);
  OLED.display(); // Force display
}

void DisplayClass::ShowPlayPause(bool play, bool no_queue) {
  ShowLine(PLAYER_PLAY_LINE, play ? "Playing" : "Paused", FONT_LARGE_DEFAULT, no_queue);
}
void DisplayClass::ShowBaseScreen(const char * help_line) {
  Serial.printf("ShowBaseScreen: %s\n", help_line ? help_line : "[NULL]");
  OLED.display(); // Empty display queue
  OLED.setline(TRACK_LINE, 0);
  OLED.setline(TICKER_TOP_LINE, 0x08);
  ShowHelpLine(help_line == NULL ? s_Help[DISPLAY_HELP_PLEASE_WAIT] : help_line);
  OLED.setline(TICKER_BOTTOM_LINE, 0x10);
  ShowLine(PLAYER_NAME_LINE, Settings.GetSourceName(), FONT_LARGE_DEFAULT); 
  switch(Settings.NV().SourceAF) {
    case SET_SOURCE_SD_CARD   :  ShowTrackNumber();    break;
    case SET_SOURCE_WEB_RADIO :  ShowWebRadioNumber(); break;
    case SET_SOURCE_WAVE_GEN  :  ShowWaveformId();     break;
    case SET_SOURCE_BLUETOOTH :  ShowBluetoothId();    break;
  }
  ShowPlayPause(Settings.V().Play);
  //ShowFrequencies();
  ShowVolume();
  OLED.display(); // Force display
}

void DisplayClass::ShowBaseScreen(uint8_t help_id) { ShowBaseScreen(s_Help[help_id]); }

void DisplayClass::ShowLine(uint8_t y, const char *s, uint8_t font, bool no_queue) {
  if(no_queue) OLED.display(); // Empty display queue
  OLED.setline(y, 0);
  OLED.puts(0, y, s, OLED.getValidStringLength(s, (ssd1306_font_id_t)font), (ssd1306_font_id_t)font);
  if(no_queue) OLED.display(); 
}

void DisplayClass::ShowHelpLine(const char * s, bool no_queue)  {
  ShowLine(TICKER_LINE, s, FONT_SMALL_DEFAULT, no_queue);
}

void DisplayClass::MakeTrackString(uint16_t curtrack, uint16_t totaltracks) {
  _track_string_len = sprintf(_track_string, "%d/%d", curtrack, totaltracks);
  Serial.printf("New track number: %s\n", _track_string); // Same line as track time, so don't display here
}

void DisplayClass::ShowWebRadioNumber(void) {
  if(Settings.NV().SourceAF == SET_SOURCE_WEB_RADIO) {
    MakeTrackString(true);
    SetTimeString("-:--");
    ShowTrackNumberAndPlayTime(true);
  }
}

void DisplayClass::ShowTrackTitle(const char * title, const char * upto, bool no_queue) {
  String temp = title;
  temp.toLowerCase();
  int16_t i = temp.lastIndexOf(upto);
  temp = title; // restore upper case characters (if applicable)
  if (i > 0) temp.remove(i);
  OLED.start_ticker(TICKER_ID, TICKER_LINE, temp.c_str(), FONT_LARGE_DEFAULT);
}

void DisplayClass::ShowStationName(const char *s, bool no_queue) {
  ShowLine(TICKER_LINE, s, FONT_LARGE_DEFAULT, no_queue);
}

void DisplayClass::ShowHelpLine(uint8_t help_id, bool no_queue) {
  ShowHelpLine(s_Help[help_id], no_queue);
}

void DisplayClass::ShowTrackNumberAndPlayTime(bool no_queue) {
  int len;
  ssd1306_font_id_t f_id;

  if(no_queue) OLED.display(); // Empty display queue
  OLED.setline(TRACK_LINE, 0);
  len = _track_string_len + _time_string_len + 1;
  if(len <= 18)       f_id = SSD1306_FONT_6X10_NUM;
  else if (len <= 21) f_id = SSD1306_FONT_5X10_NUM; 
  else                f_id = SSD1306_FONT_4X10_NUM;
  OLED.setfontid(f_id, true);
  OLED.puts(0,TRACK_LINE, _track_string, f_id);
  OLED.puts_right_aligned(TIME_LINE, _time_string, f_id);
  if(no_queue) OLED.display(); 
}

void DisplayClass::ShowBluetoothId(void) {
  SetTrackString("1/1"); 
  SetTimeString("-:--");
  ShowTrackNumberAndPlayTime(true);
}

void DisplayClass::ShowWaveformId(void) {
  sprintf(_track_string, "%d/%d", Settings.NV().WaveformId+1, SET_WAVE_FORM_MAX+1);
  _track_string_len = strlen(_track_string);
  SetTimeString("0:00");
  ShowTrackNumberAndPlayTime(true);
}

#define TEXT_VOLUME_X 68 // So it will be displayed jest left of the volume bar
#define TEXT_VOL_X    86 // So it will be displayed jest left of the volume bar

void DisplayClass::ShowVolume(uint8_t vol, uint8_t max) {
  const char *volume_text = "Volume";
  uint16_t    volume_px   = TEXT_VOLUME_X;
  OLED.setline(STATUS_LINE1, 0); 
  OLED.setline(STATUS_LINE2, 0);
  OLED.gotoy(STATUS_LINE2);
  if(Settings.NV().SourceAF == SET_SOURCE_WEB_RADIO && Settings.NV().WebRadioTotalStations > 0) {
    web_station_t web_station;
    uint16_t name_width;
    UrlSettings.GetStation(Settings.NV().WebRadioCurrentStation, web_station);
    OLED.puts(0, STATUS_LINE2, web_station.name, FONT_SMALL_DEFAULT);
    name_width = OLED.getStringWidth(web_station.name, FONT_SMALL_DEFAULT);
    if(name_width > TEXT_VOLUME_X - 5) {
      volume_text = name_width > TEXT_VOLUME_X ? "" : "Vol";
      volume_px   = TEXT_VOL_X;
    }
  }
  OLED.gotox(volume_px, true);
  OLED.puts(volume_text, FONT_SMALL_DEFAULT);
  ShowVolumeBar(vol, max);
  OLED.shift_row_down(STATUS_LINE2);
}

void DisplayClass::ShowVolume() {
  ShowVolume(Settings.NV().Volume, Settings.NV().VolumeSteps);
}

void DisplayClass::ShowVolumeBar(uint8_t vol, uint8_t max) {
  uint8_t bar[max+2];
  for(int i = 0; i <= max; i++) {
    bar[i] = vol >= i ? 0x7F : 0x41;
  }
  bar[max+1] = 0x7F;
  OLED.setmem(OLED_LCDWIDTH - max - 3, STATUS_LINE2, bar, max+2);
}

void DisplayClass::ShowTrackNumber(void) {
  MakeTrackString(false);
  //if(Settings.V().CurrentTrackTime > 0) { // OMT: quick and dirty, needs rework
  if(Settings.V().TotalTrackTime > 0) { // OMT: quick and dirty, needs rework
    Settings.V().TotalTrackTime = Settings.NV().DiskTrackTotalTime;
    MakeTimeString();
  }
  else
    SetTimeString("-:--/-:--");
  ShowTrackNumberAndPlayTime(true);
}

void DisplayClass::ShowWebRadioConnect(bool OK) {
  char s[80];
  strcpy(s, s_Help[DISPLAY_HELP_WIFI_CONNECT_FAILED_ID]);
  strcat(s, SsidSettings.GetSsid(Settings.NV().CurrentSsid, false));
  OLED.start_ticker(TICKER_ID, TICKER_LINE, s, FONT_LARGE_DEFAULT);
}

void DisplayClass::ShowPlayTime(bool no_queue) {
  MakeTimeString();
  ShowTrackNumberAndPlayTime(no_queue);
}

void DisplayClass::ShowPlayTime(uint32_t time, bool no_queue) {
  PlayTimeToString(_time_string, time);
  _time_string_len = strlen(_time_string);
  ShowTrackNumberAndPlayTime(no_queue);
}

void DisplayClass::ShowPlayTime(uint32_t curtime, uint32_t totaltime, bool no_queue) {
  PlayTimeToString(_time_string, curtime);
  strcat(_time_string, "/");
  PlayTimeToString(&_time_string[strlen(_time_string)], totaltime);
  _time_string_len = strlen(_time_string);
  ShowTrackNumberAndPlayTime(no_queue);
}

void DisplayClass::loop(uint32_t timestamp) { OLED.loop(timestamp); }

void DisplayClass::PrepareMenuScreen(bool menu_on) {
  if(menu_on) {
    OLED.SwapLines(PLAYER_NAME_LINE, MENU_LINE_1);
    OLED.SwapLines(PLAYER_PLAY_LINE, MENU_LINE_2);
    OLED.SwapLines(STATUS_LINE1,     MENU_LINE_3);
    OLED.SwapLines(STATUS_LINE2,     MENU_LINE_4);
  }
  else {
    OLED.SwapLines(MENU_LINE_1, PLAYER_NAME_LINE);
    OLED.SwapLines(MENU_LINE_2, PLAYER_PLAY_LINE);
    OLED.SwapLines(MENU_LINE_3, STATUS_LINE1);
    OLED.SwapLines(MENU_LINE_4, STATUS_LINE2);
  }
}

#if 0
void DisplayClass::ShowPlayer(void) {
  OLED.display(); // Empty display queue
  OLED.puts(0, PLAYER_NAME_LINE, Settings.GetSourceName(), FONT_LARGE_DEFAULT); 
  OLED.display(); // Force display
}

void  DisplayClass::ShowReboot(void) {
  ShowBaseScreen(s_Help[DISPLAY_HELP_PLEASE_WAIT]);
}

void DisplayClass::ShowBluetoothName(void) {
  OLED.start_ticker(TICKER_ID, TICKER_LINE, (const char *)Settings.NV().BtName, FONT_SMALL_DEFAULT);
}

void DisplayClass::ShowTrackName(const char * nam, bool no_queue) {
  ShowLine(TICKER_LINE, nam, FONT_LARGE_DEFAULT, no_queue);
}

void DisplayClass::ShowWaveformName(const char * id) {
  OLED.setline(TICKER_LINE, 0);
  OLED.puts(0, TICKER_LINE, id, FONT_LARGE_DEFAULT);
}

void DisplayClass::ShowStreamTitle(const char *s) {
  Serial.print("Streamtitle: ");
  if(strlen(s) > 0) {
    Serial.println(s);
    Display.ShowTrackTitle(s);
  }
  else {
    web_station_t web_station;
    UrlSettings.GetStation(Settings.NV().WebRadioCurrentStation, web_station);
    Display.ShowTrackTitle(web_station.name);
    Serial.print(web_station.name);
  }
}

void DisplayClass::ShowLastHost(const char *s)    { SerialPrint("Lasthost    ", s);     }
void DisplayClass::ShowStation(const char *s)     { SerialPrint("Station     ", s);     }
void DisplayClass::ShowTrackEnded(const char *s)  { SerialPrint("EOF MP3     ", s);     }
void DisplayClass::ShowStreamEnded(const char *s) { SerialPrint("audio_eof_stream", s); }
#endif

void DisplayClass::ShowMenuString(uint8_t y, const char *s, uint8_t font) {
  if(s != NULL) {
    OLED.setline(y, 0); 
    OLED.puts(0, y, s, ssd1306_font_id_t(font));
  }
}

void DisplayClass::ShowMenuStrings(const char *s1, const char *s2, const char *s3) {
  ShowMenuString(MENU_LINE_1, s1, FONT_LARGE_DEFAULT);
  
  if(s2 != NULL) {
    if(OLED.getStringWidth(s2, FONT_LARGE_DEFAULT) < OLED_LCDWIDTH) ShowMenuString(MENU_LINE_2, s2, FONT_LARGE_DEFAULT);
    else {
      OLED.setline(MENU_LINE_2,0);
      OLED.puts(0, MENU_LINE_2, s2, OLED.getValidStringLength(s2, FONT_SMALL_DEFAULT), FONT_SMALL_DEFAULT);
    }
  }
#ifndef  MENU_LINE_2_SHIFT
  OLED.setline(MENU_LINE_3, 0);
#else // Shift a bit for nicer display
  if(s2 != NULL) {
    OLED.shift_row_down(MENU_LINE_2, MENU_LINE_2_SHIFT); // Shift a bit for nicer display
  }
#endif
  ShowMenuString(MENU_LINE_4, s3, FONT_SMALL_DEFAULT);
}

#else 

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write your own display interface here
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DisplayClass::DisplayClass() { }
void DisplayClass::Initialize(bool reboot, bool use_spi) {}
void DisplayClass::ShowWelcome(void) {}
void DisplayClass::ShowBaseScreen(const char * help_line) {}
void DisplayClass::ShowBaseScreen(uint8_t help_id)  { ShowBaseScreen(s_Help[help_id]); }

void DisplayClass::ShowMenuStrings(char const*, char const*, char const*) {}
void DisplayClass::PrepareMenuScreen(bool) {}
void DisplayClass::MakeTrackString(unsigned short, unsigned short) {}
void DisplayClass::ShowTrackTitle(char const* t, char const*, bool) { SerialPrint("Track Title   : ", t); }
void DisplayClass::ShowVolume(unsigned char, unsigned char) {}
void DisplayClass::Suspend() {}
void DisplayClass::Resume() {}
void DisplayClass::ShowPlayTime(unsigned short, unsigned short, bool) {}
void DisplayClass::ShowHelpLine(const char * s, bool no_queue)   { SerialPrint("Help line     : ", s);  }
void DisplayClass::ShowStationName(const char *s, bool no_queue) { SerialPrint("Web Station   : ", s);  }
void DisplayClass::ShowHelpLine(uint8_t help_id, bool no_queue)  { ShowHelpLine(s_Help[help_id], no_queue); }
void DisplayClass::ShowPlayTime(unsigned short, bool) {}
void DisplayClass::ShowVolume() {}
void DisplayClass::ShowWebRadioConnect(bool) {}

void DisplayClass::ShowTrackNumber(void) {
  MakeTrackString(false);
  SetTimeString("-:--/-:--");
  SerialPrint("Track: ", _track_string);
}

void DisplayClass::ShowWebRadioNumber(void) {
  MakeTrackString(true);
  SetTimeString("-:--");
  SerialPrint("Web Station: ", _track_string);
}

void DisplayClass::ShowWaveformId(void) {}

void DisplayClass::ShowPlayPause(bool play, bool no_queue) { Serial.println(play ? "Play  " : "Paused"); }

void DisplayClass::ShowPlayTime(bool no_queue) {
  char s[32];
  PlayTimeToString(s, Settings.V().CurrentTrackTime);
  Serial.print(s);
  Serial.print('/');
  PlayTimeToString(s, Settings.V().TotalTrackTime);
  Serial.print(s);
  Serial.print(' ');
  if((Settings.V().CurrentTrackTime & 15) == 15) Serial.println();
}

void DisplayClass::loop(uint32_t timestamp) {}

#endif
