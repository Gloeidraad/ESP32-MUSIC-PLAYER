#include <arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <stdlib.h>
#include "Settings.h"
#include "Version.h"

#define NvData        ((unsigned char *)&_NvSettingsData[0])
#define NvMirrorData  ((unsigned char *)&_NvSettingsData[1])

#if EEPROM_I2C_ADDRESS_SIZE == 1
  #error THIS APPLICATION ACCEPS ONLY EEPROMS >= 32K
#endif

#ifdef USE_SD_MMC 
  #define SD_BEGIN() SD_MMC.begin("/sdcard", true)
  #define SD_OPEN    SD_MMC.open
  #define SD_END     SD_MMC.end
#else
  #define SD_BEGIN() SD.begin(PIN_SPI_SS, SPI, SPI_SD_SPEED)
  #define SD_OPEN    SD.open
  #define SD_END     SD.end
#endif

const char * const s_not_printable = "[Not Printable]";
const char * const s_none          = "[None]";

const uint8_t wifi_power_table[SET_WIFI_TX_MAX + 1] = {
  WIFI_POWER_19_5dBm,       // 78
  72, //WIFI_POWER_19dBm,   // 76 actual WIFI_POWER_18dBm = 72
  66, //WIFI_POWER_18_5dBm, // 74 actual WIFI_POWER_16_5dBm = 66
  //WIFI_POWER_17dBm,       // 68
  WIFI_POWER_15dBm,         // 60
  WIFI_POWER_13dBm,         // 52
  WIFI_POWER_11dBm,         // 44
  WIFI_POWER_8_5dBm,        // 34
  WIFI_POWER_7dBm,          // 28
  WIFI_POWER_5dBm,          // 20
  WIFI_POWER_2dBm,          // 8
};

//=======================================================
// Helper functions
//=======================================================

static void * settings_malloc(size_t size) { // Try to alloc in PSRAM if possible
  void * ptr;
  const char *s;
  if(psramFound()) {
    ptr = ps_malloc(size);
    s = "ps_malloc";
  }
  else {
    ptr = malloc(size);
    s = "malloc";
  }
  Serial.printf("settings_malloc %s: ptr=%lx, size=%d\n", s, ptr, size);
  return ptr;  
}

static void * settings_calloc(size_t n, size_t size) { // Try to alloc in PSRAM if possible
  void * ptr;
  const char *s;
  if(psramFound()) {
    ptr = ps_calloc(n, size);
    s = "ps_calloc";
  }
  else {
    ptr = calloc(n, size);
    s = "calloc";
  }
  Serial.printf("settings_calloc %s: ptr=%lx, n=%d, size=%d, bytes=%d\n", s, ptr, n, size, n*size);
  return ptr;  
}

static int SD_read_line(File &f, char * dest, int n) {
  memset(dest, 0, n);

  // See: https://stackoverflow.com/questions/65621607/read-line-by-line-in-file-and-store-into-string-flash-data-saving-spiffs
  int len = f.readBytesUntil('\n', dest, n);
  if (len && dest[len - 1] == '\r')
    dest[--len] = '\0'; // If a '\r' is present, remove it!
  return len;
}

#if 1
static void HEXR(uint8_t s[], int n) {
  for(int i = 0; i < n; i++)
    Serial.printf("%02X",s[i]);
  Serial.println();
}
#else
  #define HEXR(a,b)
#endif

#if 0
static void HEXP(uint8_t s[], int n) {
  for(int i = 0; i < n; i++)
    Serial.printf("%02X",s[i]);
  Serial.println();
}
#else
  #define HEXP(a,b)
#endif

inline static int Validate(int val, int min, int max, int def) {
  if(val < min || val > max)
    return def;
  return val;
}
inline static int Validate(int val, int min, int max) {
  if(val < min)
    return max;
  if (val > max)
    return min;
  return val;
}

TwoWire & EEPROM_I2C_PORT(void) { return *Settings._wire; }

#define WIRE EEPROM_I2C_PORT()

static void Read24Cxx(int addr, uint8_t * dat, int len) {
#if EEPROM_I2C_ADDRESS_SIZE == 2 // Only for 24C32 and larger
  WIRE.beginTransmission(EEPROM_I2C_DEVICE);
  WIRE.write(addr >> 8);
  WIRE.write(addr & 0xFF);
  if( WIRE.endTransmission() != 0) return;
  for(int i = 0; i < len; i+= 64) {
    if(len-i >= 64) {
       WIRE.requestFrom(EEPROM_I2C_DEVICE, 64, (int)(len-i == 64));
       WIRE.readBytes(&dat[i], 64);
    }
    else {
      WIRE.requestFrom(EEPROM_I2C_DEVICE, len-i, 1);
      WIRE.readBytes(&dat[i], len-i);
    }
  }
#endif
}

static void Read24Cxx(int page, int addr, uint8_t * dat, int len) {
#if EEPROM_I2C_ADDRESS_SIZE == 1
  WIRE.beginTransmission(EEPROM_I2C_DEVICE | (page & 7));
  WIRE.write(addr);
  if(WIRE.endTransmission() != 0) return;
  WIRE.requestFrom(EEPROM_I2C_DEVICE| (page & 7), len, 1);
  WIRE.readBytes(dat, len);
#else
  WIRE.beginTransmission(EEPROM_I2C_DEVICE);
  WIRE.write(page);
  WIRE.write(addr);
  if(WIRE.endTransmission() != 0) return;
  WIRE.requestFrom(EEPROM_I2C_DEVICE, len, 1);
  WIRE.readBytes(dat, len);
#endif
}

static void Write24Cxx(int page, int addr, uint8_t dat) {
  #if EEPROM_I2C_ADDRESS_SIZE == 1
    WIRE.beginTransmission(EEPROM_I2C_DEVICE | (page & 7));
  #else
    WIRE.beginTransmission(EEPROM_I2C_DEVICE);
    WIRE.write(page);
  #endif
  WIRE.write(addr);
  WIRE.write(dat);
  if(WIRE.endTransmission() != 0) return;
  delay(5);
}

static void Write24CxxPage(uint16_t addr, uint8_t * dat) {
  addr /= EEPROM_PAGE_SIZE; // Place on page boundary
  addr *= EEPROM_PAGE_SIZE;
  DEBUG_MSG_VAL("Writing EEPROM page at 0x%04X : ", addr);
  HEXR(dat, EEPROM_PAGE_SIZE);

  #if EEPROM_I2C_ADDRESS_SIZE == 1
    WIRE.beginTransmission(EEPROM_I2C_DEVICE | ((addr >> 8) & 7));
  #else
    WIRE.beginTransmission(EEPROM_I2C_DEVICE);
    WIRE.write(addr >> 8);
  #endif
  WIRE.write(addr & 0xFF);
  WIRE.write(dat, EEPROM_PAGE_SIZE);
  if(WIRE.endTransmission() != 0) return;
  delay(5);
}

// Write each byte individually. Optimizing is possible, but then you must consider the
// page size of the EEPROM. Page size could be 8,16,32,64,128 bytes. See datasheet for info.
static void Write24Cxx(int page, int addr, uint8_t * dat, int len) {
  for(int i = 0; i < len; i++)
    Write24Cxx(page, addr+i, dat[i]);
}

//==============================================================================
// Main settings
//   - NV stored in EEPROM page 0 @ 0x00-0x7F
//============================================================================== 

SettingsClass::SettingsClass(TwoWire &wire, uint32_t current_speed) {
  memset(this, 0, sizeof(SettingsClass));
  _prevspeed = current_speed;
  _wire      = &wire;
}

const char * SettingsClass::GetSourceName(uint8_t n) {
  static const char * player_name[SET_SOURCE_MAX + 1] =
    { "SD Card Player", "Webradio Player",  "Waveform Player",  "Bluetooth Player" };
  if(n == 0xFF) return(s_none);
  return player_name[n];
}

uint8_t SettingsClass::GetWifiTxLevel(int index) {
  if(index < 0)
    index = NV().WifiTxIndex;
  return index <= SET_WIFI_TX_MAX ? wifi_power_table[NV().WifiTxIndex] : 0;
}

const char * SettingsClass::GetWifiTxLevelDB(int index) {
  static char tx_in_db[6];
  if(index < 0)
    index = NV().WifiTxIndex;
  if(index > SET_WIFI_TX_MAX) {
    return "[ERR]";
  }
  index = wifi_power_table[index];
  itoa(index >> 2, tx_in_db, 10);
  if(index & 2)
    strcat(tx_in_db, ".5");
  return tx_in_db;
}

int8_t SettingsClass::GetBtTxLevel(int index) {
  if(index < 0)
    index = NV().BtTxIndex;
  return index <= SET_BT_TX_MAX ? 3 * (index - 4) : 9;
}

const char * SettingsClass::GetSourceName(void) { return GetSourceName(NV().SourceAF); }

void SettingsClass::doSoftRestart(void) {
  NV().MagicKey = SET_SOFT_RESET_MAGIC;
  EepromStore();
  esp_restart();
}

void SettingsClass::EepromLoad(bool may_use_psram) { // Must be called before any access to member data
  const char * subversion = SOFTWARE_SUBVERSION;
  DEBUG_MSG_VAL2("EEPROM type 24C%02d (size = %d bytes)\n", EEPROM_TYPE, EEPROM_SIZE);

  if(_SettingsData == NULL) {
    if(may_use_psram) _SettingsData = (SettingsData_t *)settings_calloc(1, sizeof(SettingsData_t));
    else              _SettingsData = (SettingsData_t *)calloc(1, sizeof(SettingsData_t));
  }
  if(_NvSettingsData == NULL) {
    if(may_use_psram) _NvSettingsData = (NvSettingsData_t *)settings_calloc(2, sizeof(NvSettingsData_t));
    else              _NvSettingsData = (NvSettingsData_t *)calloc(2, sizeof(NvSettingsData_t));
  }
  // Read from EEPROM
  SetWireClock();
  Read24Cxx(0, 0, NvData, sizeof(NvSettingsData_t));
  RestoreWireClock();
  if(NV().Version != SETTINGS_VERSION || NV().Subversion != subversion[0]) {
    memset(NvData, 0xFF, sizeof(NvSettingsData_t)); // New version, reset to defaults
  }
  memcpy(NvMirrorData, NvData, sizeof(NvSettingsData_t));
  NV().Version    = SETTINGS_VERSION;
  NV().Subversion = subversion[0];
#if 1
  // Check for valid entries
  NV().SourceAF         = Validate(NV().SourceAF,    0,                    SET_SOURCE_MAX,      SET_SOURCE_DEFAULT);
  NV().WaveformId       = Validate(NV().WaveformId,  SET_WAVE_FORM_MIN,    SET_WAVE_FORM_MAX,   SET_WAVE_DEFAULT);
  NV().WifiTxIndex      = Validate(NV().WifiTxIndex, 0,                    SET_WIFI_TX_MAX,      SET_WIFI_TX_DEFAULT);
  NV().BtTxIndex        = Validate(NV().BtTxIndex,   0,                    SET_BT_TX_MAX,       SET_BT_TX_DEFAULT);
  NV().VolumeSteps      = Validate(NV().VolumeSteps, SET_VOLUME_MIN,       SET_VOLUME_MAX,      SET_VOLUME_MAX);
  #if DAC_ID == DAC_ID_PT8211
    NV().Volume         = Validate(NV().Volume,      SET_VOLUME_MIN,       NV().VolumeSteps,      SET_VOLUME_DEFAULT);
  #else
    NV().Volume         = Validate(NV().Volume,      SET_VOLUME_MIN,       NV().VolumeSteps,      NV().VolumeSteps);
  #endif
  NV().OutputFormat     = Validate(NV().OutputFormat, 0, 1, 0);;
  NV().DiskTotalTracks  = Validate(NV().DiskTotalTracks, 0, SET_SD_TRACKS_MAX + 1, 0); 
  NV().DiskCurrentTrack = Validate(NV().DiskCurrentTrack, 1, NV().DiskTotalTracks, 1);
  NV().CurrentSsid      = Validate(NV().CurrentSsid, 0, SET_SSID_MAX, 0);
  NV().TotalSsid        = Validate(NV().TotalSsid,   0, SET_SSID_MAX + 1, 0);
  NV().WebRadioCurrentStation = Validate(NV().WebRadioCurrentStation, 1, SET_WEBRADIO_MAX, 1);
  NV().WebRadioTotalStations  = Validate(NV().WebRadioTotalStations,  0, SET_WEBRADIO_MAX, 0);

  if(NV().MagicKey != SET_SOFT_RESET_MAGIC)  NV().MagicKey = 0;
  if(NV().DiskTrackResumeTime == UINT32_MAX) NV().DiskTrackResumeTime = 0;
  if(NV().DiskTrackResumePos  == UINT32_MAX) NV().DiskTrackResumePos  = 0;
  if(NV().DiskTrackTotalTime  == UINT32_MAX) NV().DiskTrackTotalTime  = 0;
  if(NV().BtName[0] < 0x21 || NV().BtName[0] >= 127) {
    strcpy((char*)NV().BtName, BT_DEVICE_NAME);
  }
#endif
  // Save corrected entries if necessary
  EepromStore();
  // Init voltatiles
  _prevspeed           = 0;
  V().CurrentTrackTime = 0;
  V().TotalTrackTime   = 0;
  V().SeekTrackTime    = 0;
  V().AudioEnded       = 0;
  V().NoCard           = 1;
  V().Play             = 0;
  V().FirstTime        = 1;
  V().NewSourceAF      = NV().SourceAF;

}

void SettingsClass::EepromStore(void) {
  for(int i = 0; i < sizeof(NvSettingsData_t); i++) {
    if(NvMirrorData[i] != NvData[i]) {
      NvMirrorData[i] = NvData[i];
      SetWireClock();
      Write24Cxx(0, i, NvMirrorData[i]);
      RestoreWireClock();
      Serial.printf("EEPROM write %2i:%5u (%02X)\n", i, NvMirrorData[i], NvMirrorData[i]);
    }
  }
}

void SettingsClass::EepromErase(void) {
  uint8_t FF[EEPROM_PAGE_SIZE]; // Page size here is 32, so still no need to alloc in PSRAM
  memset(FF, 0xFF, EEPROM_PAGE_SIZE);
  SetWireClock();
  for(int i = 0; i < sizeof(NvSettingsData_t); i += EEPROM_PAGE_SIZE) {
    Write24CxxPage(i, FF); // Erase the contents
  }
  RestoreWireClock();
}

bool SettingsClass::LoadFromCard(const char * filename) {
  char fn[32]; // 32? No need to alloc in PSRAM
  if(_SettingsData == NULL || _NvSettingsData == NULL) {
    Serial.println("ERROR: Cannot read settings from file before reading from EEPROM");
    return false;
  }

  Serial.printf("Reading \"%s\"", filename);
  if(strlen(filename) > sizeof(fn) - 1) {
    Serial.println(" failed, file name too long");
    return false;
  }
  if(!SD_BEGIN()) {
    Serial.println("Card Mount Failed, Cannot read Bluetooth name");
    Settings.V().NoCard = 1;
    return false;
  }
  
  fn[0] = '/';
  strcpy(&fn[1], filename);
  File f = SD_OPEN(fn, FILE_READ);
  if (f == 0) {
    Serial.println(" failed to open file");
    return false;
  }
  if(!SD_read_line(f, (char *)NV().BtName, sizeof(NV().BtName))) 
    strcpy((char*)NV().BtName, BT_DEVICE_NAME); // failed: use default name  
  EepromStore();
  f.close();
  return true;
}

#define PRINT_SETTINGS_VALUE_DEC(v) Serial.printf("  " #v " = %d\n", v), loop()
#define PRINT_SETTINGS_VALUE_HEX(v) Serial.printf("  " #v " = 0x%02X\n", v), loop()
#define PRINT_SETTINGS_VALUE(v,f)   Serial.printf("  " #v " = %" #f "\n", v), loop()

void SettingsClass::Print(void) {
  Serial.printf("  Settings.NV().Version = %x.%02x\n", NV().Version >> 8, NV().Version & 0xFF); loop();
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().SourceAF);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().CurrentSsid);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().TotalSsid);
  PRINT_SETTINGS_VALUE_HEX(Settings.NV().MagicKey);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().WaveformId);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().WifiTxIndex);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().BtTxIndex);
  PRINT_SETTINGS_VALUE_HEX(Settings.NV().Subversion);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().DiskTotalTracks);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().DiskCurrentTrack);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().DiskTrackResumeTime);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().DiskTrackResumePos);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().DiskTrackTotalTime);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().WebRadioCurrentStation);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().WebRadioTotalStations);
  PRINT_SETTINGS_VALUE(Settings.NV().BtName,s);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().Volume);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().VolumeSteps);
  PRINT_SETTINGS_VALUE_DEC(Settings.NV().OutputFormat);
  
  PRINT_SETTINGS_VALUE(Settings.V().CurrentTrackTime,d);
  PRINT_SETTINGS_VALUE(Settings.V().TotalTrackTime,d);
  PRINT_SETTINGS_VALUE(Settings.V().SeekTrackTime,d);
  PRINT_SETTINGS_VALUE(Settings.V().BootKeys,d);
  PRINT_SETTINGS_VALUE(Settings.V().MenuId,d);
  PRINT_SETTINGS_VALUE(Settings.V().AudioEnded,d);
  PRINT_SETTINGS_VALUE(Settings.V().NoCard,d);
  PRINT_SETTINGS_VALUE(Settings.V().Play,d);
  PRINT_SETTINGS_VALUE(Settings.V().FirstTime,d);
  //PRINT_SETTINGS_VALUE(Settings.FirstSong,d);
  PRINT_SETTINGS_VALUE(Settings.V().NewSourceAF,d);
  PRINT_SETTINGS_VALUE(Settings.V().NewTrack,d);
  PRINT_SETTINGS_VALUE(Settings.V().NewTrackTimeSet,d);
  PRINT_SETTINGS_VALUE(Settings.V().TrackTitleReceived,d);
  PRINT_SETTINGS_VALUE(Settings.V().NewVolumeReceived,d);
  PRINT_SETTINGS_VALUE(Settings.V().NewPlayStatusReceived,d);
}

static uint16_t next_item(uint16_t current, uint16_t min, uint16_t max) {
  return current < max ? current + 1 : min;
}

static uint16_t prev_item(uint16_t current, uint16_t min, uint16_t max) {
  return current > min ? current - 1 : max;
}

void SettingsClass::NextPlayer(void) {
  Settings.NV().SourceAF = next_item(Settings.NV().SourceAF, SET_SOURCE_MIN, SET_SOURCE_MAX);
}

void SettingsClass::PrevPlayer(void) {
  Settings.NV().SourceAF = prev_item(Settings.NV().SourceAF, SET_SOURCE_MIN, SET_SOURCE_MAX);
}

void SettingsClass::PrevDiskTrack(void) {
  if(NV().DiskTotalTracks > 0) {
    NV().DiskCurrentTrack = prev_item(NV().DiskCurrentTrack, 1, NV().DiskTotalTracks);
  }
}

void SettingsClass::NextDiskTrack(void) {
  if(NV().DiskTotalTracks > 0) {
    NV().DiskCurrentTrack = next_item(NV().DiskCurrentTrack, 1, NV().DiskTotalTracks);
  }
}

void SettingsClass::PrevRadioStation(void) {
  if(NV().WebRadioTotalStations > 0) {
    NV().WebRadioCurrentStation = prev_item(NV().WebRadioCurrentStation, 1, NV().WebRadioTotalStations);
  }
}

void SettingsClass::NextRadioStation(void) {
  if(NV().WebRadioTotalStations > 0) {
    NV().WebRadioCurrentStation = next_item(NV().WebRadioCurrentStation, 1, NV().WebRadioTotalStations);
  }
}

void SettingsClass::NextWaveform(void) {
  NV().WaveformId = next_item(NV().WaveformId, SET_WAVE_FORM_MIN, SET_WAVE_FORM_MAX);
}

void SettingsClass::PrevWaveform(void) {
  NV().WaveformId = prev_item(NV().WaveformId, SET_WAVE_FORM_MIN, SET_WAVE_FORM_MAX);
}

void SettingsClass::UpdateCurrentTrackTime(int adj) {
  V().CurrentTrackTime = (int32_t)V().CurrentTrackTime + adj;
  if(V().CurrentTrackTime >= 100 * 3600)
    V().CurrentTrackTime = 0;
}

uint8_t SettingsClass::GetLogVolume(uint8_t v, uint8_t range) {
  if(NV().VolumeSteps == 0) return range;
  if(v > NV().VolumeSteps) v = NV().VolumeSteps;
  int logvol = (int)range * (int)v * (int)v / (int)NV().VolumeSteps / (int)NV().VolumeSteps;
  return logvol > v ? logvol : v;
}

uint8_t SettingsClass::GetLinVolume(uint8_t v, uint8_t range) {
  if(NV().VolumeSteps == 0) return range;
  if(v > NV().VolumeSteps) v = NV().VolumeSteps;
  int linvol = (int)range * (int)v / (int)NV().VolumeSteps;
  return linvol > v ? linvol : v;
}

//==========================================================
// SSID settings for webradio 
//   - ID       : stored in EEPROM page 0 @ 0x80 (128)
//   - Password : stored in EEPROM page 1
//   - SettingsClass Settings instance must also be present
//==========================================================

SsidSettingsClass::SsidSettingsClass() {
  memset(this, 0, sizeof(SsidSettingsClass));
}

SsidSettingsClass::~SsidSettingsClass() { free(_credentials); }

void SsidSettingsClass::EepromLoad(void) {
  if(_credentials == NULL) {
    _credentials = (wifi_credentials_t *)settings_calloc(SET_SSID_MAX+1, sizeof(wifi_credentials_t));
  }
  
  Settings.SetWireClock();
  for(int i = 0; i < Settings.NV().TotalSsid; i++) {
    Read24Cxx(EEPROM_SSID_PAGE,     i * EEPROM_SSID_SIZE + EEPROM_SSID_ADDRESS, _credentials[i].ssid, EEPROM_SSID_SIZE);
    Read24Cxx(EEPROM_PASSWORD_PAGE, i * EEPROM_PASSWORD_SIZE, _credentials[i].password, EEPROM_PASSWORD_SIZE);
  }
  Settings.RestoreWireClock();
  _valid = _credentials[0].ssid[0] != 0xFF;
}

void SsidSettingsClass::EepromStore(int n, wifi_credentials_t &c) {
  Settings.SetWireClock();
  if(memcmp(_credentials[n].ssid, c.ssid, EEPROM_SSID_SIZE) != 0) {
    HEXP(_credentials[n].ssid, EEPROM_SSID_SIZE);
    HEXP(c.ssid, EEPROM_SSID_SIZE);
    memcpy(_credentials[n].ssid, c.ssid, EEPROM_SSID_SIZE);
    Write24Cxx(EEPROM_SSID_PAGE, n * EEPROM_SSID_SIZE + EEPROM_SSID_ADDRESS, c.ssid, EEPROM_SSID_SIZE);
    Serial.printf("Saving SSID[%d]: %s \n", n, c.ssid);
  }
  if(memcmp(_credentials[n].password, c.password, EEPROM_PASSWORD_SIZE) != 0) {
    memcpy(_credentials[n].password, c.password, EEPROM_PASSWORD_SIZE);
    Write24Cxx(EEPROM_PASSWORD_PAGE, n * EEPROM_PASSWORD_SIZE, c.password, EEPROM_PASSWORD_SIZE);
    Serial.printf("Saving PASS[%d]: %s \n", n, "********"/*c.password*/);
  }
  Settings.RestoreWireClock();
}

bool SsidSettingsClass::LoadFromCard(const char * filename) {
  char fn[32]; // 32? No need to alloc in PSRAM
  Serial.printf("Reading \"%s\"", filename);
  if(strlen(filename) > sizeof(fn) - 1) {
    Serial.println(" failed, file name too long");
    return false;
  }
  
  if(!SD_BEGIN()) {
    Serial.println("Card Mount Failed, Cannot read SSID list");
    Settings.V().NoCard = 1;
    return false;
  }

  fn[0] = '/';
  strcpy(&fn[1], filename);
  File f = SD_OPEN(fn, FILE_READ);
  if (f == 0) {
    Serial.println(" failed to open file");
    return false;
  }
  int n1, n2;
  wifi_credentials_t * wifi_credentials = (wifi_credentials_t *)settings_calloc(1, sizeof(wifi_credentials_t));
  Settings.NV().TotalSsid = 0;
  Serial.print(": ");
  for (int i = 0; i <= SET_SSID_MAX; i++) {
    n1 = SD_read_line(f, (char *)wifi_credentials->ssid, sizeof(wifi_credentials->ssid));
    n2 = SD_read_line(f, (char *)wifi_credentials->password, sizeof(wifi_credentials->password));
    if (n1 && n2) {
      EepromStore(Settings.NV().TotalSsid++, *wifi_credentials);
    }
  }
  Serial.printf("Found %d SSIDs\n", Settings.NV().TotalSsid);
  if (Settings.NV().CurrentSsid >= Settings.NV().TotalSsid)
    Settings.NV().CurrentSsid = 0;
  Settings.EepromStore();
  f.close();
  free(wifi_credentials);
  return true;
}

void SsidSettingsClass::Print(bool showpassword) { // For debug purposes
  if(Settings.NV().TotalSsid == 0) {
    Serial.println("No SSIDs Defined");
    return;
  }
  for(int i = 0; i < Settings.NV().TotalSsid; i++) {
    bool printable = isPrintable(_credentials[i].ssid[0]);
    Serial.printf("SSID: \"%s\", PASS: \"", printable ? (char *)_credentials[i].ssid : s_not_printable);
    if(showpassword) {
      printable = isPrintable(_credentials[i].password[0]);
      Serial.print(printable ? (char *)_credentials[i].password : s_not_printable);
    }
    else
      Serial.print("********");
    Serial.println("\"");
    Settings.loop(); // Keep critical function running
  }
}

const char * SsidSettingsClass::GetSsid(int n, bool use_zero_length) { 
  const char *ssid = (const char *)_credentials[n].ssid;
  return use_zero_length || ssid[0] != '\0' ? ssid : s_none;
}

//==========================================================
// URL settings for webradio 
//   - stored in EEPROM page 2+
//   - SettingsClass Settings instance must also be present
//==========================================================

UrlSettingsClass::UrlSettingsClass() {
  _valid = 0;
  _ptr   = 0;
  _index = 0;
  _data = NULL;
}

void UrlSettingsClass::EepromLoad(void) {
  Settings.SetWireClock();
  if(_data == NULL)
    _data = (uint8_t *)settings_calloc(1, EEPROM_WEB_LIST_SIZE);
  Read24Cxx(EEPROM_WEB_ADDRESS, _data, EEPROM_WEB_LIST_SIZE);
  _valid = 1;
  Settings.RestoreWireClock();
}

void UrlSettingsClass::EepromStore(void) {
  // We must use page writes here.
  // Otherwise writing 7680 (when using 24C64) individual bytes,
  // will take at least 7680 * 5ms = 38 seconds
  uint8_t buf[EEPROM_PAGE_SIZE]; // Page size here is 32, so still no need to alloc in PSRAM
  if(_data == NULL) return; 
  Settings.SetWireClock();
  for(int i = 0; i < EEPROM_WEB_LIST_SIZE; i += EEPROM_PAGE_SIZE) {
    int addr = i + EEPROM_WEB_ADDRESS;
    Read24Cxx(addr, buf, EEPROM_PAGE_SIZE); // Read the current contents
    if(memcmp(buf, &_data[i], EEPROM_PAGE_SIZE) != 0) {
      Write24CxxPage(addr, &_data[i]); // Write the (new) contents
    }
  }
  Settings.RestoreWireClock();
}
 
bool UrlSettingsClass::LoadFromCard(const char * filename) {
  char fn[32]; // 32? No need to alloc in PSRAM

  if(_data == NULL)
    _data = (uint8_t *)settings_calloc(1, EEPROM_WEB_LIST_SIZE);
  Serial.printf("Reading \"%s\"", filename);
  if(strlen(filename) > sizeof(fn) - 1) {
      Serial.println(" failed, file name too long");
      return false;
  }
  
  if(!SD_BEGIN()) {
    Serial.println("Card Mount Failed, Cannot read station list");
    Settings.V().NoCard = 1;
    return false;
  }

  fn[0] = '/';
  strcpy(&fn[1], filename);
  File f = SD_OPEN(fn, FILE_READ);
  if (f == 0) {
    Serial.println(" failed to open file");
    return false;
  }
  Serial.print(": ");
  int n;
  uint8_t buff[256];
  Settings.NV().WebRadioTotalStations  = 0;
  _ptr = _index = 0;
  while((n = SD_read_line(f, (char *)buff, sizeof(buff))) != 0) {
    n++; // We include the terminating zero
    if(_ptr + n <= EEPROM_WEB_LIST_SIZE) { // Check if it fits in the list
      memcpy(&_data[_ptr], buff, n);
      _ptr += n;
      Settings.NV().WebRadioTotalStations++;
    }
  }
  Serial.printf("Found %d stations, using %d bytes of %d in list.\n", Settings.NV().WebRadioTotalStations, _ptr, EEPROM_WEB_LIST_SIZE);
  if (Settings.NV().WebRadioCurrentStation > Settings.NV().WebRadioTotalStations)
    Settings.NV().WebRadioCurrentStation = 1;
  EepromStore();
  Settings.EepromStore();
  _valid = 1;
  f.close();
  return true;
}

void UrlSettingsClass::Print(bool raw) {
  uint16_t old_ptr   = _ptr;
  uint16_t old_index = _index;
  web_station_t stat;
  if(_data == NULL) return; 
  if(Settings.NV().WebRadioTotalStations == 0 || !_valid) {
    Serial.println("No Radio URLs Defined");
    return;
  }
  _ptr = _index = 0;
  for(int i = 0; i < Settings.NV().WebRadioTotalStations; i++) {
    if(raw) {
      Serial.println(isPrintable(_data[_ptr]) ? (char *)&_data[_ptr] : s_not_printable);
    }
    else {
      GetStation(stat);
      Settings.loop(); // Keep critical function running
      Serial.printf("[%d] = %-16s: %s\n", _index + 1, stat.name,stat.url);
    }
    NextStation();
    Settings.loop();   // Keep critical function running
  }
  _ptr   = old_ptr;
  _index = old_index;
}

void UrlSettingsClass::NextStation(void) {
  if(_data == NULL) return; 
  if(isPrintable(_data[_ptr])) {
    _ptr += strlen((char *)&_data[_ptr]) + 1;
  }
  _index++;
}

void UrlSettingsClass::GetStation(web_station_t & web_station) {
  if(_data == NULL) return; 
  if(!isPrintable(_data[_ptr])) {
    sprintf(web_station.name, "[%d]", _index + 1);
    web_station.namesize = strlen(web_station.name);
    web_station.url = s_not_printable;
    web_station.urlsize = strlen(web_station.url);
    return;
  }

  char * p1 = (char *)&_data[_ptr];
  char * p2 = strchr(p1, '|');

  if(p2 != NULL) {
    web_station.namesize = p2 - p1;
    if(web_station.namesize > EEPROM_WEB_NAME_SIZE)
       web_station.namesize = EEPROM_WEB_NAME_SIZE;
    strncpy(web_station.name, p1, web_station.namesize);
    web_station.name[web_station.namesize] = 0; // terminate with 0
    web_station.url = p2 + 1; // skip the '|
    web_station.urlsize = strlen(web_station.url);
  }
  else {
    sprintf(web_station.name, "[%d]", _index + 1);
    web_station.namesize = strlen(web_station.name);
    web_station.url = p1;
    web_station.urlsize = strlen(p1);
  }
}

void UrlSettingsClass::GetStation(uint16_t idx, web_station_t & web_station) {
  if(_data == NULL) return; 
  if(Settings.NV().WebRadioTotalStations > 0) {
    _ptr = _index = 0;
    for(int i = 1; i < idx && i <= Settings.NV().WebRadioTotalStations; i++) {
      NextStation();
      Settings.loop(); // Keep critical function running
    }
    GetStation(web_station);
  }
}

//==========================================================
// MP3 Track list 
//   - SettingsClass Settings instance must also be present
//==========================================================

bool TrackSettingsClass::LoadFromCard(const char * dir, bool keep_mounted) {
  if(_tracks == NULL) { 
    #if 0
    if(psramFound()) {
      _list_max_size = SET_SD_TRACKS_PS_SIZE;
      _tracks = (uint8_t *)ps_malloc(SET_SD_TRACKS_PS_SIZE);
    }
    else {
      _list_max_size = SET_SD_TRACKS_SIZE;
      _tracks = (uint8_t *)malloc(SET_SD_TRACKS_SIZE);
    }
    #else
    _list_max_size = psramFound() ? SET_SD_TRACKS_PS_SIZE : SET_SD_TRACKS_SIZE;
    _tracks = (uint8_t *)settings_malloc(_list_max_size);
    #endif
  }
  memset(_tracks, 0, _list_max_size);
  
  _number_of_tracks = 0;
  _list_size        = 0;
   
  if(!SD_BEGIN()) {
    Serial.println("Card Mount Failed, Cannot read track list");
    Settings.V().NoCard = 1;
    return false;
  }
  Settings.V().NoCard = 0;
  File _root;
  File _file;
  Serial.printf("Counting audio files ... ");
//for(int ff = 0; ff <10; ff++) // test by 10x number of files
{
  _root = SD_OPEN(dir);
  if (!_root || !_root.isDirectory()) {
    Serial.println("failed to open root directory");
    return false;
  }
  _file = _root.openNextFile();
  while (_file && _list_size < _list_max_size) {
    if (!_file.isDirectory()) {
      String temp = _file.name();
      temp.toLowerCase();
      if (temp.endsWith(".wav") || temp.endsWith(".mp3")) {
        int f_len = temp.length() + 1; // include the terminal 0
        if(_list_size + f_len < _list_max_size) {
          _number_of_tracks++;
          strcpy((char *)_tracks + _list_size, _file.name());
          _list_size += f_len;
        }   
      }
    }
    _file = _root.openNextFile();
  }
  _root.close();
}
  if(_number_of_tracks > 0)
    Serial.printf("found %d audio files, (list memory usage %d of %d, average file name size %d)\n", _number_of_tracks, _list_size, _list_max_size, _list_size / _number_of_tracks);
  else
    Serial.println("no audio files found on card");
  Settings.NV().DiskTotalTracks = _number_of_tracks;
  if (Settings.NV().DiskCurrentTrack > Settings.NV().DiskTotalTracks) {
    Settings.NV().DiskCurrentTrack = 1;
    Serial.println("Reset DiskCurrentTrack");
  }
  Settings.EepromStore();
  if(!keep_mounted)
    SD_END(); // webradio doesn't like a mounted SD Card
  return true;  
}

void TrackSettingsClass::Print(void) {
  if(Settings.V().NoCard) {
    Serial.println("No card found");
    return;
  }
  if(_number_of_tracks == 0) {
    Serial.println("Track list is empty");
    return;
  }
  char * p = (char *)_tracks;
  for(int i = 0; i < _number_of_tracks; i++) {
    Serial.printf("[%d] = %s\n", i + 1, p);
    p += strlen(p) + 1;
    if(_loop != NULL) _loop();
  }
}

const char * TrackSettingsClass::GetTrackName(uint16_t  n) {
  if(n > _number_of_tracks) return "";
  char * p = (char *)_tracks;
  for(int i = 1; i < n; i++) {
    p += strlen(p) + 1;
  }
  return p;
}
