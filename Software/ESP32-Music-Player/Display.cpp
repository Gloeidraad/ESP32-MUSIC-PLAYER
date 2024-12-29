
#include "Display.h"

#include "src/Version.h"
#include "src/Hardware/SSD1306.h"
#include "src/settings.h"
#include "SimpleWaveGenerator.h"

//******************************************************//const
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
      _track_string_len = sprintf(_track_string, "%d/%d",Settings.NV.WebRadioCurrentStation,Settings.NV.WebRadioTotalStations);
    else
      _track_string_len = sprintf(_track_string, "%d/%d",Settings.NV.DiskCurrentTrack,Settings.NV.DiskTotalTracks);
  #endif
}

//#define CURRENT_TIME_TEST  "33:12"
//#define CURRENT_TIME_TEST  "5:44:12"
//#define CURRENT_TIME_TEST  "25:44:12"
//#define TRACK_TIME_TEST    "33:45"
//#define TRACK_TIME_TEST    "6:33:45"
//#define TRACK_TIME_TEST    "26:33:45"

void DisplayClass::MakeTimeString(void) {
  char tt[12];
  int tt_l;
  #ifdef CURRENT_TIME_TEST
    strcpy(_time_string, CURRENT_TIME_TEST);
    _time_string_len = strlen(CURRENT_TIME_TEST);
  #else
    _time_string_len = PlayTimeToString(_time_string, Settings.CurrentTrackTime);
  #endif
  if(Settings.NV.SourceAF == SET_SOURCE_SD_CARD) {
    #ifdef TRACK_TIME_TEST
      strcpy(tt, TRACK_TIME_TEST);
      tt_l = strlen(tt);
    #else
      tt_l = PlayTimeToString(tt, Settings.TotalTrackTime > Settings.CurrentTrackTime ? Settings.TotalTrackTime : Settings.CurrentTrackTime);
    #endif
    if(_track_string_len + _time_string_len + tt_l <= 25 - 2) { // Do add total time olny if it fits
      _time_string[_time_string_len] = '/';
      strcpy(&_time_string[_time_string_len + 1], tt);
      _time_string_len += tt_l + 1;
    }
  }
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

  #ifdef I2C_PORT_OLED
    ssd1306_class OLED(I2C_PORT_OLED, SSD1306_LCDROWS + 4, 2); // Four extra rows for menus, two tickers
  #else
    ssd1306_class OLED(SPI, PIN_CS, PIN_DC, SSD1306_LCDROWS + 4, 2); // Four extra rows for menus, two tickers
  #endif

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

void DisplayClass::Initialize(bool reboot) {
  #ifdef I2C_PORT_OLED
    OLED.initialize(reboot ? OLED_INIT_NO_REGS_MASK : ROTATE_DISPLAY/*OLED_INIT_ROTATE180_MASK*/, I2C_HIGH_CLOCK_SPEED);
  #else
    OLED.initialize(reboot ? OLED_INIT_NO_REGS_MASK : ROTATE_DISPLAY/*OLED_INIT_ROTATE180_MASK*/, SPI_OLED_CLOCK_SPEED);
  #endif
  OLED.SetLineRow(MENU_LINE_1, PLAYER_NAME_LINE);
  OLED.SetLineRow(MENU_LINE_2, PLAYER_PLAY_LINE);
  OLED.SetLineRow(MENU_LINE_3, STATUS_LINE1);
  OLED.SetLineRow(MENU_LINE_4, STATUS_LINE2);
}

void DisplayClass::Suspend() { OLED.suspend(); }
void DisplayClass::Resume()  { OLED.resume();  }

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

void DisplayClass::ShowWelcome(void) {
  int x;
  OLED.puts(0, 1, s_Welcome[0], SSD1306_FONT_8X8);
  OLED.puts(0, 3, s_Welcome[1], SSD1306_FONT_8X8);
  OLED.puts(0, 5, s_Welcome[2], SSD1306_FONT_8X8);
  x = SSD1306_Fonts[SSD1306_FONT_5X8_NARROW].u8LineLength - strlen(s_Welcome[3]); 
  OLED.puts(x, 7, s_Welcome[3], SSD1306_FONT_5X8_NARROW);
  OLED.display(); // Force display
}

void DisplayClass::ShowInfo(const char * msg) {
  int len = strlen(msg);
  OLED.display(); // Empty display queue
  if(len > SSD1306_Fonts[SSD1306_FONT_5X8].u8LineLength)
    OLED.puts(0, 4, msg, SSD1306_Fonts[SSD1306_FONT_5X8_NARROW].u8LineLength, SSD1306_FONT_5X8_NARROW);
  else if(len > SSD1306_Fonts[SSD1306_FONT_8X8].u8LineLength)
    OLED.puts(0, 4, msg, SSD1306_Fonts[SSD1306_FONT_5X8].u8LineLength, SSD1306_FONT_5X8);
  else
    OLED.puts(0, 4, msg, SSD1306_Fonts[SSD1306_FONT_8X8].u8LineLength, SSD1306_FONT_8X8);
  OLED.display(); // Force display
}

void DisplayClass::ShowPlayer(void) {
  OLED.display(); // Empty display queue
  OLED.puts(0, PLAYER_NAME_LINE, Settings.GetSourceName(), SSD1306_FONT_8X8);
  OLED.display(); // Force display
}

void DisplayClass::ShowPlayPause(bool play, bool no_queue) {
  ShowLine(PLAYER_PLAY_LINE, play ? "Playing" : "Paused", SSD1306_FONT_8X8, no_queue);
}

void DisplayClass::ShowBaseScreen(const char * help_line) {
  Serial.printf("ShowBaseScreen: %s\n", help_line ? help_line : "[NULL]");
  OLED.display(); // Empty display queue
  OLED.setline(TRACK_LINE, 0);
  OLED.setline(TICKER_TOP_LINE, 0x08);
  //if(help_line != NULL) ShowHelpLine(help_line);
  ShowHelpLine(help_line == NULL ? s_Help[DISPLAY_HELP_PLEASE_WAIT] : help_line);
  OLED.setline(TICKER_BOTTOM_LINE, 0x10);
  ShowLine(PLAYER_NAME_LINE, Settings.GetSourceName(), SSD1306_FONT_8X8);
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD   :  ShowTrackNumber();    break;
    case SET_SOURCE_WEB_RADIO :  ShowWebRadioNumber(); break;
    case SET_SOURCE_WAVE_GEN  :  ShowWaveformId();     break;
    case SET_SOURCE_BLUETOOTH :  ShowBluetoothId();    break;
  }
  ShowPlayPause(Settings.Play);
  //ShowFrequencies();
  ShowVolume();
  OLED.display(); // Force display
}

void DisplayClass::ShowBaseScreen(uint8_t help_id) { ShowBaseScreen(s_Help[help_id]); }

void DisplayClass::ShowLine(uint8_t y, const char *s, uint8_t font, bool no_queue) {
  if(no_queue) OLED.display(); // Empty display queue
  OLED.setline(y, 0);
  if(s != NULL && s[0] != '\0') {
     int ll = SSD1306_Fonts[font].u8LineLength;
     if(strlen(s) > ll)
       OLED.puts(0, y, s, ll, SSD1306_FontId_t(font));
     else
       OLED.puts(0, y, s, SSD1306_FontId_t(font));
  }
  if(no_queue) OLED.display(); 
}

void DisplayClass::ShowHelpLine(const char * s, bool no_queue)  {
  ShowLine(TICKER_LINE, s, SSD1306_FONT_5X8_NARROW, no_queue);
}

void DisplayClass::ShowHelpLine(uint8_t help_id, bool no_queue) {
  ShowHelpLine(s_Help[help_id], no_queue);
}

void DisplayClass::ShowStationName(const char *s, bool no_queue) {
  ShowTrackName(s, no_queue);
}

void  DisplayClass::ShowReboot(void) {
  ShowBaseScreen(s_Help[DISPLAY_HELP_PLEASE_WAIT]);
}

void DisplayClass::ShowBluetoothId(void) {
  SetTrackString("1/1"); 
  SetTimeString("-:--");
  ShowTrackNumberAndPlayTime(true);
}

void DisplayClass::ShowBluetoothName(void) {
  int ll = SSD1306_Fonts[SSD1306_FONT_5X8_NARROW].u8LineLength;
  if(strlen((const char *)Settings.NV.BtName) > ll)
    OLED.start_ticker(TICKER_ID, TICKER_LINE, (const char *)Settings.NV.BtName, SSD1306_FONT_8X8);//, uint16_t speed = 3, uint16_t scrolldelay = 50);
  else
    ShowHelpLine((const char *)Settings.NV.BtName);
}

void DisplayClass::ShowTrackNumber(void) {
  MakeTrackString(false);
  if(Settings.CurrentTrackTime > 0) { // OMT: quick and dirty, needs rework
    Settings.TotalTrackTime = Settings.NV.DiskTrackTotalTime;
    MakeTimeString();
  }
  else
    SetTimeString("-:--/-:--");
  ShowTrackNumberAndPlayTime(true);
}

void DisplayClass::ShowTrackName(const char * nam, bool no_queue) {
  ShowLine(TICKER_LINE, nam, SSD1306_FONT_8X8, no_queue);
}

void DisplayClass::ShowTrackTitle(const char * t, bool no_queue) {
  OLED.start_ticker(TICKER_ID, TICKER_LINE, t, SSD1306_FONT_8X8);
}

void DisplayClass::ShowWebRadioConnect(bool OK) {
  char s[80];
  strcpy(s, s_Help[DISPLAY_HELP_WIFI_CONNECT_FAILED_ID]);
  strcat(s, SsidSettings.GetSsid(Settings.NV.CurrentSsid, false));
  OLED.start_ticker(TICKER_ID, TICKER_LINE, s, SSD1306_FONT_8X8);
}

void DisplayClass::ShowWaveformId(void) {
  sprintf(_track_string, "%d/%d", Settings.NV.WaveformId+1, SET_WAVE_FORM_MAX+1);
  _track_string_len = strlen(_track_string);
  SetTimeString("0:00");
  ShowTrackNumberAndPlayTime(true);
}

void DisplayClass::ShowWebRadioNumber(void) {
  MakeTrackString(true);
  SetTimeString("-:--");
  ShowTrackNumberAndPlayTime(true);
}

void DisplayClass::ShowPlayTime(bool no_queue) {
  MakeTimeString();
  ShowTrackNumberAndPlayTime(no_queue);
}

void DisplayClass::ShowTrackNumberAndPlayTime(bool no_queue) {
  int len, fn;

  if(no_queue) OLED.display(); // Empty display queue
  OLED.setline(TRACK_LINE, 0);
  len = _track_string_len +  _time_string_len + 1;
  if(len <= 18)       fn = SSD1306_FONT_6X10_NUM;
  else if (len <= 21) fn = SSD1306_FONT_5X10_NUM; 
  else                fn = SSD1306_FONT_4X10_NUM;
  const SSD1306_Font_t & f = SSD1306_Fonts[fn];
  OLED.puts(0,TRACK_LINE, _track_string, SSD1306_FontId_t(fn));
  OLED.set_x_offset(SSD1306_LCDWIDTH - (_time_string_len * f.u8FontWidth - (f.u8FontWidth - f.u8DataCols))); // Align right
  OLED.puts(0,TIME_LINE, _time_string, _time_string_len, SSD1306_FontId_t(fn));
  OLED.set_x_offset(0);
  if(no_queue) OLED.display(); 
}

void DisplayClass::ShowWaveformName(const char * id) {
  OLED.setline(TICKER_LINE, 0);
  OLED.puts(0, TICKER_LINE, id, SSD1306_FONT_8X8);
}

void DisplayClass::ShowStreamTitle(const char *s) {
  Serial.print("Streamtitle: ");
  if(strlen(s) > 0) {
    Serial.println(s);
    Display.ShowTrackTitle(s);
  }
  else {
    web_station_t web_station;
    UrlSettings.GetStation(Settings.NV.WebRadioCurrentStation, web_station);
    Display.ShowTrackTitle(web_station.name); // OMT
    Serial.print(web_station.name);
  }
}

#if 0  // no longer used, free to use for other info

void DisplayClass::ShowFrequencies(void) {
  char InfoString1[22];
  char InfoString2[22];
  InfoString1[0] = 0;
  InfoString2[0] = 0;
  OLED.setline(STATUS_LINE1, 0); 
  OLED.setline(STATUS_LINE2, 0);
  #if 0  // no longer used, free to use for other info 
  switch(Settings.NV.OutputSel) {
    case SET_OUTPUT_AM_ONLY: Settings.AmFreqToString(InfoString2, "AM:");
                             break;
    case SET_OUTPUT_FM_ONLY: Settings.FmFreqToString(InfoString2, "FM:");
                             break;
    case SET_OUTPUT_AM_FM:   Settings.AmFreqToString(InfoString1, "AM:");
                             Settings.FmFreqToString(InfoString2, "FM:");
                             break;
    case SET_OUTPUT_TV_ONLY: Settings.TvFreqToString(InfoString2, "TV:");
                             break;
    case SET_OUTPUT_AM_TV:   Settings.AmFreqToString(InfoString1, "AM:");
                             Settings.TvFreqToString(InfoString2, "TV:");
                             break;
    case SET_OUTPUT_FM_TV:   Settings.FmFreqToString(InfoString1, "FM:");
                             Settings.TvFreqToString(InfoString2, "TV:");
                             break;
    case SET_OUTPUT_ALL:     Settings.AmFreqToString(InfoString1, "AM:");
                             Settings.FmFreqToString(InfoString2, "FM:",0,true);
                             break;
    default:                 strcpy(InfoString2, "Output Sel. Error");
                             break;
  }
  OLED.puts_xy(0, STATUS_LINE1, InfoString1, SSD1306_FONT_5X8);
  OLED.puts_xy(0, STATUS_LINE2, InfoString2, SSD1306_FONT_5X8);
  if(Settings.NV.OutputSel == SET_OUTPUT_ALL) {
    Settings.TvChanToString(InfoString2,"TV:", 0, true);
    OLED.puts(SSD1306_Fonts[SSD1306_FONT_5X8].u8LineLength-strlen(InfoString2), STATUS_LINE2, InfoString2, SSD1306_FONT_5X8);
  }
  OLED.shift_row_down(STATUS_LINE2);
  #else
  ShowVolume();
  #endif
}
#endif

void DisplayClass::ShowVolume(uint8_t vol, uint8_t max) {
  OLED.setline(STATUS_LINE1, 0); 
  OLED.setline(STATUS_LINE2, 0);
  if(Settings.NV.SourceAF == SET_SOURCE_WEB_RADIO && Settings.NV.WebRadioTotalStations > 0) {
    web_station_t web_station;
    UrlSettings.GetStation(Settings.NV.WebRadioCurrentStation, web_station);
    OLED.puts(0, STATUS_LINE2, web_station.name, SSD1306_FONT_5X8_NARROW);
    if(strlen(web_station.name) <= 9)
      OLED.puts(11, STATUS_LINE2, "Volume", SSD1306_FONT_5X8_NARROW);
    else if(strlen(web_station.name) <= 12)
      OLED.puts(14, STATUS_LINE2, "Vol", SSD1306_FONT_5X8_NARROW);
  }
  else {
    OLED.puts(11, STATUS_LINE2, "Volume", SSD1306_FONT_5X8_NARROW);
  }
  ShowVolumeBar(vol, max);
  //OLED.shift_row_down(STATUS_LINE2);
}

void DisplayClass::ShowVolume() {
  ShowVolume(Settings.NV.Volume, Settings.NV.VolumeSteps);
}

void DisplayClass::ShowVolumeBar(uint8_t vol, uint8_t max) {
  uint8_t bar[max+2];
  for(int i = 0; i <= max; i++) {
    bar[i] = vol >= i ? 0x7F : 0x41;
  }
  bar[max+1] = 0x7F;
  OLED.setmem(SSD1306_LCDWIDTH - max - 3, STATUS_LINE2, bar, max+2);
}

void DisplayClass::ShowMenuString(uint8_t y, const char *s, uint8_t font) {
  if(s != NULL) {
    OLED.setline(y, 0); 
    OLED.puts(0, y, s, SSD1306_FontId_t(font));
  }
}

uint8_t DisplayClass::MenuStringMax() { return SSD1306_Fonts[SSD1306_FONT_5X8_NARROW].u8LineLength; }

void DisplayClass::ShowMenuStrings(const char *s1, const char *s2, const char *s3){
  ShowMenuString(MENU_LINE_1, s1, SSD1306_FONT_8X8);
  
  if(s2 != NULL) {
    int len = strlen(s2);
    Serial.printf("********************* ShowMenuStrings: len = %d\n", len);
    if(len <= 16)
      ShowMenuString(MENU_LINE_2, s2, SSD1306_FONT_8X8);
    else 
      OLED.puts(0, MENU_LINE_2, s2, 21, SSD1306_FONT_5X8_NARROW); // Truncate to 21 characters
  }
#ifndef  MENU_LINE_2_SHIFT
  OLED.setline(MENU_LINE_3, 0);
#else // Shift a bit for nicer display
  if(s2 != NULL) {
    OLED.shift_row_down(MENU_LINE_2, MENU_LINE_2_SHIFT); // Shift a bit for nicer display
  }
#endif
  ShowMenuString(MENU_LINE_4, s3, SSD1306_FONT_5X8_NARROW);
}

void DisplayClass::ShowLastHost(const char *s)    { SerialPrint("Lasthost    ", s);     }
void DisplayClass::ShowStation(const char *s)     { SerialPrint("Station     ", s);     }
void DisplayClass::ShowTrackEnded(const char *s)  { SerialPrint("EOF MP3     ", s);     }
void DisplayClass::ShowStreamEnded(const char *s) { SerialPrint("audio_eof_stream", s); }

void DisplayClass::loop(uint32_t timestamp) { OLED.loop(timestamp); }

#else 

// Write your own display interface here

DisplayClass::DisplayClass() { }
void DisplayClass::Initialize(bool reboot) {}
void DisplayClass::ShowWelcome(void) {}
void DisplayClass::ShowBaseScreen(const char * help_line) {}
void DisplayClass::ShowBaseScreen(uint8_t help_id)  { ShowBaseScreen(s_Help[help_id]); }

void DisplayClass::ShowStation(const char *s)                    { SerialPrint("Station     ", s);      }
void DisplayClass::ShowLastHost(const char *s)                   { SerialPrint("Lasthost    ", s);      }
void DisplayClass::ShowTrackEnded(const char *s)                 { SerialPrint("EOF MP3     ", s);      }
void DisplayClass::ShowStreamEnded(const char *s)                { SerialPrint("audio_eof_stream ", s); }
                                                      
void DisplayClass::ShowTrackTitle(const char * t, bool no_queue) { SerialPrint("Track Title   : ", t);  }
void DisplayClass::ShowWaveformName(const char * id)             { SerialPrint("Waveform name : ", id); } 
void DisplayClass::ShowHelpLine(const char * s, bool no_queue)   { SerialPrint("Help line     : ", s);  }
void DisplayClass::ShowBluetoothName(void)                       { SerialPrint("Bluetooth name: ", (const char *)Settings.NV.BtName); }
void DisplayClass::ShowStationName(const char *s, bool no_queue) { SerialPrint("Web Station   : ", s);  }

void DisplayClass::ShowHelpLine(uint8_t help_id, bool no_queue)  { ShowHelpLine(s_Help[help_id], no_queue); }

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

void DisplayClass::ShowReboot(void) {}
void DisplayClass::ShowPlayer(void) {}
void DisplayClass::ShowInfo(const char * msg) {}
void DisplayClass::ShowWaveformId(void) {}

void DisplayClass::ShowPlayPause(bool play, bool no_queue) { Serial.println(play ? "Play  " : "Paused"); }

void DisplayClass::ShowPlayTime(bool no_queue) {
  char s[32];
  PlayTimeToString(s, Settings.CurrentTrackTime);
  Serial.print(s);
  Serial.print('/');
  PlayTimeToString(s, Settings.TotalTrackTime);
  Serial.print(s);
  Serial.print(' ');
  if((Settings.CurrentTrackTime & 15) == 15) Serial.println();
}

void DisplayClass::ShowStreamTitle(const char *s) {
  SerialPrint("Streamtitle ", s);
}

void DisplayClass::loop(uint32_t timestamp) {}

#endif
