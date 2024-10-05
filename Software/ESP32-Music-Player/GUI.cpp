#include <string.h>
#include <stdio.h>
#include "src/Hardware/board.h"
#include "src/Settings.h"
#include "GUI.h"
#include "display.h"
#include "Player.h"

#define MENU_DEBUG(s) Serial.printf(s)

/*****************************************************************************/
/*  Menu texts                                                               */
/*****************************************************************************/

#define GUI_STRING static const char * const

#define NEW_SD_CARD "SD Card"

         // Ruler for 16 characters "1234567890123456" , "1234567890123456"; 

GUI_STRING MENU_TEXT_SOURCE0    =   "Audio Source"    ;
GUI_STRING MENU_TEXT_SSID0      =   "Select Wifi SSID";
GUI_STRING MENU_TEXT_TIME       =   "Go to Track Time";
GUI_STRING MENU_SELECT_TRACK    =   "Select Track"    ;
GUI_STRING MENU_SELECT_STATION  =   "Select Station"  ;
GUI_STRING MENU_OK_CONFIRM        = "Press OK to confirm";
GUI_STRING MENU_WIFI_TX_INDEX0    = "WiFi TX Level";
GUI_STRING MENU_BT_TX_INDEX0      = "BT TX Level";

/*****************************************************************************/
/*  Playing time variables and functions                                     */
/*    - For efficient processing speed we maintain both the integer and an   */
/*      ASCII version for CurrentTrackTime                                   */
/*****************************************************************************/

static char CurrentTrackTimeString[10]; // first char holds offset

static void CurrentTrackTimeToString() {
    unsigned t = Settings.CurrentTrackTime;
    CurrentTrackTimeString[0] = 5; // index to first valid char
    CurrentTrackTimeString[9] = 0;
    CurrentTrackTimeString[8] = t % 10 + '0'; t = t / 10;
    CurrentTrackTimeString[7] = t %  6 + '0'; t = t /  6;
    CurrentTrackTimeString[6] = ':';
    CurrentTrackTimeString[5] = t % 10 + '0'; t = t / 10;
    if(t) CurrentTrackTimeString[0] = 4;
    CurrentTrackTimeString[4] = t %  6 + '0'; t = t /  6;
    CurrentTrackTimeString[3] = ':';
    if(t) CurrentTrackTimeString[0] = 2;
    CurrentTrackTimeString[2] = t % 10 + '0'; t = t / 10;
    if(t) CurrentTrackTimeString[0] = 1;
    CurrentTrackTimeString[1] = t % 10 + '0'; t = t / 10;
}

/*****************************************************************************/
/*  Menu defines and variables                                               */
/*****************************************************************************/

#define MENU_DELAY_PLAYER      8  // seconds
#define MENU_DELAY_SEEK        10 // seconds

#define MENU_FUNC_INIT         0
#define MENU_FUNC_CALLBACK     1
#define MENU_FUNC_TICK         2  // Occurs every tenth of a second
#define MENU_FUNC_KEY_UP       3
#define MENU_FUNC_KEY_DN       4
#define MENU_FUNC_KEY_OK       5
#define MENU_FUNC_KEY_MENU     6
#define MENU_FUNC_EXIT         7
#define MENU_FUNC_KEY_LONG_UP  8
#define MENU_FUNC_KEY_LONG_DN  9

#define MENU_RESULT_EXIT       50
#define MENU_RESULT_SETUP      52

static int menu_delay_100ms = 0; // Menu counter for delay in tenths of a second
static int menu_edit_val    = 0; // Value will be copied to settings when confirmed

#define MENU_RESET_TIMEOUT(T) menu_delay_100ms =  (T * TIMESTAMP_RATE_100MS)

/*****************************************************************************/
/*  Some useful menu routines                                                */
/*****************************************************************************/

static void MenuValueAdjust(int adj, int min, int max, int roll) {
  if( min == max)
    menu_edit_val = min;
  else {
    menu_edit_val += adj;
    if(menu_edit_val < min) menu_edit_val = roll ? max : min;
    if(menu_edit_val > max) menu_edit_val = roll ? min : max;
  }
  MENU_RESET_TIMEOUT(MENU_DELAY_PLAYER);
}

/*****************************************************************************/
/*  Player menu                                                              */
/*****************************************************************************/

static int MenuPlayer(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : MENU_DEBUG("MenuPlayer: INIT\n");
                              Display.PrepareMenuScreen(false);
                              break;
#if 0
    case MENU_FUNC_KEY_DN   : MENU_DEBUG("MenuPlayer: KEY_DN\n");
                              return GUI_RESULT_PREV_PLAY;
    case MENU_FUNC_KEY_UP   : MENU_DEBUG("MenuPlayer: KEY_UP\n");
                              return GUI_RESULT_NEXT_PLAY;
#else
    case MENU_FUNC_KEY_LONG_DN : MENU_DEBUG("MenuPlayer: KEY_LONG_DN\n");
                                 return GUI_RESULT_PREV_PLAY;
    case MENU_FUNC_KEY_LONG_UP : MENU_DEBUG("MenuPlayer: KEY_LONG_UP\n");
                                 return GUI_RESULT_NEXT_PLAY;
    case MENU_FUNC_KEY_DN   : MENU_DEBUG("MenuPlayer: KEY_DN\n");
                              if(Settings.NV.Volume > SET_VOLUME_MIN) {
                                Settings.NV.Volume--;
                                return GUI_RESULT_NEW_VOLUME;
                              }
                              break;
    case MENU_FUNC_KEY_UP   : MENU_DEBUG("MenuPlayer: KEY_UP\n");
                              if(Settings.NV.Volume < SET_VOLUME_MAX) {
                                Settings.NV.Volume++;
                                return GUI_RESULT_NEW_VOLUME;
                              }
                              break;
#endif
    case MENU_FUNC_KEY_OK   : MENU_DEBUG("Menu Player: KEY_OK\n");
                              return GUI_RESULT_PAUSE_PLAY;
    case MENU_FUNC_KEY_MENU : MENU_DEBUG("MenuPlayer: KEY_MENU\n");
                              Display.PrepareMenuScreen(true);
                              return MENU_RESULT_EXIT;
                              break;
  }
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Source menu                                                              */
/*****************************************************************************/

static void MenuSourceAdjust(int adj) {
  MenuValueAdjust(adj, 0, SET_SOURCE_MAX, 1);
  Display.ShowMenuStrings(NULL, Settings.GetSourceName(menu_edit_val), NULL);
}

static int MenuSource(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : menu_edit_val = Settings.NV.SourceAF;
                              Display.ShowMenuStrings(MENU_TEXT_SOURCE0, NULL, MENU_OK_CONFIRM);
                              MenuSourceAdjust(0);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_100ms == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_UP   : MenuSourceAdjust(1);  break;
    case MENU_FUNC_KEY_DN   : MenuSourceAdjust(-1); break;
    case MENU_FUNC_KEY_OK   : Settings.NewSourceAF = menu_edit_val; 
                              return GUI_RESULT_NEW_AF_SOURCE;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Select SSID                                                         */
/*****************************************************************************/

static void MenuSelectSsidAdjust(int adj) {
  if(Settings.NV.TotalSsid == 0) {
    Display.ShowMenuStrings(NULL, "No Network found", NULL);
    //Display.StartTicker("Insert SD Card with \"ssid.ini\", and restart the board.");
    MENU_RESET_TIMEOUT(MENU_DELAY_PLAYER);
  }
  else {
    MenuValueAdjust(adj, 0, Settings.NV.TotalSsid - 1, 1);
    Display.ShowMenuStrings(NULL, SsidSettings.GetSsid(menu_edit_val), NULL);
  }
}

static int MenuSelectSsid(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : if(Settings.NV.SourceAF != SET_SOURCE_WEB_RADIO)
                                return MENU_RESULT_EXIT; // go to next menu item
                              menu_edit_val = Settings.NV.CurrentSsid;
                              Display.ShowMenuStrings(MENU_TEXT_SSID0, NULL, MENU_OK_CONFIRM);
                              MenuSelectSsidAdjust(0);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_100ms == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_UP   : MenuSelectSsidAdjust(1);  break;
    case MENU_FUNC_KEY_DN   : MenuSelectSsidAdjust(-1); break;
    case MENU_FUNC_KEY_OK   : Settings.NV.CurrentSsid = menu_edit_val;   
                              return GUI_RESULT_NEW_SSID;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : //if(Settings.NV.SourceAF == SET_SOURCE_WEB_RADIO && Settings.NV.TotalSsid == 0)
                              //  Display.StartTicker("");
                              break;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Select Track                                                             */
/*****************************************************************************/

static void MenuTrackAdjust(int adj) {
  char text[32];
  MenuValueAdjust(adj, 1, Settings.NV.DiskTotalTracks, 1);
  const char * track_name = TrackSettings.GetTrackName(menu_edit_val);
  int len = strlen(track_name);
  strncpy(text, track_name, Display.MenuStringMax());
  text[Display.MenuStringMax()] = '\0';
  if(len < Display.MenuStringMax()) {
    char * p = strrchr(text, '.');
    if(p != NULL) *p = '\0'; // do not show the .mp3 extension 
  }
  Display.ShowMenuStrings(NULL, text, NULL);
}

static int MenuTrack(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : Serial.println("OMT Menu Track");
                              if(Settings.NV.SourceAF == SET_SOURCE_SD_CARD && Settings.NV.DiskTotalTracks > 2) {
                                menu_edit_val = Settings.NV.DiskCurrentTrack;
                                Display.ShowMenuStrings(MENU_SELECT_TRACK, NULL, MENU_OK_CONFIRM);
                              }
                              else
                               return MENU_RESULT_EXIT;
                              Settings.NewTrack = 0;                              
                              MenuTrackAdjust(0);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_100ms == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_UP   : MenuTrackAdjust(1);  break;
    case MENU_FUNC_KEY_DN   : MenuTrackAdjust(-1); break;
    case MENU_FUNC_KEY_OK   : Settings.NewTrack = menu_edit_val;
                              return GUI_RESULT_NEW_TRACK;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  if(Settings.NewTrack > 0)
    return GUI_RESULT_TIMEOUT;  // Terminate this menu as soon a new track has been chosen
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Select Station                                                           */
/*****************************************************************************/

static void MenuStationAdjust(int adj) {
  web_station_t web_station;
  MenuValueAdjust(adj, 1, Settings.NV.WebRadioTotalStations, 1);
  UrlSettings.GetStation(menu_edit_val, web_station);
  Display.ShowMenuStrings(NULL, web_station.name, NULL);
}

static int MenuStation(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : Serial.println("OMT Menu URL");
                              if(Settings.NV.SourceAF == SET_SOURCE_WEB_RADIO && Settings.NV.WebRadioTotalStations > 2) {
                                menu_edit_val = Settings.NV.WebRadioCurrentStation;
                                Display.ShowMenuStrings(MENU_SELECT_STATION, NULL, MENU_OK_CONFIRM);
                              }
                              else
                                return MENU_RESULT_EXIT;
                              Settings.NewTrack = 0;                              
                              MenuStationAdjust(0);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_100ms == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_UP   : MenuStationAdjust(1);  break;
    case MENU_FUNC_KEY_DN   : MenuStationAdjust(-1); break;
    case MENU_FUNC_KEY_OK   : Settings.NewTrack = menu_edit_val;
                              return GUI_RESULT_NEW_TRACK;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  if(Settings.NewTrack > 0)
    return GUI_RESULT_TIMEOUT;  // Terminate this menu as soon a new station has been chosen
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Track Time menu                                                          */
/*****************************************************************************/

static struct {
   unsigned char flag_play   : 1;
   unsigned char flag_select : 1;
   uint8_t       seconds     : 6;
   uint32_t      max_minutes;
   char          total_time[10];
} menu_time_data;

static void MenuTimeAdjust(int adj) {
  char text[32];
  if(adj != 0) {
    if(menu_time_data.seconds) {
      if(adj > 0)                                             // if(prev) round down to current minute
        MenuValueAdjust(1, 0, menu_time_data.max_minutes, 0); // else round up to next minute
      menu_time_data.seconds = 0;
    }
    else
      MenuValueAdjust(adj, 0, menu_time_data.max_minutes, 0);
  }
  sprintf(text, "%d:%02d%s", menu_edit_val, menu_time_data.seconds, menu_time_data.total_time);
  Display.ShowMenuStrings(NULL, text, NULL);
  MENU_RESET_TIMEOUT(MENU_DELAY_SEEK);
}

static int MenuTime(int function) {
  char text[24];
  static Audio * SD_Audio;
  if(menu_time_data.flag_select != 0) { // Already sought in a track?
    menu_time_data.flag_select = 0;
    return GUI_RESULT_TIMEOUT;
  }
  switch(function) {
    case MENU_FUNC_INIT     : MENU_DEBUG("MenuTime: INIT\n");
                              if(Settings.NV.SourceAF != SET_SOURCE_SD_CARD || Settings.NoCard || Settings.NV.DiskTotalTracks == 0)
                                return MENU_RESULT_EXIT; // go to next menu item

                              SD_Audio = Player.GetAudioPlayer();
                              if(SD_Audio == NULL)  
                                 return MENU_RESULT_EXIT; // go to next menu item
                              
                              menu_time_data.flag_play = SD_Audio->isRunning();
                              if(menu_time_data.flag_play) {
                                Settings.Play = 0;
                                SD_Audio->pauseResume(); // (temporally) stop playing
                              }
                              Display.ShowMenuStrings(MENU_TEXT_TIME, NULL, MENU_OK_CONFIRM);
                              Settings.CurrentTrackTime  = SD_Audio->getAudioCurrentTime();
                              Settings.TotalTrackTime    = SD_Audio->getAudioFileDuration();
                              menu_time_data.max_minutes = Settings.TotalTrackTime / 60;
                              sprintf(menu_time_data.total_time, "/%d:%02d", menu_time_data.max_minutes,  Settings.TotalTrackTime % 60);
                              menu_time_data.seconds     = Settings.CurrentTrackTime % 60; // keep seconds
                              menu_edit_val              = Settings.CurrentTrackTime / 60; // only minutes
                              menu_time_data.flag_select = 0;
                              MenuTimeAdjust(0);
                              MENU_RESET_TIMEOUT(MENU_DELAY_SEEK);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_100ms == 0) {
                                Settings.Play = menu_time_data.flag_play;
                                if(menu_time_data.flag_play)
                                  SD_Audio->pauseResume(); // continue playing
                                return GUI_RESULT_TIMEOUT;
                              }
                              break;
    case MENU_FUNC_KEY_UP   : MenuTimeAdjust(1);  break;
    case MENU_FUNC_KEY_DN   : MenuTimeAdjust(-1); break;
    case MENU_FUNC_KEY_OK   : Settings.SeekTrackTime = menu_edit_val * 60 + menu_time_data.seconds;
                              menu_time_data.flag_select = 1;
                              Settings.Play = menu_time_data.flag_play;
                              return GUI_RESULT_SEEK_PLAY;
    case MENU_FUNC_KEY_MENU : Settings.Play = menu_time_data.flag_play;
                              if(menu_time_data.flag_play)
                                SD_Audio->pauseResume(); // continue playing
                              return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Select Volume                                                            */
/*****************************************************************************/
#if 0 

static int SpeakerVolumeSelect; // 0 Volume, 1 = Balance

static void MenuSpeakerVolumeShowSelect() {
  //char * s = SpeakerVolumeSelect ? MENU_TEXT_VOLUME0_BAL : MENU_TEXT_VOLUME0_AUX;
  //Display_ShowMenuStrings(s, NULL, NULL);
}

static void MenuSpeakerVolumeShowLevel() {
  char text[32];
  //sprintf(text, MENU_TEXT_VOLUME1_VOL, Settings.NV.VolAux1, Settings.NV.VolAux2);
  //Display_ShowMenuStrings(NULL, text, NULL);
  MENU_RESET_TIMEOUT(MENU_DELAY_PLAYER);
}

static void MenuSpeakerVolumeAdjustVal(unsigned char *val, int adj) {
  menu_edit_val = *val;
  MenuValueAdjust(adj, SET_VOLUME_MIN, SET_VOLUME_MAX, 0);
  *val = menu_edit_val;
}

static void MenuSpeakeVolumeAdjustVol(int adj) {
  #if 0
  unsigned char *v1,*v2;
  if(SpeakerVolumeSelect == 0) {
    if(Settings.NV.VolAux1 <= Settings.NV.VolAux2)
      { v1 = &Settings.NV.VolAux1; v2 = &Settings.NV.VolAux2; }
    else
      { v1 = &Settings.NV.VolAux2; v2 = &Settings.NV.VolAux1; }
    if(*v1 + adj < SET_VOLUME_MIN)
      { *v2 = SET_VOLUME_MIN + *v2 - *v1; *v1 = SET_VOLUME_MIN; }
    else if(*v2 + adj > SET_VOLUME_MAX)
      { *v1 = SET_VOLUME_MAX - *v2 + *v1; *v2 = SET_VOLUME_MAX; }
    else
      { *v1 += adj; *v2 += adj; }
  }
  else {
    MenuSpeakerVolumeAdjustVal(&Settings.NV.VolAux1, -adj);
    MenuSpeakerVolumeAdjustVal(&Settings.NV.VolAux2,  adj);
    // balance cannot be odd; adjust if necessary
    if(Settings.NV.VolAux1 == SET_VOLUME_MAX && Settings.NV.VolAux2 & 1)
      MenuSpeakerVolumeAdjustVal(&Settings.NV.VolAux2, -1);
    if(Settings.NV.VolAux2 == SET_VOLUME_MAX && Settings.NV.VolAux1 & 1)
      MenuSpeakerVolumeAdjustVal(&Settings.NV.VolAux1, -1);
    if(Settings.NV.VolAux1 == SET_VOLUME_MIN && Settings.NV.VolAux2 & 1)
      MenuSpeakerVolumeAdjustVal(&Settings.NV.VolAux2, 1);
    if(Settings.NV.VolAux2 == SET_VOLUME_MIN && Settings.NV.VolAux1 & 1)
      MenuSpeakerVolumeAdjustVal(&Settings.NV.VolAux1, 1);
  }
  MenuSpeakerVolumeShowLevel();
  #endif
}

static int MenuVolumeControl(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : SpeakerVolumeSelect = 0;
                              MenuSpeakerVolumeShowSelect();
                              MenuSpeakerVolumeShowLevel();
                              //TFT_ICONS(MENUFONT_MINUS,MENUFONT_PLUS,MENUFONT_BALANCE,MENUFONT_MENU);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_100ms == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_UP   : MenuSpeakeVolumeAdjustVol(4);  return GUI_RESULT_NEW_VOLUME;
    case MENU_FUNC_KEY_DN   : MenuSpeakeVolumeAdjustVol(-4); return GUI_RESULT_NEW_VOLUME;
    //case MENU_FUNC_ENC_SW   :
    case MENU_FUNC_KEY_OK   : SpeakerVolumeSelect = !SpeakerVolumeSelect;
                              MenuSpeakerVolumeShowSelect();
                              MenuSpeakerVolumeShowLevel();
                              //if(SpeakerVolumeSelect) TFT_ICONS(0,0,MENUFONT_VOLUME,0);
                              //else                    TFT_ICONS(0,0,MENUFONT_BALANCE,0);
                              break;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    //case MENU_FUNC_ENC_CCW  : MenuSpeakeVolumeAdjustVol(-1); return GUI_RESULT_NEW_VOLUME;
    //case MENU_FUNC_ENC_CW   : MenuSpeakeVolumeAdjustVol(1);  return GUI_RESULT_NEW_VOLUME;
    case MENU_FUNC_EXIT     : /*AuxVolToString();*/ break;
  };
  return GUI_RESULT_NOP;
}
#endif

/*****************************************************************************/
/*  Select WIFI Maximum TX level                                             */
/*****************************************************************************/

static void MenuWifiTxLevelAdjust(int adj) {
  char s[20];
  MenuValueAdjust(adj, 0, SET_WIFI_TX_MAX, 0);
  sprintf(s,"Max pwr %s dB",Settings.GetWifiTxLevelDB(menu_edit_val));
  Display.ShowMenuStrings(NULL, s, NULL);
}

static int MenuWifiTxLevel(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : if(Settings.NV.SourceAF != SET_SOURCE_WEB_RADIO)
                                return MENU_RESULT_EXIT; // go to next menu item
                              menu_edit_val = Settings.NV.WifiTxIndex;
                              Display.ShowMenuStrings(MENU_WIFI_TX_INDEX0, NULL, MENU_OK_CONFIRM);
                              MenuWifiTxLevelAdjust(0);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_100ms == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_UP   : MenuWifiTxLevelAdjust(-1); break;
    case MENU_FUNC_KEY_DN   : MenuWifiTxLevelAdjust(1);  break;
    case MENU_FUNC_KEY_OK   : Settings.NV.WifiTxIndex = menu_edit_val;
                              return GUI_RESULT_NEW_WIFI_TX_LEVEL;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Select Bluetooth Maximum TX level                                        */
/*****************************************************************************/

static void MenuBtTxLevelAdjust(int adj) {
  char s[20];
  MenuValueAdjust(adj, 0, SET_BT_TX_MAX, 0);
  // For levels see:
  // https://docs.espressif.com/projects/esp-idf/en/v4.1/api-reference/bluetooth/controller_vhci.html
  sprintf(s,"Max pwr %+d dB", menu_edit_val * 3 - 12);
  Display.ShowMenuStrings(NULL, s, NULL);
}

static int MenuBtTxLevel(int function) {
  switch(function) {
    case MENU_FUNC_INIT     : //if(Settings.NV.SourceAF != SET_SOURCE_BLUETOOTH)
                              //  return MENU_RESULT_EXIT; // go to next menu item
                              return MENU_RESULT_EXIT;  // T.b.d.
                              menu_edit_val = Settings.NV.BtTxIndex;
                              Display.ShowMenuStrings(MENU_BT_TX_INDEX0, NULL, MENU_OK_CONFIRM);
                              MenuBtTxLevelAdjust(0);
                              break;
    case MENU_FUNC_TICK     : if(--menu_delay_100ms == 0)
                                return GUI_RESULT_TIMEOUT;
                              break;
    case MENU_FUNC_KEY_UP   : MenuBtTxLevelAdjust(1);  break;
    case MENU_FUNC_KEY_DN   : MenuBtTxLevelAdjust(-1); break;
    case MENU_FUNC_KEY_OK   : Settings.NV.BtTxIndex = menu_edit_val;
                              return GUI_RESULT_NEW_BT_TX_LEVEL;
    case MENU_FUNC_KEY_MENU : return MENU_RESULT_EXIT;
    case MENU_FUNC_EXIT     : break;
  };
  return GUI_RESULT_NOP;
}

/*****************************************************************************/
/*  Menu function selector                                                   */
/*****************************************************************************/

static int current_menu;

typedef struct {
  int (* f)(int);
  int next;
  int next_alt;
  int use_timeout;
} MenuType;

#define MENU_INDEX_PLAYER          0
#define MENU_INDEX_TRACK           1
#define MENU_INDEX_URL             2
#define MENU_INDEX_TIME            3
#define MENU_INDEX_SSID            4
#define MENU_INDEX_SOURCE          5
#define MENU_INDEX_VOLUME          6 // Not used in this application
#define MENU_INDEX_SETUP_TX_LEVEL1 7
#define MENU_INDEX_SETUP_TX_LEVEL2 8
#define MENU_INDEX_COUNT           9

#define MENU_INDEX_DEFAULT         MENU_INDEX_PLAYER
static MenuType MenuEntries[MENU_INDEX_COUNT] = {
  { MenuPlayer,          MENU_INDEX_TRACK,           0,0 },
  { MenuTrack,           MENU_INDEX_URL,             0,1 },
  { MenuStation,         MENU_INDEX_TIME,            0,1 },
  { MenuTime,            MENU_INDEX_SSID,            0,1 },
  { MenuSelectSsid,      MENU_INDEX_SOURCE,          0,1 },
  { MenuSource,          MENU_INDEX_SETUP_TX_LEVEL1, 0,1 },
  { MenuWifiTxLevel,     MENU_INDEX_SETUP_TX_LEVEL2, 0,0 },
  { MenuBtTxLevel,       MENU_INDEX_DEFAULT,         0,0 }
};

#define MenuFunction(func) MenuEntries[current_menu].f(func)

/*****************************************************************************/
/*  GUI: Init                                                                */
/*****************************************************************************/

void GuiInit() {
  // Init some strings
  CurrentTrackTimeToString();

  menu_time_data.flag_select = 0;
  // Init menu
  current_menu = MENU_INDEX_PLAYER;
  MenuFunction(MENU_FUNC_INIT);
}

/*****************************************************************************/
/* GUI: Main routine (called every 20.8 to 125 microseconds)                 */
/*****************************************************************************/

int GuiCallback(int key_press) {
  static uint8_t  key               = 0xff;
  static uint64_t timestamp_us_prev = 0;
  int result                        = GUI_RESULT_NOP;
  uint64_t timestamp_us             = esp_timer_get_time();
 
  if(key_press != 0xff)
    key = key_press;  // Remember the key to next time slot

  // Use own local time stamp. (The main time stamp does not work properly here,
  // since it falls behind the real stamp when playing track which loads the
  // processor heavily. As soon as playing is interrupted, you get all the
  // delayed ticks immediately. Causing the menu to malfunction).
  if(timestamp_us - timestamp_us_prev >= TIMESTAMP_100MS_DIV) { 
    timestamp_us_prev = timestamp_us;
    result = MenuFunction(MENU_FUNC_TICK); // 100ms second time slices
  }
  else
   return GUI_RESULT_NOP;

  if(result == GUI_RESULT_NOP) {
    if(key != 0xff) {
      switch(key) {
        case KEY_SPARE : break;
        case KEY_DN      : result = MenuFunction(MENU_FUNC_KEY_DN);      break;
        case KEY_UP      : result = MenuFunction(MENU_FUNC_KEY_UP);      break;
        case KEY_LONG_DN : result = MenuFunction(MENU_FUNC_KEY_LONG_DN); break;
        case KEY_LONG_UP : result = MenuFunction(MENU_FUNC_KEY_LONG_UP); break;
        case KEY_OK      : result = MenuFunction(MENU_FUNC_KEY_OK);      break;
        case KEY_MENU    : result = MenuFunction(MENU_FUNC_KEY_MENU);    break;
      }
      key = 0xff;
    }
  }
  do {
    switch(result) {
      case MENU_RESULT_SETUP:     MenuFunction(MENU_FUNC_EXIT);
                                  current_menu = MenuEntries[current_menu].next_alt;
                                  MenuFunction(MENU_FUNC_INIT);
                                  break;
      case MENU_RESULT_EXIT:      MenuFunction(MENU_FUNC_EXIT);
                                  Settings.EepromStore();
                                  current_menu = MenuEntries[current_menu].next;
                                  result = MenuFunction(MENU_FUNC_INIT);
                                  break;
      case GUI_RESULT_TIMEOUT:    MenuFunction(MENU_FUNC_EXIT);
                                  Settings.EepromStore();
                                  current_menu = MENU_INDEX_DEFAULT;
                                  MenuFunction(MENU_FUNC_INIT);
                                  break;
    }
  } while(result == MENU_RESULT_EXIT);
  return result;
}
