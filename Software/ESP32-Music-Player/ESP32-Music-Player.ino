// Compile with Espressif arduino-esp32 3.3.10 or lower (3.3.x); 
// Note: Espressif latest (3.3.11 & 3.3.12) overflows IRAM by 184 bytes
// audioI2SVers = 4.0.0g

#include "Arduino.h"
#include "rtc_wdt.h"

#include "src/Hardware/SSD1306.h"
#include "src/OneButton/OneButton.h"
#include "src/OneButton/OneButtonTiny.h"
#include "src/OneButton/OneButtonSmall.h"
#include "src/I2C_Scanner.h"
#include "src/SdCard.h"

#include "Display.h"
#include "Player.h"
#include "CommandHandler.h"
#include "GUI.h"

#define MyOneButton OneButtonSmall

DisplayClass        Display;
PlayerClass         Player;
CommandHandlerClass CommandHandler;

SettingsClass       Settings(I2C_PORT_EEPROM, I2C_LOW_CLOCK_SPEED);
SsidSettingsClass   SsidSettings;
UrlSettingsClass    UrlSettings;
TrackSettingsClass  TrackSettings;

MyOneButton SW2(PIN_SW2, true, false);
MyOneButton SW3(PIN_SW3, true, false);
MyOneButton SW4(PIN_SW4, true, false);
MyOneButton SW5(PIN_SW5, true, false);

#define BLUETOOTH_FILE_NAME "bluetooth.ini"
#define SSID_FILE_NAME      "ssid.ini"
#define WEBRADIO_FILE_NAME  "webradio.ini"

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
  ten_ms_tick           = false;
  second_tick           = false;
  four_seconds_tick     = false;
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
//== Local: Button handlers                                          ==//
//=====================================================================//

static void SW2_HandleClick(void)     { CommandHandler.GuiCommand(KEY_DN);        }
static void SW2_HandleLongClick(void) { CommandHandler.GuiCommand(KEY_LONG_DN);   }
static void SW3_HandleClick(void)     { CommandHandler.GuiCommand(KEY_UP);        }
static void SW3_HandleLongClick(void) { CommandHandler.GuiCommand(KEY_LONG_UP);   }
static void SW4_HandleClick(void)     { CommandHandler.GuiCommand(KEY_OK);        }
static void SW5_HandleClick(void)     { CommandHandler.GuiCommand(KEY_MENU);      }
static void SW5_HandleLongClick(void) { CommandHandler.GuiCommand(KEY_LONG_MENU); }

/*****************************************************************************************************************************************************************/

static bool read_setup_from_card() {
  if(MountSdCard()) {
   Serial.println("SD Card Mount Succeeded");
  }
  else { 
    Serial.println("SD Card Mount Failed, using SSID & Radio lists from EEPROM");
    Settings.V().NoCard = 1;
    return false;
  }
  if(Settings.NV().SourceAF == SET_SOURCE_SD_CARD) {
    TrackSettings.LoadFromCard("/", true);
  }
  if(Settings.NV().SourceAF == SET_SOURCE_WEB_RADIO) {
    if(!SsidSettings.LoadFromCard(SSID_FILE_NAME))
      Serial.println("Using SSID list from EEPROM");
    if(!UrlSettings.LoadFromCard(WEBRADIO_FILE_NAME))
      Serial.println("Using web station list from EEPROM");
    UnmountSdCard(); // webradio doesn't like a mounted SD Card
    Serial.println("SD Card Closed");
  }
  if(Settings.NV().SourceAF == SET_SOURCE_BLUETOOTH) {
    if(!Settings.LoadFromCard(BLUETOOTH_FILE_NAME))
      Serial.println("Using Bluetooth ID from EEPROM");
  }
  return true;
} 

static bool read_setup_from_eeprom() {
  if(Settings.NV().SourceAF == SET_SOURCE_WEB_RADIO) {
    SsidSettings.EepromLoad();
    UrlSettings.EepromLoad();
  }
  return true;
}

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

SET_LOOP_TASK_STACK_SIZE(16*1024 ); // 16KB; default = 8kB;

// https://github.com/dmody/ESP-32-Heap-Watchdog-Reliability/tree/main

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);  // 1000000 (defined in Board.h)
  delay(1000);

  InitSdCard();
  pinMode(PIN_CS, OUTPUT);
  pinMode(PIN_DC, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_DC, HIGH);
  SPI_PORT_OLED.begin(PIN_SCLK, PIN_MISO, PIN_MOSI);
  SPI_PORT_OLED.setFrequency(SPI_OLED_CLOCK_SPEED);
  Serial.printf("Arduino Loop Task Stack Size = %d\n", getArduinoLoopTaskStackSize());
  I2C_PORT_EEPROM.begin(PIN_SDA1, PIN_SCL1, I2C_LOW_CLOCK_SPEED);
  I2C_PORT_EEPROM.setBufferSize(256 + 8);
  // Buttons
  uint8_t  BootKeys = 0;
  if(digitalRead(PIN_SW1) == LOW) BootKeys |= BOOT_KEY1_MASK;
  if(digitalRead(PIN_SW2) == LOW) BootKeys |= BOOT_KEY2_MASK;
  if(digitalRead(PIN_SW3) == LOW) BootKeys |= BOOT_KEY3_MASK;
  if(digitalRead(PIN_SW4) == LOW) BootKeys |= BOOT_KEY4_MASK;
  if(digitalRead(PIN_SW5) == LOW) BootKeys |= BOOT_KEY5_MASK;
  Serial.printf("Button boot keys pressed at startup 0x%02X \n", BootKeys);

  if (BootKeys == 0x12) { // UP & MENU
    Settings.EepromErase();        // Use this when settings have got corrupted
  }
  Settings.EepromLoad(true);    // Get the primary settings from EEPROM
  Settings.V().BootKeys = BootKeys;
  Display.Initialize(Settings.isSoftRestart(), true);
  SW2.attachClick(SW2_HandleClick);
  SW2.attachLongPressStart(SW2_HandleLongClick);
  SW3.attachClick(SW3_HandleClick);
  SW3.attachLongPressStart(SW3_HandleLongClick);
  SW4.attachClick(SW4_HandleClick);
  SW5.attachClick(SW5_HandleClick);
  SW5.attachLongPressStart(SW5_HandleLongClick);
  if(Settings.isSoftRestart()) {   // A soft restart does not show the welcome screen, so it
    Display.ShowBaseScreen(Settings.V().Play ? DISPLAY_HELP_PLEASE_WAIT : DISPLAY_HELP_PRESS_PLAY_TO_START);
    Settings.clearSoftRestart();   // simulates an impression of a dynamically change of player.
    Settings.V().Play = 1;             // After a soft restart, we start playing immediately (Note: BT always starts immediately)
    if(Settings.NV().SourceAF == SET_SOURCE_WEB_RADIO) {
      read_setup_from_eeprom();
      if(Settings.NV().TotalSsid == 0 || Settings.NV().WebRadioTotalStations == 0)
        read_setup_from_card();
      Display.ShowVolume(); // Will also show station name
    }
    if(Settings.NV().SourceAF == SET_SOURCE_SD_CARD) {
      read_setup_from_card(); // (Re)load the track list
    }
  }
  else {
    TimeStamps.Update();
    uint32_t timestamp_now = TimeStamps.GetStamp10ms();
    Display.ShowWelcome();
    read_setup_from_eeprom(); // Read from EEPROM first...
    read_setup_from_card();   // ... read any necessary setup (changes) from sd card
    while(TimeStamps.GetStamp10ms() < timestamp_now + 300)  // Show welcome screen at least 3 seconds
      TimeStamps.Update();
    Settings.V().Play = Settings.NV().SourceAF == SET_SOURCE_BLUETOOTH; // Bluetooth is always "autoplay"
    Display.ShowBaseScreen(Settings.V().Play ? DISPLAY_HELP_PLEASE_WAIT : DISPLAY_HELP_PRESS_PLAY_TO_START);
  }
  if(Settings.NV().SourceAF == SET_SOURCE_BLUETOOTH) {
    Display.ShowHelpLine((const char *)Settings.NV().BtName);
    Display.ShowPlayPause(false);
  }
  Player.Init(); // OMT new
  //Settings.SetLoopFunction(SettingsLoopFunction);
  delay(50);
  Scan_I2C(Wire, 0);
  //if(oled_i2c_present)
  Scan_I2C(Wire1, 1);
  Serial.printf("Current player is \"%s\"\n", Settings.GetSourceName());
  CommandHandler.WifiConnect(false); // Connect in case of web radio
  CommandHandler.SetBtPower(Settings.NV().BtTxIndex);  // Set power in case of bluetooth player
  Serial.printf("RAM: %s\n", psramFound() ? "PSRAM" : "SRAM");
  GuiInit();
  if(Settings.NV().SourceAF == SET_SOURCE_WEB_RADIO) CommandHandler.GuiCommand(KEY_OK); // Always start webradio
}

//=====================================================================//
//== Main Loop                                                       ==//
//=====================================================================//

void loop() {
  rtc_wdt_protect_off();
  rtc_wdt_disable();  
  rtc_wdt_protect_on();

  TimeStamps.Update();
  if(TimeStamps.GetSecondTick()) {
    Settings.EepromStore();
  }
  if(TimeStamps.Get4SecTick()) { 
    Housekeeping();
  }
  Display.loop(TimeStamps.Get10msTick());
  Player.loop(TimeStamps.Get10msTick(), TimeStamps.GetSecondTick()); // runs also audio.loop() if applicable
  //Button logic
  SW2.tick();
  SW3.tick();
  SW4.tick();
  SW5.tick();
  CommandHandler.GuiCommand(KEY_NONE); // Check for updates from callback functions
  //Serial port commands
  if (Serial.available()) {
    String r = Serial.readStringUntil('\n');
    CommandHandler.SerialCommand(r);
  }
  vTaskDelay(1); // Audio can sound distoreted without this
}
