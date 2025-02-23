#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stdint.h>
#include <Wire.h>

#include "Hardware/board.h" // For macro USE_SD_MMC

#ifdef USE_SD_MMC
  #include <SD_MMC.h>
#else
  #include <SD.h>
#endif

//=======================================================
// EEPROM specification
//=======================================================

#define BT_DEVICE_NAME   "ESP32 AM/FM Transmitter"

#define EEPROM_TYPE      64 // 24Cxx where xx is EEPROM_SIZE
#define EEPROM_PAGE_SIZE 32 // in bytes, check datasheet for value

#if EEPROM_TYPE <= 16
  #define EEPROM_I2C_ADDRESS_SIZE     1  // 1 for 24C02 through 24C16
#else
  #define EEPROM_I2C_ADDRESS_SIZE     2  // 2 for 24C32 and larger
#endif

#if EEPROM_TYPE == 0
  #define EEPROM_SIZE        64
  #define EEPROM_BLOCK_SIZE  64
#elsif EEPROM_TYPE == 1
  #define EEPROM_SIZE       128
  #define EEPROM_BLOCK_SIZE 128
#else
  #define EEPROM_SIZE (256 * EEPROM_TYPE / 2)
  #if EEPROM_I2C_ADDRESS_SIZE == 1
    #define EEPROM_BLOCK_SIZE 256
  #else
    #define EEPROM_BLOCK_SIZE EEPROM_SIZE
  #endif

#endif

#define EEPROM_I2C_DEVICE         0x50
#define EEPROM_I2C_CLOCK_SPEED  100000  // Some EEPROMs work only at 100kHz

#define EEPROM_SETTINGS_PAGE         0
#define EEPROM_SETTINGS_ADDR         0

//=======================================================
// Main settings
//   - NV stored in EEPROM page 0 @ 0x00-0x7F
//   - Make sure sizeof(NV) stays below 128
//=======================================================

#define SET_SOURCE_MIN            0
#define SET_SOURCE_MAX            3

#define SET_SOURCE_SD_CARD        0
#define SET_SOURCE_WEB_RADIO      1
#define SET_SOURCE_WAVE_GEN       2
#define SET_SOURCE_BLUETOOTH      3
#define SET_SOURCE_DEFAULT        SET_SOURCE_SD_CARD

#define SET_WAVE_FORM_MIN         0
#define SET_WAVE_FORM_MAX         4 //3: only waveforms, 4: plus DAC test, 6: plus DAC debug
#define SET_WAVE_SINE_440HZ       0
#define SET_WAVE_SINE_1KHZ        1
#define SET_WAVE_TRI_440HZ        2
#define SET_WAVE_TRI_1KHZ         3
#define SET_WAVE_DEFAULT          SET_WAVE_SINE_440HZ

#define SET_SSID_MAX               3  //max 4 networks

#define SET_WIFI_TX_MAX            9  // max wifi tx level (index)
#define SET_WIFI_TX_DEFAULT        0  // Points to highest level

#define SET_BT_TX_MAX              7  // max bluetooth tx level (index)
#define SET_BT_TX_DEFAULT          5  // +3dbm 

#define SET_WEBRADIO_MAX         199  // By far enough
//#define SET_SD_TRACKS_MAX        299 // should be by far enough
//#define SET_SD_TRACKS_MAX        99  // should be by far enough
#define SET_SD_TRACKS_MAX        5000  // should be by far enough
#define SET_SD_TRACKS_SIZE      5000//10000  // should be enough for about 200 tracks, depending on the lengths of the file names
#define SET_SD_TRACKS_PS_SIZE  128000  // should be enough for about 2500 tracks, depending on the lengths of the file names

#define SET_SOFT_RESET_MAGIC     0x5A    //  If this is set, the reset procedure will be different.

// Use the same values as Audio class, see:
// https://github.com/schreibfaul1/ESP32-audioI2S/wiki#how-to-adjust-the-balance-and-volume
#define SET_VOLUME_MIN          1
#define SET_VOLUME_MAX         21
#define SET_VOLUME_DEFAULT      7

typedef struct {
#pragma pack(1) // place at 8 bit boundaries
  uint16_t Version;         // 
  uint8_t  SourceAF;        // 0 = SD-Card, 1 = web radio, 2 = Tone generator, 3 = Bluetooth audio
  uint8_t  CurrentSsid;     // 0,1,2,3:  We can connect to up to four wifi access points
  uint8_t  TotalSsid;       // Valid number of SSISs
  uint8_t  MagicKey;        // When set to SET_SOFT_RESET value, the ESP32 wil perform a restart.
  uint8_t  WaveformId;      // See "SimpleWaveGenerator.h" for the list of wave forms
  uint8_t  WifiTxIndex;     // Index in wifi tabel with TX levels
  uint8_t  BtTxIndex;       // Index in bluetooth tabel with TX levels
  uint8_t  Subversion;      // expansion of Version
  uint16_t DiskTotalTracks;
  uint16_t DiskCurrentTrack;
  uint32_t DiskTrackResumeTime;
  uint32_t DiskTrackResumePos;
  uint32_t DiskTrackTotalTime;
  uint16_t WebRadioCurrentStation;
  uint16_t WebRadioTotalStations;
  uint8_t  Volume;         // Volume settings only for debugging puposes;
  uint8_t  VolumeSteps;
  uint8_t  OutputFormat; 
  uint8_t  BtName[32];
#pragma pack()
} NvSettings_t;

class SettingsClass {

  public:

    friend TwoWire & EEPROM_I2C_PORT(void);
    friend class SsidSettingsClass;
    friend class UrlSettingsClass;

    SettingsClass(TwoWire &wire = Wire, uint32_t current_speed = 400000);
    uint8_t      GetCurrentSourceId(void) { return NV.SourceAF; }
    const char * GetSourceName(void);
    const char * GetSourceName(uint8_t n);

    void SetLoopFunction(void (*f)(void)) { _loop = f; }
    void EepromLoad(void);
    void EepromStore(void);
    void EepromErase(void);
    bool LoadFromCard(const char * filename);

    void Print(void);

    void doSoftRestart(void);
    bool isSoftRestart(void)    { return NV.MagicKey == SET_SOFT_RESET_MAGIC; }
    void clearSoftRestart(void) { NV.MagicKey = 0; EepromStore(); }

    void NextPlayer(void);
    void PrevPlayer(void);

    void PrevDiskTrack(void);
    void NextDiskTrack(void);
    void PrevRadioStation(void);
    void NextRadioStation(void);

    void NextWaveform(void);
    void PrevWaveform(void);
    void AdjustOutputSelect(int adj = 0); // Valid values are -1, 0, or 1

    void UpdateCurrentTrackTime(int adj = 1);
    
    uint8_t GetLogVolume(uint8_t range) { return GetLogVolume(NV.Volume, range); }
    uint8_t GetLogVolume(uint8_t v, uint8_t range);
    uint8_t GetLinVolume(uint8_t range) { return GetLinVolume(NV.Volume, range); }
    uint8_t GetLinVolume(uint8_t v, uint8_t range);

    uint8_t GetWifiTxLevel(int index = -1);
    int8_t  GetBtTxLevel(int index = -1);
    const char * GetWifiTxLevelDB(int index = -1);

    // non-volatiles start here
    NvSettings_t NV;
    // volatiles start here
    uint32_t CurrentTrackTime;
    uint32_t TotalTrackTime;
    uint32_t SeekTrackTime;
    uint8_t  BootKeys;
    uint8_t  MenuId;  // 0 = player, > 0 = menu id
    uint8_t  AudioEnded;
    uint8_t  NoCard;
    uint8_t  Play;
    uint8_t  FirstTime;
    uint8_t  FirstSong;
    uint8_t  NewSourceAF;
    uint8_t  NewTrack;
    uint8_t  WebTitleReceived;

  protected:

    void SetWireClock(void)     { if(_prevspeed != EEPROM_I2C_CLOCK_SPEED) Wire.setClock(EEPROM_I2C_CLOCK_SPEED); }
    void RestoreWireClock(void) { if(_prevspeed != EEPROM_I2C_CLOCK_SPEED) Wire.setClock(_prevspeed); }
    void loop(void)             { if(_loop) _loop(); }

    TwoWire * _wire;
    uint32_t _prevspeed;
    void (*_loop)(void);
    NvSettings_t NvMirror;
};

extern SettingsClass Settings;

//==========================================================
// SSID settings for webradio
//   - ID       : stored in EEPROM page 0 @ 0x80 (128)
//   - Password : stored in EEPROM page 1
//   - SettingsClass Settings instance must also be present
//==========================================================

#define EEPROM_SSID_PAGE       0
#define EEPROM_SSID_ADDRESS  128
#define EEPROM_PASSWORD_PAGE   1
#define EEPROM_SSID_SIZE      32
#define EEPROM_PASSWORD_SIZE  63

#pragma pack(1) // place at 8 bit boundaries

typedef struct {
  uint8_t ssid[EEPROM_SSID_SIZE+1];         // +1 for terminating zero
  uint8_t password[EEPROM_PASSWORD_SIZE+1]; // +1 for terminating zero
} wifi_credentials_t;

#pragma pack()

class SsidSettingsClass {

  public:

    SsidSettingsClass();
    void EepromLoad(void);
    void EepromStore(int n, wifi_credentials_t &);
    bool LoadFromCard(const char * filename);

    void Print(bool showpassword = false);
    const char * GetSsid(int n, bool use_zero_length = true);
    const char * GetPassword(int n) { return (const char *)_credentials[n].password; }

  protected:
    uint8_t   _valid;
    wifi_credentials_t _credentials[SET_SSID_MAX+1];
};

extern SsidSettingsClass SsidSettings;

//==========================================================
// URL settings for webradio
//   - stored in EEPROM page 2+
//   - SettingsClass Settings instance must also be present
//==========================================================

#define EEPROM_WEB_ADDRESS    (2 * 256)  // location of first entry
#define EEPROM_WEB_LIST_SIZE  (EEPROM_SIZE - EEPROM_WEB_ADDRESS)
#define EEPROM_WEB_NAME_SIZE  16  // Your personal station name
#define EEPROM_WEB_URL_SIZE   256 // The web address of the station

typedef struct {
  char name[EEPROM_WEB_NAME_SIZE+1];
  uint8_t namesize;
  const char * url;
  uint16_t  urlsize;
} web_station_t;

class UrlSettingsClass {

  public:

    UrlSettingsClass();
    void EepromLoad(void);
    bool LoadFromCard(const char * filename);

    void Print(bool raw = false);
    void GetStation(uint16_t idx, web_station_t & web_station);

  protected:
    void EepromStore(void);
    void GetStation(web_station_t & web_station);
    void NextStation(void);

  protected:

    uint8_t  _valid;
    uint16_t _ptr;
    uint16_t _index;
    uint8_t  _data[EEPROM_WEB_LIST_SIZE];
};

extern UrlSettingsClass UrlSettings;

//==========================================================
// MP3 Track list
//   - SettingsClass Settings instance must also be present
//==========================================================

class TrackSettingsClass {
  public:
    TrackSettingsClass()  { _tracks = NULL; _loop = NULL; }
    ~TrackSettingsClass() { if(_tracks != NULL) delete _tracks; }
    void AddLoopFunction(void (*f)(void)) { _loop = f; }

    bool LoadFromCard(bool keep_mounted = false);
    void Print(void);
    const char * GetTrackName(uint16_t n); // Note: n = 1 .. _number_of_tracks
    bool IsValid(void) { return _tracks != NULL && _number_of_tracks > 0; }

  protected:

    uint8_t * _tracks;
    uint32_t  _number_of_tracks;
    uint32_t  _list_size;
    uint32_t  _list_max_size;
    void    (*_loop)(void);
};

extern TrackSettingsClass TrackSettings;

#endif
