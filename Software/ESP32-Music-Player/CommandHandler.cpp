#include <Arduino.h>
#include "WiFi.h"

#define STRIPPED_AUDIO_FORMATS

#include "CommandHandler.h"
#include "src/Settings.h"
#include "src/ChipInfo.h" 
#include "src/Hardware/SSD1306.h"
#include "src/OneButton/OneButton.h"
#include "src/OneButton/OneButtonTiny.h"
#include "src/OneButton/OneButtonSmall.h"
#include "src/AudioI2S/Audio.h"
#include "src/BluetoothA2DP/BluetoothA2DPSink.h"
#include "SimpleWaveGenerator.h"
#include "Player.h"
#include "Display.h"
#include "GUI.h"

#include "src/AudioI2S/aac_decoder/aac_decoder.h"
#include "src/AudioI2S/flac_decoder/flac_decoder.h"
#include "src/AudioI2S/mp3_decoder/mp3_decoder.h"
#ifndef STRIPPED_AUDIO_FORMATS
  #include "src/AudioI2S/opus_decoder/opus_decoder.h"
  #include "src/AudioI2S/vorbis_decoder/vorbis_decoder.h"
  #include "src/AudioI2S/wav_decoder/wav_decoder.h"
#endif

extern PlayerClass    Player;
extern DisplayClass   Display;
extern ssd1306_class  OLED;
extern Audio          audio;

#define PL_SD SET_SOURCE_SD_CARD
#define PL_WR SET_SOURCE_WEB_RADIO
#define PL_WF SET_SOURCE_WAVE_GEN
#define PL_BT SET_SOURCE_BLUETOOTH

//=====================================================================//
//== Local: print track title in hex (to identify UTF8 characters)   ==//
//=====================================================================//

static void print_title_hex() {
  const char * s = Player.GetCallbackTrackTitle();
  while(*s) {
    Serial.printf("%02X", *s++);
    if(*s) Serial.print(',');
  }
  Serial.println();
}

//=====================================================================//
//== Local: print wifi information and power set                     ==//
//=====================================================================//

static void print_wifi_info() {
  char s[64];
  WiFi.printDiag(Serial);
  sprintf(s, "WiFi TX level: %d (%d = %sdB)", WiFi.getTxPower(), Settings.GetWifiTxLevel(), Settings.GetWifiTxLevelDB());
  Serial.println(s);
}

static void set_wifi_power(int pwr, bool force = false) {
  if(!force && (pwr < 0 || pwr == Settings.NV().WifiTxIndex || pwr > SET_WIFI_TX_MAX)) {
    pwr = Settings.NV().WifiTxIndex;
    Serial.print("Wifi TX Power stays at ");
  }
  else {
    Settings.NV().WifiTxIndex = pwr;
    Serial.print("Changing Wifi TX Power to ");
    if (Settings.NV().SourceAF == SET_SOURCE_WEB_RADIO)
      WiFi.setTxPower((wifi_power_t)Settings.GetWifiTxLevel());
  }
  Serial.printf("entry %d (value %d = %sdB)\n",pwr, Settings.GetWifiTxLevel(), Settings.GetWifiTxLevelDB());
}

//=====================================================================//
//== Local: print bluetooth information and power set                ==//
//=====================================================================//
//#include "esp_gap_ble_api.h"
//#include "esp_bt_defs.h"
#include "esp_bt.h"

//#include "BLEDevice.h"
//#include "esp_power_level.h"
// See: https://github.com/espressif/arduino-esp32/blob/master/libraries/BLE/src/BLEDevice.cpp

static void print_bluetooth_info() {
  if(Settings.NV().SourceAF == SET_SOURCE_BLUETOOTH) {
    #if 1
      int lvl = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);
      Serial.printf("Bluetooth TX level: %d (%d dB) \n", lvl, (lvl - 4)*3);

    #else
    BLEDevice::init((char *)Settings.NV().BtName);
    Serial.printf("Bluetooth TX level: %d (%d dB) \n", esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT), BLEDevice::getPower());
    Serial.println(BLEDevice::toString());
    #endif
  }
}

static void set_ble_tx_power() {
  if(Settings.NV().SourceAF == SET_SOURCE_BLUETOOTH) {
    #if 1
      esp_err_t errRc = /*::*/esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, (esp_power_level_t)Settings.NV().BtTxIndex);
      if (errRc != ESP_OK) {
        //log_e("esp_ble_tx_power_set: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
      }

    #else
    BLEDevice::init((char *)Settings.NV().BtName);
    BLEDevice::setPower((esp_power_level_t)Settings.NV().BtTxIndex, ESP_BLE_PWR_TYPE_DEFAULT);
    BLEDevice::setPower((esp_power_level_t)Settings.NV().BtTxIndex, ESP_BLE_PWR_TYPE_CONN_HDL0);
    BLEDevice::setPower((esp_power_level_t)Settings.NV().BtTxIndex, ESP_BLE_PWR_TYPE_CONN_HDL1);
    BLEDevice::setPower((esp_power_level_t)Settings.NV().BtTxIndex, ESP_BLE_PWR_TYPE_CONN_HDL2);
    BLEDevice::setPower((esp_power_level_t)Settings.NV().BtTxIndex, ESP_BLE_PWR_TYPE_CONN_HDL3);
    BLEDevice::setPower((esp_power_level_t)Settings.NV().BtTxIndex, ESP_BLE_PWR_TYPE_CONN_HDL4);
    BLEDevice::setPower((esp_power_level_t)Settings.NV().BtTxIndex, ESP_BLE_PWR_TYPE_CONN_HDL5);
    BLEDevice::setPower((esp_power_level_t)Settings.NV().BtTxIndex, ESP_BLE_PWR_TYPE_CONN_HDL6);
    BLEDevice::setPower((esp_power_level_t)Settings.NV().BtTxIndex, ESP_BLE_PWR_TYPE_CONN_HDL7);
    BLEDevice::setPower((esp_power_level_t)Settings.NV().BtTxIndex, ESP_BLE_PWR_TYPE_CONN_HDL8);
    BLEDevice::setPower((esp_power_level_t)Settings.NV().BtTxIndex, ESP_BLE_PWR_TYPE_ADV);
    BLEDevice::setPower((esp_power_level_t)Settings.NV().BtTxIndex, ESP_BLE_PWR_TYPE_SCAN);
    #endif
  }
}

#if 0

https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/bluetooth/esp_gap_ble.html

https://docs.espressif.com/projects/esp-techpedia/en/latest/esp-friends/advanced-development/performance/modify-tx-power.html

esp_ble_tx_power_set()

ESP32 BLE Power Levels 
The esp_ble_tx_power_set function accepts the following power levels from the esp_power_level_t enumeration: 

    -12 dBm: ESP_PWR_LVL_N12
    -9 dBm: ESP_PWR_LVL_N9
    -6 dBm: ESP_PWR_LVL_N6
    -3 dBm: ESP_PWR_LVL_N3
    0 dBm: ESP_PWR_LVL_P0 (Standard reference)
    3 dBm: ESP_PWR_LVL_P3
    6 dBm: ESP_PWR_LVL_P6
    9 dBm: ESP_PWR_LVL_P9 (Maximum power) 

   #include "esp_gap_ble_api.h"

// Set to maximum power (+9 dBm)
esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);

#endif

//=====================================================================//
//== Local: change player                                            ==//
//=====================================================================//

static void change_player(uint8_t old_id, uint8_t new_id) {
  if(new_id != old_id) {
    Serial.print("Change Player from \"");
    Serial.print(Settings.GetSourceName(old_id));
    Serial.print("\" to \"");
    Serial.print(Settings.GetSourceName(new_id));
    Serial.println("\"");
    Player.Stop();
    Settings.V().Play = 0;
    Settings.NV().SourceAF = new_id;
    Display.ShowBaseScreen(DISPLAY_HELP_PLEASE_WAIT);
    delay(100); // Give threads in Player.Stop() some time to die out properly
    Settings.doSoftRestart();
  }
}

//=====================================================================//
//== Local: start/pause/ressume player                               ==//
//=====================================================================//

static void sd_play_track(bool clear_nv_pos) {
  if(Settings.NV().DiskTotalTracks > 0) {
    if(Settings.NV().DiskCurrentTrack == 0) Settings.NV().DiskCurrentTrack = 1;
    Display.MakeTrackString(Settings.NV().DiskCurrentTrack, Settings.NV().DiskTotalTracks);
    const char *s = TrackSettings.GetTrackName(Settings.NV().DiskCurrentTrack);
    Display.ShowTrackTitle(s, ".mp3");
    Serial.printf("Playing \"%s\"\n", s);
    if(clear_nv_pos) {
      Settings.NV().DiskTrackResumeTime = 0;
      Settings.NV().DiskTrackResumePos  = 0;
      Settings.NV().DiskTrackTotalTime  = 0;
    }
    Settings.V().CurrentTrackTime = 0;
    Settings.V().TotalTrackTime   = 0;
    Player.PlayTrack(SD_MMC, s);
    Display.ShowPlayPause(true);
  }
}

static void sd_play_prev_track() {
  if(Settings.NV().DiskTotalTracks > 0) {
    if(Settings.NV().DiskCurrentTrack == 0) Settings.NV().DiskCurrentTrack = 1;
    // If track is playing 5 seconds or longer, then restart the track only
    if(Settings.V().CurrentTrackTime <= 5) {   
      Serial.printf("Next from %d/%d to ", Settings.NV().DiskCurrentTrack, Settings.NV().DiskTotalTracks);
      Settings.PrevDiskTrack();
      Serial.printf("%d/%d\n", Settings.NV().DiskCurrentTrack, Settings.NV().DiskTotalTracks);
    }
    else {
      Serial.printf("Restart %d/%d\n", Settings.NV().DiskCurrentTrack, Settings.NV().DiskTotalTracks);
    }
    sd_play_track(true);
  }
}

static void sd_play_next_track() {
  if(Settings.NV().DiskTotalTracks > 0) {
    if(Settings.NV().DiskCurrentTrack == 0) Settings.NV().DiskCurrentTrack = 1;
    Serial.printf("Next from %d/%d to ", Settings.NV().DiskCurrentTrack, Settings.NV().DiskTotalTracks);
    Settings.NextDiskTrack();
    Serial.printf("%d/%d\n", Settings.NV().DiskCurrentTrack, Settings.NV().DiskTotalTracks);
    sd_play_track(true);
  }
}

static void wr_connect(bool reconnect) {
  if(Settings.NV().SourceAF != SET_SOURCE_WEB_RADIO) return;
  if(WiFi.status() == WL_CONNECTED)
    WiFi.disconnect();
  uint64_t timestamp_us = esp_timer_get_time();
  uint8_t  timeout = 25;
  Display.ShowHelpLine(reconnect ? DISPLAY_HELP_RECONNECT_WEBRADIO: DISPLAY_HELP_INITIALIZING_WEBRADIO, true);
  Display.Suspend();
  Serial.printf("Connecting to %s ", SsidSettings.GetSsid(Settings.NV().CurrentSsid), false);
  WiFi.begin(SsidSettings.GetSsid(Settings.NV().CurrentSsid), SsidSettings.GetPassword(Settings.NV().CurrentSsid),1);
  while (WiFi.status() != WL_CONNECTED  && timeout > 0) {
    if( esp_timer_get_time() >= timestamp_us + 500000) {
      timestamp_us = esp_timer_get_time();
      timeout--;
      Serial.print(".");
    }
  }
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println(" CONECTION FAILED");
    Display.Resume();
    Display.ShowWebRadioConnect(false);
  }
  else {
    Serial.println(" CONNECTED");
    set_wifi_power(Settings.NV().WifiTxIndex, true);
    Display.Resume();
    Display.ShowHelpLine(Settings.NV().WebRadioTotalStations > 0 ? DISPLAY_HELP_PRESS_PLAY_TO_START : DISPLAY_HELP_NO_URLS_FOUND, true);
  }
}

static void wr_play_station() {
  if(Settings.NV().WebRadioTotalStations == 0) return;
  Settings.V().TrackTitleReceived = 0;
  if(WiFi.isConnected()) {
    web_station_t web_station;
    UrlSettings.GetStation(Settings.NV().WebRadioCurrentStation, web_station);
    Serial.printf("**********start a new radio: %s\n",web_station.url);
    Player.PlayWebStation(web_station.url);
    Display.ShowStationName(web_station.name);
     Serial.println("**********start a new radio************");
    Settings.V().AudioEnded = 0;
    Settings.V().Play = 1;
    Settings.V().FirstTime = 0;
  }
  Display.ShowWebRadioNumber();
  Display.ShowVolume(Settings.NV().Volume, Settings.NV().VolumeSteps); // Show also station name  
}

static void wr_play_prev_station() {
  if(Settings.NV().WebRadioTotalStations > 0) {
    Settings.PrevRadioStation();
    wr_play_station();
    //Display.ShowVolume(Settings.NV().Volume, Settings.NV().VolumeSteps); // Show also station name
  }
}

static void wr_play_next_station() {
  if(Settings.NV().WebRadioTotalStations > 0) {
    Settings.NextRadioStation();
    wr_play_station();
    //Display.ShowVolume(Settings.NV().Volume, Settings.NV().VolumeSteps);  // Show also station name
  }
}

static void bt_play_prev_track() { Player.PlayPrevious(); }

static void bt_play_next_track() { Player.PlayNext(); }

static void sd_play_first_track() {
  Serial.println("FIRST TIME");    
  if(Settings.NV().DiskTrackResumeTime <= 10) { // just play if resume time is quit early...
    sd_play_track(true);
  }
  else { // ...else play quitely for 5 seconds, and then reposition to saved location
    Player.SetVolume(0);     
    sd_play_track(false);
    char s[16] = "Forward";
    for(int sec = 5; sec > 0; sec--) {
      strcat(s, ".");
      Display.ShowTrackTitle(s);
      uint32_t start_time = millis();
      while(millis() < start_time + 1000) { Player.loop(); Display.loop(0); }
    }
    Player.RepositionTrack(true);
    Player.SetVolume(Settings.NV().Volume);
    Display.ShowTrackTitle(TrackSettings.GetTrackName(Settings.NV().DiskCurrentTrack), ".mp3"); // Restore track name
  }
  Settings.V().FirstTime = 0;
}

static void wf_play_waveform(int adj = 0) {
  if(adj != 0) {
    if(adj < 0) Settings.PrevWaveform(); //Settings.NV().WaveformId =  Settings.NV().WaveformId == 0 ? SET_WAVE_FORM_MAX : Settings.NV().WaveformId - 1;
    else        Settings.NextWaveform(); //Settings.NV().WaveformId = Settings.NV().WaveformId >= SET_WAVE_FORM_MAX ? 0 : Settings.NV().WaveformId + 1;
  }
  Player.PlayWaveform();
  Display.ShowTrackTitle(Player.WaveformName(Settings.NV().WaveformId));
  Settings.V().CurrentTrackTime = 0;
  Display.ShowWaveformId();
  Display.ShowPlayPause(Player.IsRunning());
}

#define PL_SD SET_SOURCE_SD_CARD
#define PL_WR SET_SOURCE_WEB_RADIO
#define PL_WF SET_SOURCE_WAVE_GEN
#define PL_BT SET_SOURCE_BLUETOOTH

static void play_next() {
  switch(Settings.NV().SourceAF) {
    case PL_SD: sd_play_next_track(); Settings.V().FirstTime = 0; break;
    case PL_WR: wr_play_next_station(); Settings.V().FirstTime = 0; break;
    case PL_WF: wf_play_waveform(1); break;
    case PL_BT: bt_play_next_track(); Settings.V().FirstTime = 0; break;
  }
}

static void play_prev() {
  switch(Settings.NV().SourceAF) {
    case PL_SD: sd_play_prev_track(); Settings.V().FirstTime = 0; break;
    case PL_WR: wr_play_prev_station(); Settings.V().FirstTime = 0; break;
    case PL_WF: wf_play_waveform(-1); break;
    case PL_BT: bt_play_prev_track(); Settings.V().FirstTime = 0; break;
  }
}

static void play_new(void) {
  switch(Settings.NV().SourceAF) {
    case PL_SD: Settings.NV().DiskCurrentTrack = Settings.V().NewTrack;
                sd_play_track(true);
                Settings.V().FirstTime = 0; 
                break;
    case PL_WR: Settings.NV().WebRadioCurrentStation = Settings.V().NewTrack;
                wr_play_station();
                break;
    default: break;
  }  
}

static void pause_resume() {
  switch(Settings.NV().SourceAF) {
    case PL_SD: if(Settings.V().FirstTime) {
                  sd_play_first_track();
                }
                else {
                  if(Player.PauseResume()) {
                    Serial.printf("Resume\n");
                    Display.ShowPlayPause(true);
                  }
                  else {
                    Serial.printf("Stopped at %d/%d (%d)\n", Settings.NV().DiskTrackResumeTime, Settings.V().TotalTrackTime, Settings.NV().DiskTrackResumePos);
                    Display.ShowPlayPause(false);
                  }
                }
                break;
    case PL_WR: if(Settings.V().FirstTime) {
                  wr_play_station();
                  Settings.V().FirstTime = 0;
                  Display.ShowPlayPause(true);
                }
                else {
                  bool play = Player.PauseResume();
                  Serial.println(play ? "Resume" : "Paused");
                  Display.ShowPlayPause(play);
                }
                break;
    case PL_WF: if(Settings.V().FirstTime) {
                  wf_play_waveform(0); 
                  Settings.V().FirstTime = 0;
                  Display.ShowPlayPause(true);
                }
                else {
                  Player.PauseResume();
                  Display.ShowPlayPause(Player.IsRunning());
                }
                break;
    case PL_BT: Player.PauseResume();
                Settings.V().FirstTime = 0;
                break;
  }
}

static void player_update() {
  if(Player.TimeUpdated()) {
    if(Settings.NV().SourceAF == SET_SOURCE_SD_CARD)
      Display.ShowPlayTime(Settings.V().CurrentTrackTime, Settings.V().TotalTrackTime);
    else
      Display.ShowPlayTime(Settings.V().CurrentTrackTime);
  }
  if(Settings.V().AudioEnded) {
    sd_play_next_track();  // Only for SD-Card
    Settings.V().AudioEnded = 0; 
  }
  if(Settings.V().TrackTitleReceived) {
    Display.ShowTrackTitle(Player.GetCallbackTrackTitle(), "");
    Settings.V().TrackTitleReceived = 0;
  }
  if(Settings.V().NewVolumeReceived) {
    Display.ShowVolume(Settings.NV().Volume, Settings.NV().VolumeSteps);
    Settings.V().NewVolumeReceived = 0;
  }
  if(Settings.V().NewPlayStatusReceived) {
    switch(Settings.V().NewPlayStatusReceived) {
      case SET_PLAYBACK_STOPPED :  Display.ShowPlayPause(Settings.V().Play = false); break;
      case SET_PLAYBACK_PLAYING :  Display.ShowPlayPause(Settings.V().Play = true);  Settings.V().FirstTime = 0; break; 
      case SET_PLAYBACK_PAUSED  :  Display.ShowPlayPause(Settings.V().Play = false); break;
    }
    Settings.V().NewPlayStatusReceived = 0;    
  }
}

static void system_info() {
  PrintChipInfo(0xF);
  Serial.println();
  Serial.print("Audioi2S version: "); Serial.println(audio.getVersion());
  Serial.println("Some sizes of used classes:");
  Serial.printf("SettingsData_t     : %d\n", sizeof(SettingsData_t));
  Serial.printf("NvSettingsData_t   : %d\n", sizeof(NvSettingsData_t));
  Serial.printf("SettingsClass      : %d\n", sizeof(SettingsClass));
  Serial.printf("SsidSettingsClass  : %d\n", sizeof(SsidSettingsClass));
  Serial.printf("web_station_t      : %d\n", sizeof(web_station_t));
  Serial.printf("UrlSettingsClass   : %d\n", sizeof(UrlSettingsClass));
  Serial.printf("TrackSettingsClass : %d\n", sizeof(TrackSettingsClass));
  Serial.printf("OneButton          : %d\n", sizeof(OneButton));
  Serial.printf("OneButtonTiny      : %d\n", sizeof(OneButtonTiny));
  Serial.printf("OneButtonSmall     : %d\n", sizeof(OneButtonSmall));
  Serial.printf("ssd1306_font_t     : %d\n", sizeof(ssd1306_font_t));
  Serial.printf("ssd1306_class      : %d\n", sizeof(ssd1306_class));
  Serial.printf("AudioBuffer        : %d\n", sizeof(AudioBuffer));
  Serial.printf("Audio              : %d\n", sizeof(Audio));
  Serial.printf("  Decoder          : %d\n", sizeof(Decoder));
  Serial.printf("  AACDecoder       : %d\n", sizeof(AACDecoder));
  Serial.printf("  FlacDecoder      : %d\n", sizeof(FlacDecoder));
  Serial.printf("  MP3Decoder       : %d\n", sizeof(MP3Decoder));
  #ifndef STRIPPED_AUDIO_FORMATS
    Serial.printf("  OpusDecoder      : %d\n", sizeof(OpusDecoder));
    Serial.printf("  VorbisDecoder    : %d\n", sizeof(VorbisDecoder));
    Serial.printf("  WavDecoder       : %d\n", sizeof(WavDecoder));
  #endif
  Serial.printf("BluetoothA2DPSink  : %d\n", sizeof(BluetoothA2DPSink));
  Serial.printf("DisplayClass       : %d\n", sizeof(DisplayClass));
  Serial.printf("PlayerClass        : %d\n", sizeof(PlayerClass));
  Serial.printf("SimpleWaveGeneratorClass: %d\n", sizeof(SimpleWaveGeneratorClass));
}

//=====================================================================//
//== Local: serial_command parser (for debugging purposes)           ==//
//=====================================================================//

static const struct {
  const char * const shortname;
  const char * const longname;
  bool suppress_help;
} serial_command_list[] = {
  { "?",      "help"            , false }, // 0
  { "s",      "settings"        , false }, // 1
  { "rl",     "radiolist"       , false }, // 2
  { "rlr",    "radiolistraw"    , false }, // 3
  { "ssid",   "ssid"            , false }, // 4
  { "ssidpw", "ssidpassword"    , true  }, // 5
  { "tl",     "tracklist"       , false }, // 6
  { "pi",     "playinfo"        , false }, // 7
  { "sd",     "sd-card"         , false }, // 8
  { "wr",     "webradio"        , false }, // 9
  { "wf",     "waveform"        , false }, // 10
  { "bt",     "bluetooth"       , false }, // 11
  { "res",    "softreset"       , false }, // 12
  { "pw",     "printwaveform"   , false }, // 13
  { "pwh",    "printwaveformhex", false }, // 14
  { "sys",    "systeminfo"      , false }, // 15
  { "oled",   "oled-display"    , false }, // 16
  { "wifi",   "wifi-status"     , false }, // 17
  { "tx",     "wifi-tx-power"   , false }, // 18
  { "vol",    "volume"          , false }, // 19
  { "pp",     "play/pause"      , false }, // 20
  { "next",   "nexttrack"       , false }, // 21
  { "prev",   "prevtrack"       , false }, // 22
  { "ta",     "title-analyze"   , false }, // 23
  { "btx",  "bluetooth-tx-power", false }, // 24
  { "mem",    "memory-usage"    , false }, // 25
};

static void serial_command_help(void) {
  Serial.println("Short   | Long");
  Serial.println("--------|-------------");
  for (int i = 0; i < sizeof(serial_command_list) / sizeof(serial_command_list[0]); i++)
    if (!serial_command_list[i].suppress_help)
      Serial.printf("%-8s| %s\n", serial_command_list[i].shortname, serial_command_list[i].longname);
}

static int find_command_match(String &r) {
  for(int i = sizeof(serial_command_list) / sizeof(serial_command_list[0]) - 1; i >= 0 ; i--) {
    if(r.startsWith(serial_command_list[i].shortname) || r.startsWith(serial_command_list[i].longname)) {
      int len = r.indexOf(' ');
      if(len > 0) 
        r = r.substring(len + 1);
      return i;
    }
  }
  return -1;
}

static void parse_command(String &r) { 
  r.trim();
  r.toLowerCase();
  int len = r.indexOf(' ');
  Serial.print("SERIAL COMMAND: ");
  Serial.println(r);
  switch(find_command_match(r)) {
    case  0: serial_command_help();     break;
    case  1: Settings.Print();          break;
    case  2: UrlSettings.Print(false);  break;
    case  3: UrlSettings.Print(true);   break;
    case  4: SsidSettings.Print(false); break;
    case  5: SsidSettings.Print(true);  break;
    case  6: TrackSettings.Print();     break;
//    case  7: Player.SD_PrintDebug();    break;
    case  8: change_player(Settings.GetCurrentSourceId(), SET_SOURCE_SD_CARD);   break;
    case  9: change_player(Settings.GetCurrentSourceId(), SET_SOURCE_WEB_RADIO); break;
    case 10: change_player(Settings.GetCurrentSourceId(), SET_SOURCE_WAVE_GEN);  break;
    case 11: change_player(Settings.GetCurrentSourceId(), SET_SOURCE_BLUETOOTH); break;
    case 12: Settings.doSoftRestart();  break;
    //case 13: Player.WP_PrintDebug(false); break;
    //case 14: Player.WP_PrintDebug(true);  break;
    case 15: system_info();             break;
    case 16: OLED.Print();              break;
    case 17: print_wifi_info();         break;
    case 18: set_wifi_power(r.toInt()); break;
    case 19: { int vol = r.toInt();
               if(vol > 0)
                 Player.SetVolume(vol);
             }
             break;
    case 20: pause_resume();     break;
    case 21: play_next();        break;
    case 22: play_prev();        break;
    case 23: print_title_hex();  break;
    case 24: print_bluetooth_info(); break;
    case 25: PrintChipInfo(0xE);  /*Settings.V().CurrentTrackTime= 18 *3600;*/ break;
    default: Serial.println("Unknown Command"); break;
  }
}

//=====================================================================//
//== Publics                                                        ==//
//=====================================================================//

CommandHandlerClass::CommandHandlerClass() { }

void CommandHandlerClass::SerialCommand(String &r) { parse_command(r); }

void CommandHandlerClass::SetWifiPower(int pwr, bool force) { set_wifi_power(pwr, force); }
void CommandHandlerClass::SetBtPower(int pwr, bool force) { set_ble_tx_power(); }

void CommandHandlerClass::WifiConnect(bool reconnect) { wr_connect(false); }
void CommandHandlerClass::CheckUpdateFromCallback()   { player_update(); }

void CommandHandlerClass::GuiCommand(uint8_t key) {
  if(key != 0xFF) Serial.printf("Button %d pressed: ", key);
  switch(GuiCallback(key)) {
    case GUI_RESULT_VOLUME_DOWN   : if(Player.VolumeDown()) Display.ShowVolume(Settings.NV().Volume, Settings.NV().VolumeSteps); break;
    case GUI_RESULT_VOLUME_UP     : if(Player.VolumeUp())   Display.ShowVolume(Settings.NV().Volume, Settings.NV().VolumeSteps); break;
    case GUI_RESULT_NEXT_PLAY     : play_next(); break;
    case GUI_RESULT_PREV_PLAY     : play_prev(); break;
    case GUI_RESULT_NEW_TRACK     : play_new(); break;
    case GUI_RESULT_PAUSE_PLAY    : pause_resume(); break;
    case GUI_RESULT_OUTPUT_FORMAT : Player.UpdateOutputFormat(); break;
    case GUI_RESULT_SEEK_PLAY     : Settings.NV().DiskTrackResumeTime = Settings.V().SeekTrackTime;
                                    Settings.NV().DiskTrackResumePos  = 0;    // Unkown file position
                                    Player.RepositionTrack(false);  
                                    Display.ShowPlayTime(Settings.NV().DiskTrackResumeTime, Settings.V().TotalTrackTime);  
                                    break;
    //case GUI_RESULT_TIMEOUT           : break;
    case GUI_RESULT_NEW_AF_SOURCE     : change_player(Settings.GetCurrentSourceId(), Settings.V().NewSourceAF); break;
    case GUI_RESULT_NEW_SSID          : wr_connect(true);/*Player.NewSsid();*/ break;
    case GUI_RESULT_NEW_WIFI_TX_LEVEL : SetWifiPower(Settings.NV().WifiTxIndex, true); break;
    case GUI_RESULT_NEW_BT_TX_LEVEL   : SetBtPower(Settings.NV().BtTxIndex, true); break;
    default: break;
  }
  CheckUpdateFromCallback();
  if(key != 0xFF) Serial.println();
}
