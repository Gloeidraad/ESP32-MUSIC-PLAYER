#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include "src/settings.h"

#define DISPLAY_HELP_NONE                    (uint8_t)0  
#define DISPLAY_HELP_WIFI_CONNECT_FAILED     (uint8_t)1  
#define DISPLAY_HELP_WIFI_CONNECTED          (uint8_t)2  
#define DISPLAY_HELP_PRESS_PLAY_TO_START     (uint8_t)3
#define DISPLAY_HELP_PLEASE_WAIT             (uint8_t)4
#define DISPLAY_HELP_INITIALIZING_WEBRADIO   (uint8_t)5
#define DISPLAY_HELP_NO_CARD                 (uint8_t)6
#define DISPLAY_HELP_NO_TRACKS_FOUND         (uint8_t)7
#define DISPLAY_HELP_WIFI_CONNECT_FAILED_ID  (uint8_t)8
#define DISPLAY_HELP_URLS_FOUND              (uint8_t)9

class DisplayClass {
    
  public:
    DisplayClass();
    void Initialize(bool reboot);

    void ShowWelcome(void);
    void ShowBaseScreen(const char * help_line);
    void ShowBaseScreen(uint8_t help_id);
    
    void ShowReboot(void);
    void ShowPlayer(void);
    void ShowInfo(const char * msg);
    void ShowPlayPause(void) { ShowPlayPause(Settings.Play); }
    void ShowPlayPause(bool play, bool no_queue = false);
    void ShowPlayTime(bool no_queue = false);
    void ShowTrackNumber(void);
    void ShowWebRadioNumber(void);
    void ShowWebRadioConnect(bool OK);
    void ShowBluetoothId(void);
    void ShowBluetoothName(void);
    
    void ShowTrackName(const char * n, bool no_queue = false);
    void ShowTrackTitle(const char * t, bool no_queue = false);
    void ShowHelpLine(uint8_t help_id, bool no_queue = false);
    void ShowHelpLine(const char * s, bool no_queue = false);
    void ShowWaveformName(const char * id);
    void ShowWaveformId(void);
    void ShowStationName(const char *s, bool no_queue = false);
    
    void ShowStreamTitle(const char *s);
    void ShowLastHost(const char *s);
    void ShowTrackEnded(const char *s);
    void ShowStreamEnded(const char *s);
    void ShowStation(const char *s);

    void PrepareMenuScreen(bool menu_on); // false = leaving, true = entering
    void ShowMenuStrings(const char *s1, const char *s2, const char *s3);

    //void ShowFrequencies(void);
    void ShowVolume(void);
    void ShowVolume(uint8_t vol, uint8_t max);

    void loop(uint32_t timestamp);

  protected:
    void ShowLine(uint8_t y, const char *s, uint8_t font, bool no_queue = false);
 
    void MakeTrackString(bool web = false);
    void MakeTimeString(void);
    void ShowTrackNumberAndPlayTime(bool no_queue = false);
    void SetTrackString(const char *);
    void SetTimeString(const char *);
    void ShowMenuString(uint8_t y, const char *s, uint8_t font);

    char    _track_string[10]; // Can hold max "9999/9999"
    char    _time_string[18];  // Can hold max "24:59:59/24:59:59"
    uint8_t _track_string_len;
    uint8_t _time_string_len;
};

extern DisplayClass Display;

#endif //_DISPLAY_H_
