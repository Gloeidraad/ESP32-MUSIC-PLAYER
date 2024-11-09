
// Increase size:
// https://github.com/espressif/arduino-esp32/issues/1906

#include <esp_idf_version.h>

#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)) || (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
  #error Cannot be built with version 1.0.6 or older or version 3.0.0 or newer
#endif

#include "src/AudioI2S/Audio.h"
#include "src/OneButton/OneButton.h"
#include "src/Hardware/board.h"
#include "src/Hardware/NFOR_SSD1306.h"
#include "src/settings.h"
#include "src/ChipInfo.h" 
#include "src/I2C_Scanner.h"

#include "Player.h"
#include "Display.h"
#include "SimpleWaveGenerator.h"
#include "Gui.h"

SettingsClass      Settings(I2C_PORT_EEPROM, I2C_LOW_CLOCK_SPEED);

SsidSettingsClass  SsidSettings;
UrlSettingsClass   UrlSettings;
TrackSettingsClass TrackSettings;

PlayerClass        Player;
DisplayClass       Display;

SimpleWaveGeneratorClass WaveGenerator;

#define BLUETOOTH_FILE_NAME "bluetooth.ini"
#define SSID_FILE_NAME      "ssid.ini"
#define WEBRADIO_FILE_NAME  "webradio.ini"

OneButton SW1(PIN_SW1);
OneButton SW2(PIN_SW2);
OneButton SW3(PIN_SW3);
OneButton SW4(PIN_SW4);
OneButton SW5(PIN_SW5);

static void PrintWifiInfo() {
  char s[64];
   //WiFi.printDiag(Serial);
   sprintf(s, "WiFi TX level: %d (max %d = %sdB)", WiFi.getTxPower(), Settings.GetWifiTxLevel(), Settings.GetWifiTxLevelDB());
  Serial.println(s);
}

void SetWifiPower(int pwr, bool force = false) {
  if(!force && (pwr < 0 || pwr == Settings.NV.WifiTxIndex || pwr > SET_WIFI_TX_MAX)) {
    pwr = Settings.NV.WifiTxIndex;
    Serial.print("Wifi TX Power stays at ");
  }
  else {
    Settings.NV.WifiTxIndex = pwr;
    Serial.print("Changing Wifi TX Power to ");
    if (Settings.NV.SourceAF == SET_SOURCE_WEB_RADIO)
      WiFi.setTxPower((wifi_power_t)Settings.GetWifiTxLevel());
  }
  Serial.printf("entry %d (value %d = %sdB)\n",pwr, Settings.GetWifiTxLevel(), Settings.GetWifiTxLevelDB());
}

//=====================================================================//
//== Local: serial_command parser (for debugging purposes)           ==//
//=====================================================================//

static struct {
  const char * shortname;
  const char * longname;
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
  { "vol",    "volume"          , true  }, // 19
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

static void change_player(uint8_t old_id, uint8_t new_id);

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
    case  7: Player.SD_PrintDebug();    break;
    case  8: change_player(Settings.GetCurrentSourceId(), SET_SOURCE_SD_CARD);   break;
    case  9: change_player(Settings.GetCurrentSourceId(), SET_SOURCE_WEB_RADIO); break;
    case 10: change_player(Settings.GetCurrentSourceId(), SET_SOURCE_WAVE_GEN);  break;
    case 11: change_player(Settings.GetCurrentSourceId(), SET_SOURCE_BLUETOOTH); break;
    case 12: Settings.doSoftRestart();    break;
    case 13: Player.WP_PrintDebug(false); break;
    case 14: Player.WP_PrintDebug(true);  break;
    case 15: PrintChipInfo(0x0F);         break;
    case 16: OLED.Print();                break;
    case 17: PrintWifiInfo();             break;
    case 18: SetWifiPower(r.toInt());     break;
    case 19: { int vol = r.toInt();
               if(vol > 0)
                 Player.SetVolume(vol);
             }
             break;
    default: Serial.println("Unknown Command"); break;
  }
}

//=====================================================================
//== Local: Timestamps
//=====================================================================

class TimeStampClass {
  public:
    TimeStampClass();
    void Update(void);
    uint32_t GetStamp10ms(void) { return timestamp_10ms;    }
    uint32_t GetStamp1sec(void) { return timestamp_1sec;    } 
    bool     Get10msTick(void)  { return ten_ms_tick;       }
    bool     GetSecondTick()    { return second_tick;       }
    bool     Get4SecTick()      { return four_seconds_tick; }
  private:
    uint64_t timestamp_us_prev;
    uint16_t timestamp_1sec_cnt;
    uint32_t timestamp_10ms;
    uint32_t timestamp_1sec;
    bool     ten_ms_tick;
    bool     second_tick;
    bool     four_seconds_tick;
};

TimeStampClass::TimeStampClass() {
  timestamp_10ms          = 0;
  timestamp_1sec          = 0;
  ten_ms_tick             = false;
  second_tick             = false;
  four_seconds_tick       = false;
  timestamp_us_prev       = 0;
  timestamp_1sec_cnt      = 0;
}

void TimeStampClass::Update(void) {
  uint64_t timestamp_us = esp_timer_get_time();
  ten_ms_tick             = false;
  second_tick             = false;
  four_seconds_tick       = false;
  if(timestamp_us - timestamp_us_prev >= TIMESTAMP_10MS_DIV) {
    timestamp_10ms++;
    timestamp_1sec_cnt++;
    timestamp_us_prev += TIMESTAMP_10MS_DIV;
    if(timestamp_us - timestamp_us_prev < TIMESTAMP_10MS_DIV) // supress multiple ticks 
      ten_ms_tick = true; 
    if(timestamp_1sec_cnt == 100) {
      timestamp_1sec++;
      timestamp_1sec_cnt = 0;
      second_tick = true;
      four_seconds_tick = !(timestamp_1sec & 3);
    }
  }
}

TimeStampClass TimeStamps;

//=====================================================================//
//== Local: Read setup and track data from SD Card                   ==//
//=====================================================================//

// MMC-SD: see https://github.com/espressif/arduino-esp32/tree/master/libraries/SD_MMC/src

static bool ReadSetupFromCard(bool get_init_data = true) {
  if(Settings.NV.SourceAF == SET_SOURCE_WAVE_GEN) return true; // Nothing to load for waveform player
  #ifdef USE_SD_MMC
    if(!SD_MMC.begin( "/sdcard", true)) {
      Serial.println("Card Mount Failed, using SSID & Radio lists from EEPROM");
      Settings.NoCard = 1;
      return false;
    }
  #else
    if (!SD.begin(PIN_SPI_SS, SPI, SPI_SD_SPEED)) {
      Serial.println("Card Mount Failed, using SSID & Radio lists from EEPROM");
      Settings.NoCard = 1;
      return false;
    }
  #endif
  Serial.println("Card Mount Succeeded");
  Settings.NoCard = 0;
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD   : TrackSettings.LoadFromCard();
                                break;
    case SET_SOURCE_WEB_RADIO : if(!SsidSettings.LoadFromCard(SSID_FILE_NAME))
                                  Serial.println("Using SSID list from EEPROM");
                                if(!UrlSettings.LoadFromCard(WEBRADIO_FILE_NAME))
                                  Serial.println("Using web station list from EEPROM");
                                break;
    case SET_SOURCE_BLUETOOTH : if(!Settings.LoadFromCard(BLUETOOTH_FILE_NAME))
                                  Serial.println("Using Bluetooth ID from EEPROM");
                                break;
  }
  #ifdef USE_SD_MMC
    SD_MMC.end(); // webradio doesn't like a mounted SD Card
  #else
    SD_MMC.end(); // webradio doesn't like a mounted SD Card
  #endif
  return true;
}

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
    Settings.Play = 0;
    Settings.NV.SourceAF = new_id;
    Display.ShowBaseScreen(DISPLAY_HELP_PLEASE_WAIT);
    delay(100); // Give threads in Stop() some time to die out properly
    Settings.doSoftRestart();
  }
}

static void change_player(bool prev) {
  uint8_t CurrentSourceId = Settings.GetCurrentSourceId();
  if(prev) Settings.PrevPlayer();
  else     Settings.NextPlayer();
  change_player(CurrentSourceId, Settings.GetCurrentSourceId());
}

//=====================================================================//
//== Local: Loop function for Settings (keeps audio running)         ==//
//=====================================================================//

void SettingsLoopFunction(void) { Player.loop(); }

//=====================================================================//
//== Local: Button handlers                                          ==//
//=====================================================================//

static void GUI_Handler(uint8_t key) {
  if(key != 0xFF) Serial.printf("Button %d pressed: ", key);
  #if 1
  switch(GuiCallback(key)) {
    case GUI_RESULT_NEW_TRACK      : Player.PlayNew();      break;
    case GUI_RESULT_NEXT_PLAY      : Player.PlayNext();     break;
    case GUI_RESULT_PREV_PLAY      : Player.PlayPrevious(); break;
    case GUI_RESULT_PAUSE_PLAY     : if(Settings.FirstTime) {
                                       Serial.println("FIRST TIME");    
                                       Settings.FirstSong = 1;
                                       Player.Play();
                                       Settings.FirstTime = 0;
                                     }
                                     else
                                       Player.PauseResume();
                                     break;
    case GUI_RESULT_SEEK_PLAY      : Player.PositionEntry(); break;
    //case GUI_RESULT_STOP_PLAY    : break;
    //case GUI_RESULT_START_PLAY   : break;
    case GUI_RESULT_NEW_SSID       : Player.NewSsid(); break;
    case GUI_RESULT_NEW_AF_SOURCE  : change_player(Settings.NV.SourceAF, Settings.NewSourceAF); break;
    case GUI_RESULT_NEW_WIFI_TX_LEVEL : SetWifiPower(Settings.NV.WifiTxIndex, true); break;
    case GUI_RESULT_NEW_BT_TX_LEVEL   : /*SetBtPower(Settings.NV.BtTxIndex, true);*/ break;
    case GUI_RESULT_NEW_VOLUME     : Player.SetVolume(Settings.NV.Volume); break;
    default: break;
  }
  #endif
  if(key != 0xFF) Serial.println();
}

static void SW1_HandleClick(void)     { GUI_Handler(KEY_SPARE);   } 
static void SW2_HandleClick(void)     { GUI_Handler(KEY_DN);      }
static void SW2_HandleLongClick(void) { GUI_Handler(KEY_LONG_DN); }
static void SW3_HandleClick(void)     { GUI_Handler(KEY_UP);      }
static void SW3_HandleLongClick(void) { GUI_Handler(KEY_LONG_UP); }
static void SW4_HandleClick(void)     { GUI_Handler(KEY_OK);      }
static void SW5_HandleClick(void)     { GUI_Handler(KEY_MENU);    }

//=====================================================================//
//== Local: Housekeeping, fix things that might go wrong in time     ==//
//=====================================================================//

static void Housekeeping(void) {
 }

//=====================================================================//
//== Main Set-up                                                     ==//
//=====================================================================//
//
// It appears that switching to another player cannot be done dynamically.
// Some parts of the player seems not to be cleaned up properly, so the new
// player will not work, or is unstable. Therefore, the solution is to do a
// soft restart to assure the player get a fresh end clean environment.

void setup(void) {
  //Serial
  Serial.begin(115200);

  //SD(SPI)
  #ifdef USE_SD_MMC
    pinMode(SD_MMC_D0, INPUT_PULLUP);
    SD_MMC.setPins(SD_MMC_CLK,SD_MMC_CMD, SD_MMC_D0);
  #else
    pinMode(PIN_SPI_SS, OUTPUT);
    digitalWrite(PIN_SPI_SS, HIGH);
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
    SPI.setFrequency(1000000);
  #endif
  // I2C
  I2C_PORT_OLED.begin(PIN_SDA2, PIN_SCL2, I2C_HIGH_CLOCK_SPEED);
  I2C_PORT_OLED.setBufferSize(256 + 8);
  I2C_PORT_EEPROM.begin(PIN_SDA1, PIN_SCL1, I2C_LOW_CLOCK_SPEED);
  I2C_PORT_EEPROM.setBufferSize(256 + 8);

  // Buttons
  // uint8_t boot_keys = 0;
  Settings.BootKeys = 0;
  if(digitalRead(PIN_SW1) == LOW) Settings.BootKeys |= BOOT_KEY1_MASK;
  if(digitalRead(PIN_SW2) == LOW) Settings.BootKeys |= BOOT_KEY2_MASK;
  if(digitalRead(PIN_SW3) == LOW) Settings.BootKeys |= BOOT_KEY3_MASK;
  if(digitalRead(PIN_SW4) == LOW) Settings.BootKeys |= BOOT_KEY4_MASK;
  if(digitalRead(PIN_SW5) == LOW) Settings.BootKeys |= BOOT_KEY5_MASK;
  Serial.printf("Button boot keys pressed at startup 0x%02X \n", Settings.BootKeys);

  SW1.attachClick(SW1_HandleClick);
  SW2.attachClick(SW2_HandleClick);
  SW2.attachLongPressStart(SW2_HandleLongClick);
  SW3.attachClick(SW3_HandleClick);
  SW3.attachLongPressStart(SW3_HandleLongClick);
  SW4.attachClick(SW4_HandleClick);
  SW5.attachClick(SW5_HandleClick);

  if (Settings.BootKeys == 0x12) { // UP & MENU
    Settings.EepromErase();        // Use this when settings have got corrupted
  }
  Settings.EepromLoad();           // Get the primary settings form EEPROM
  Display.Initialize(Settings.isSoftRestart());
  if(Settings.isSoftRestart()) {   // A soft restart does not show the TimesStampClassme screen, so it
    Settings.clearSoftRestart();   // simulates an impression of a dynamically change of player.
    Settings.Play = 1;             // After a soft restart, we start playing immediately (Note: BT always starts immediately)
    if(Settings.NV.SourceAF == SET_SOURCE_WEB_RADIO) {
      SsidSettings.EepromLoad();
      UrlSettings.EepromLoad();
      if(Settings.NV.TotalSsid == 0 || Settings.NV.WebRadioTotalStations == 0)
        ReadSetupFromCard();
    }
    if(Settings.NV.SourceAF == SET_SOURCE_SD_CARD)
      ReadSetupFromCard(); // (Re)load the track list
  }
  else {
    TimeStamps.Update();
    uint32_t timestamp_now = TimeStamps.GetStamp10ms();
    Display.ShowWelcome(); 
    if(Settings.NV.SourceAF == SET_SOURCE_WEB_RADIO) {
      SsidSettings.EepromLoad();
      UrlSettings.EepromLoad();
    }
    ReadSetupFromCard();
    while(TimeStamps.GetStamp10ms() < timestamp_now + 300)  // Show welcome screen at least 3 seconds
      TimeStamps.Update();
    Settings.Play = Settings.NV.SourceAF == SET_SOURCE_BLUETOOTH; // Bluetooth is always "autoplay"
  }
  Settings.SetLoopFunction(SettingsLoopFunction);
  delay(50);
  Scan_I2C(Wire, 0);
  Scan_I2C(Wire1, 1);
  Display.ShowBaseScreen(Settings.Play ? DISPLAY_HELP_PLEASE_WAIT : DISPLAY_HELP_PRESS_PLAY_TO_START);
  Serial.printf("Current player is \"%s\"\n", Settings.GetSourceName());
  Player.Start(Settings.Play);

  if(Settings.Play == 1 || Settings.NV.SourceAF == SET_SOURCE_BLUETOOTH) {
    Settings.FirstTime = 0;
  }
  else {
    Serial.println("Press \"Play\" to start");
    Display.ShowPlayPause(Settings.Play);
  }
  GuiInit();
}

//=====================================================================//
//== Main Loop                                                       ==//
//=====================================================================//

void loop(void) {
  static unsigned run_time = 0;

  TimeStamps.Update();
  if(TimeStamps.GetSecondTick()) {
    Settings.EepromStore();
  }
  if(TimeStamps.Get4SecTick()) { 
    Housekeeping();
  }

  Player.loop(TimeStamps.Get10msTick(), TimeStamps.GetSecondTick()); // runs also audio.loop() if applicable
  //Button logic
  SW1.tick();
  SW2.tick();
  SW3.tick();
  SW4.tick();
  SW5.tick();
  GUI_Handler(KEY_NONE);
  //Serial port commands
  if (Serial.available()) {
    String r = Serial.readStringUntil('\n');
    parse_command(r);
  }
}
