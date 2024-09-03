#include "src/ChipInfo.h"
#include "esp_wifi.h"
#include "Display.h"
#include "SimpleWaveGenerator.h"
#include "Player.h"

#if ESP_IDF_VERSION_MAJOR == 5
#include "ESP_I2S.h" 
#endif

#define BT_ACTIVITY_TIMER   10  // in 10 ms slots

#define BLUETOOTH_VOLUME_MIN  24
#define BLUETOOTH_VOLUME_MAX  127

static void AudioDieOut(Audio * _audio, int loops = 100) {
  if(_audio == NULL) return;
  for(int i = 0; i < loops; i++) {
    uint64_t timestamp_us = esp_timer_get_time();
    while(esp_timer_get_time() < timestamp_us + 1000) { // every millisecond
      _audio->loop();
    }
  }
}

//******************************************************//
// Constructor                                          //
//******************************************************//

PlayerClass::PlayerClass() {
  _loop             = NULL;
  _audio            = NULL;
  _a2dp_sink        = NULL;
  _waveform_player  = NULL;
  _activity_counter = 0;
  _activity_sum     = 1;
  _connected        = false;
}

//******************************************************//
// SD Card Player                                       //
//******************************************************//

void PlayerClass::SD_PrintDebug(void) {
  if(_audio != NULL) {
    Serial.printf("getAudioCurrentTime : %d\n", _audio->getAudioCurrentTime());
    Serial.printf("getAudioFileDuration: %d\n", _audio->getAudioFileDuration());
    Serial.printf("getSampleRate       : %d\ngetBitsPerSample    : %d\n", _audio->getSampleRate(), _audio->getBitsPerSample());
    Serial.printf("getFileSize         : %d\ngetFilePos          : %d\n", _audio->getFileSize(), _audio->getFilePos());
    Serial.printf("getBitRate          : %d\n", _audio->getBitRate());
    Serial.printf("getChannels         : %d\n", _audio->getChannels());
  }
}

bool PlayerClass::PlayTrackFromSD(int n, uint32_t resume_pos, uint32_t resume_time, uint32_t track_time, uint32_t total_time) {
  _audio->stopSong();
  if(n == 0 || n > Settings.NV.DiskTotalTracks) {
    Serial.printf("Illegal track number: 0 < n < %d\n", Settings.NV.DiskTotalTracks + 1);
    return false;
  }
  String track(TrackSettings.GetTrackName(n));
  Serial.printf("Starting [%d]: \"%s\" at %d (%ds)\n", n, track.c_str(), resume_pos, resume_time);
  Display.ShowTrackTitle(track.c_str());
  #ifdef USE_SD_MMC
    _audio->connecttoFS(SD_MMC, track.c_str(), resume_pos);
  #else
    _audio->connecttoFS(SD, track.c_str(), resume_pos);
  #endif
  // Fix positioning if resume_pos fails (run until AudioCurrentTime is valid, then reposition if necessary)
  #if 1
  if(resume_time > 15) {
    uint32_t time  = millis();
    uint32_t cur, len;
    uint16_t ms100 = 0;
    while(ms100 < AUDIO_HEADER_TIMEOUT/100) { 
      _audio->loop();
      if(millis() > time + 100) {
        cur = _audio->getAudioCurrentTime();
        if(cur > 0) {
          len = _audio->getAudioFileDuration();
          Serial.printf("Resume found = %d/%d after %d00 ms\n", cur, len, ms100);
          break;
        }
        ms100++;
        time += 100;
      }
    }
    #if 0
    if(cur < resume_time - 15) { // 15 seconds off is accpetable
      while(millis() > time + 1000) // Run for another second
        _audio->loop();
      Serial.printf("Reposition %d to %d\n", cur, resume_time);
      _audio->setAudioPlayPosition(resume_time);
    }
    #endif
  }
  // end fix
  #endif
  Settings.AudioEnded             = 0;
  Settings.NV.DiskCurrentTrack    = n;
  Settings.NV.DiskTrackResumePos  = resume_pos;
  Settings.NV.DiskTrackResumeTime = resume_time;
  Settings.CurrentTrackTime       = resume_time; 
  Settings.TotalTrackTime         = track_time;  // Reset: Will be adjusted later, see loop()
  return true;
}

void PlayerClass::PositionEntry(void) {
  if(Settings.CurrentTrackTime == Settings.SeekTrackTime) {
    Serial.println("SD time not changed");
  }
  else {
    Serial.printf("SD time changed from %d to %d\n", Settings.CurrentTrackTime, Settings.SeekTrackTime);
    Settings.CurrentTrackTime = Settings.SeekTrackTime;
    _audio->setAudioPlayPosition(Settings.SeekTrackTime);
    Settings.NV.DiskTrackResumeTime = Settings.SeekTrackTime;
    //Settings.NV.DiskTrackResumePos  = _audio->getFilePos() - _audio->inBufferFilled();
    Display.ShowPlayTime(true); //OMT: new
  }
  
  if(Settings.Play)
    _audio->pauseResume(); // continue playing
}

//******************************************************//
// All Players                                          //
//******************************************************//

static void avrc_metadata_callback(uint8_t attribute, const uint8_t *data) {
  switch(attribute) {
    case 0x01: Serial.printf("AVRC metadata: title \"%s\"\n", data);
               Settings.CurrentTrackTime = -1; 
               Display.ShowTrackTitle(strlen((const char *)data) > 0 ? (const char *)data : (const char *)Settings.NV.BtName);
               break;
    default  : Serial.printf("AVRC metadata: 0x20  rsp: attribute id 0x%x, %s\n", attribute, data); break;
  }
}

static void bt_volumechange(int v) { 
  Serial.printf("bt_volumechange: %d\n",v);
  v = Player.CheckBluetoothVolume(v);
  for(int v2 = 1; v2 <= Settings.NV.VolumeSteps; v2++) {
    if(Settings.GetLinVolume(v2, BLUETOOTH_VOLUME_MAX) >= v) {
      Display.ShowVolume(v2, Settings.NV.VolumeSteps);
      return;
    }
  }
}

//static bool address_validator(esp_bd_addr_t remote_bda) { return true; }
//static void sample_rate_callback(uint16_t rate) { Serial.printf("sample_rate_callback: %d\n", rate); }
//static void rssi_callback(esp_bt_gap_cb_param_t::read_rssi_delta_param &rssi) { Serial.printf("rssi_callback\n"); }

void bt_raw_stream_reader(const uint8_t* pdata, uint32_t len) { 
  Player._activity_counter = BT_ACTIVITY_TIMER;  
  static int i = 0;
  if(len < 100) return; // Ignore small packets
  if(!(++i & 0xF)) { // We examine the data only once in 16 times, to save some processor performance
    const uint16_t *p = (const uint16_t *)pdata;
    uint32_t sum = 0; 
    for(int j = 0; j < len/2; j++) sum += *p++;
    Player._activity_sum = sum;
    #if 0
    //Serial.printf("%d ", len);
    //Serial.printf("%-2d", Player._activity_sum != 0);
    Serial.printf("%8d ", Player._activity_sum);
    if(!(i & 0xFF))
      Serial.println();
    #endif
  }
}

static void bt_on_data_received(void) {
 // _activity_counter = BT_ACTIVITY_TIMER; 
  /*
  static int i = 0;
  Serial.print('d');
  if(++i == 64) {
    Serial.println();
    i = 0;
  }*/
}

void PlayerClass::NewSsid(void) {
  if(_audio != NULL && Settings.NV.SourceAF == SET_SOURCE_WEB_RADIO) {
    Stop();
    if(WiFi.status() == WL_CONNECTED)
      WiFi.disconnect();
    Start();
  }
}

void PlayerClass::Stop(void) { // Stop() must be followed by a reboot (or in the future perhaps Start())
  Serial.printf("Stopping player \"%s\"\n", Settings.GetSourceName(Settings.NV.SourceAF));
  if(_audio != NULL) { _audio->stopSong(); AudioDieOut(_audio); delete _audio; _audio = NULL; }
  if(_a2dp_sink != NULL) { _a2dp_sink->pause(); }  // Still need to pause BT, because it's a separate thread
  if(_waveform_player != NULL) _waveform_player->Pause();;
}

void PlayerClass::Start(bool autoplay) {
  Serial.printf("Initialize player \"%s\"\n", Settings.GetSourceName(Settings.NV.SourceAF));
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD    : if(Settings.NoCard || !TrackSettings.IsValid()) { 
                                   Display.ShowHelpLine("Loading Track List...", true);
                                   TrackSettings.LoadFromCard(true); // If previous session had no card, or after a soft restart.
                                 }
                                 else {
                                   #ifdef USE_SD_MMC
                                   if(!SD_MMC.begin( "/sdcard", true)) {
                                   #else
                                   if (!SD.begin(PIN_SPI_SS, SPI, SPI_SD_SPEED)) {
                                   #endif
                                     Serial.println("Card Mount Failed, Cannot play track");
                                     Display.ShowHelpLine(DISPLAY_HELP_NO_CARD, true);
                                     Settings.NoCard = 1;
                                     break;
                                   }
                                   Serial.println("Card Mount Succeeded");
                                   Settings.NoCard    = 0;
                                 }
                                 if(Settings.NoCard) { //
                                   Display.ShowHelpLine(DISPLAY_HELP_NO_CARD, true);
                                   Settings.Play = 0;
                                 }
                                 else if(Settings.NV.DiskTotalTracks == 0) {
                                   Display.ShowHelpLine(DISPLAY_HELP_NO_TRACKS_FOUND, true);
                                   Settings.Play = 0;
                                 }
                                 else {
                                   Display.ShowHelpLine(Settings.Play ? DISPLAY_HELP_NONE : DISPLAY_HELP_PRESS_PLAY_TO_START, true);
                                   Settings.FirstSong = 1;
                                   Settings.CurrentTrackTime = Settings.NV.DiskTrackResumeTime;
                                   Settings.TotalTrackTime   = Settings.NV.DiskTrackTotalTime;
                                 }
                                 _audio = new Audio;
                                 _audio->setPinout(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT);
                                 _audio->setHeaderTimeout(10000);  // Using OMT expansion
                                 #if DAC_ID == DAC_ID_PT8211
                                   _audio->setI2SCommFMT_LSB(true);
                                   _audio->setVolume(SET_VOLUME_MAX);
                                 #else
                                   _audio->setVolume(SET_VOLUME_DEFAULT);
                                 #endif
                                 break;
    case SET_SOURCE_BLUETOOTH  : Display.ShowHelpLine("Setting up Bluetooth");
#if ESP_IDF_VERSION_MAJOR == 5  // OMT: under development
#error
static I2SClass i2s;
_a2dp_sink = new BluetoothA2DPSink(i2s);
i2s.setPins(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT);
if(!i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
  Serial.println("Failed to initialize I2S!");
  while (1); // do nothing
}
_a2dp_sink->start("MyMusic");

#else
                                 _a2dp_sink = new BluetoothA2DPSink;
                                 if(_a2dp_sink == NULL)
                                   Serial.println("Error creating BluetoothA2DPSink");
                                 else {
                                   Serial.println("Initializing created");
                                   i2s_pin_config_t my_pin_config = {
                                     .mck_io_num   = I2S_PIN_NO_CHANGE,
                                     .bck_io_num   = PIN_I2S_BCK,
                                     .ws_io_num    = PIN_I2S_WS,
                                     .data_out_num = PIN_I2S_DOUT,
                                     .data_in_num  = I2S_PIN_NO_CHANGE
                                   };
                                   _a2dp_sink->set_pin_config(my_pin_config);
                                   #if DAC_ID == DAC_ID_PT8211
                                     i2s_config_t my_i2s_config = {
                                       .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX),
                                       .sample_rate = 44100, // updated automatically by A2DP
                                       .bits_per_sample = (i2s_bits_per_sample_t)16,
                                       .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
                                       .communication_format = (i2s_comm_format_t) (I2S_COMM_FORMAT_STAND_MSB),
                                       .intr_alloc_flags = 0, // default interrupt priority
                                       .dma_buf_count = 8,
                                       .dma_buf_len = 64,
                                       .use_apll = true,
                                       .tx_desc_auto_clear = true // avoiding noise in case of data unavailability
                                     };
                                     _a2dp_sink->set_i2s_config(my_i2s_config);
                                   #endif                                   
                                   _a2dp_sink->set_avrc_metadata_callback(avrc_metadata_callback);
                                   //_a2dp_sink->set_sample_rate_callback(sample_rate_callback);
                                   _a2dp_sink->set_on_volumechange(bt_volumechange);
                                   //_a2dp_sink->set_rssi_callback(rssi_callback);
                                   _a2dp_sink->set_raw_stream_reader(bt_raw_stream_reader);
                                   //_a2dp_sink->set_on_data_received(bt_on_data_received);
                                   _a2dp_sink->start((const char *)Settings.NV.BtName, true);
                                   _a2dp_sink->reconnect(); // OMT new
                                   //_a2dp_sink->set_volume(BLUETOOTH_VOLUME_MAX);
                                   Display.ShowHelpLine((const char *)Settings.NV.BtName);
                                 }
#endif
                                 _activity_counter = 0;
                                 break;
    case SET_SOURCE_WEB_RADIO  :{uint64_t timestamp_us = esp_timer_get_time();
                                 uint8_t  timeout = 25;
                                 Display.ShowHelpLine(DISPLAY_HELP_INITIALIZING_WEBRADIO, true);
                                 Serial.printf("Connecting to %s ", SsidSettings.GetSsid(Settings.NV.CurrentSsid), false);
                                 WiFi.begin(SsidSettings.GetSsid(Settings.NV.CurrentSsid), SsidSettings.GetPassword(Settings.NV.CurrentSsid));
                                 WiFi.setTxPower((wifi_power_t)Settings.GetWifiTxLevel());
                                 while (WiFi.status() != WL_CONNECTED  && timeout > 0) {
                                   if( esp_timer_get_time() >= timestamp_us + 500000) {
                                      timestamp_us = esp_timer_get_time();
                                      timeout--;
                                      Serial.print(".");
                                   }
                                   // OMT: to do: Keep display running
                                 }
                                 if(WiFi.status() != WL_CONNECTED) {
                                   Serial.println(" CONNECTION FAILED");
                                   //Display.ShowHelpLine(DISPLAY_HELP_WIFI_CONNECT_FAILED, true);
                                   Display.ShowWebRadioConnect(false);
                                   //Settings.WifiConnected = 0;
                                 }
                                 else {
                                   Serial.println(" CONNECTED");
                                   if(Settings.Play)
                                     Display.ShowHelpLine(DISPLAY_HELP_WIFI_CONNECTED, true);
                                   else {
                                     if(Settings.NV.WebRadioTotalStations > 0)  
                                       Display.ShowHelpLine(DISPLAY_HELP_PRESS_PLAY_TO_START, true);
                                     else
                                       Display.ShowHelpLine(DISPLAY_HELP_URLS_FOUND, true);
                                     //Display.ShowHelpLine("PRESS_PLAY_TO_START", true);
                                   //Settings.WifiConnected = 1;
                                   }
                                 }
                                 _audio = new Audio;
                                 _audio->setPinout(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT);
                                 #if DAC_ID == DAC_ID_PT8211
                                   _audio->setI2SCommFMT_LSB(true);
                                   _audio->setVolume(SET_VOLUME_MAX);
                                 #else
                                   _audio->setVolume(SET_VOLUME_DEFAULT);
                                 #endif
                                 //Settings.InitDAC = 1;
                                }break;
    
    case SET_SOURCE_WAVE_GEN   : _waveform_player = new SimpleWaveGeneratorClass;
                                 _waveform_player->Init();
                                 _waveform_player->Volume(Settings.GetLogVolume(WAVEFORM_VOLUME_MAX));
                                 break;
    default: break;
  }
  Display.ShowPlayer();
  SetVolume(Settings.NV.Volume);
  if(autoplay)
    Play();
//PrintChipInfo(0xC);
}

#ifdef SET_VOLUME_DEFAULT
void PlayerClass::SetVolume(int vol) {
  if(vol > Settings.NV.VolumeSteps) vol = Settings.NV.VolumeSteps;
  if(_audio != NULL)           _audio->setVolume(vol);
  if(_waveform_player != NULL) _waveform_player->Volume(Settings.GetLogVolume(WAVEFORM_VOLUME_MAX));
  if(_a2dp_sink != NULL)       _a2dp_sink->set_volume(Settings.GetLinVolume(BLUETOOTH_VOLUME_MAX));
  //if(Settings.NV.Volume != vol) {
    Display.ShowVolume();
    Settings.NV.Volume = vol;
  //}
}
#endif

void PlayerClass::Play(void) {
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD    : if(Settings.NoCard || Settings.NV.DiskTotalTracks == 0)
                                   break;
                                 else {
                                   if(Settings.FirstSong) {
                                     Serial.printf("OMT: DiskTrackResumePos %d, DiskTrackResumeTime %d\n", Settings.NV.DiskTrackResumePos, Settings.NV.DiskTrackResumeTime); 
                                     PlayTrackFromSD(Settings.NV.DiskCurrentTrack, Settings.NV.DiskTrackResumePos, Settings.NV.DiskTrackResumeTime, Settings.NV.DiskTrackTotalTime);
                                     Serial.printf("OMT: Current time %d\n", Settings.CurrentTrackTime); 
                                     Settings.FirstSong = 0;
                                   }
                                   else
                                     PlayTrackFromSD(Settings.NV.DiskCurrentTrack);
                                 }
                                 Settings.Play = 1;
                                 Display.ShowPlayPause(Settings.Play);
                                 Display.ShowPlayTime(true);
                                 Display.ShowTrackNumber();
                                 break;
    case SET_SOURCE_BLUETOOTH  : Settings.CurrentTrackTime = 0;
                                 Display.ShowBluetoothName();
                                 Settings.Play = 0; // omt
                                 Display.ShowPlayPause(0);
                                 break;
    case SET_SOURCE_WEB_RADIO  : if(Settings.NV.WebRadioTotalStations == 0) break;
                                 if(WiFi.isConnected()) {
                                   web_station_t web_station;
                                   UrlSettings.GetStation(Settings.NV.WebRadioCurrentStation, web_station);
                                   Serial.printf("**********start a new radio: %s\n",web_station.url);
                                   _audio->connecttohost(web_station.url);
                                   Serial.println("**********start a new radio************");
                                   Settings.AudioEnded = 0;
                                   Settings.Play = 1;
                                 }
                                 else
                                   Settings.Play = 0;
                                 Settings.CurrentTrackTime = 0;
                                 Display.ShowWebRadioNumber();
                                 Display.ShowPlayPause(Settings.Play);
                                 Serial.println("**********new radio started************");
                                 Settings.WebTitleReceived = 0;
                                 break;
    case SET_SOURCE_WAVE_GEN   : _waveform_player->Volume(Settings.GetLogVolume(WAVEFORM_VOLUME_MAX)); 
                                 _waveform_player->Start(Settings.NV.WaveformId);
                                 Settings.Play = 1;
                                 Settings.CurrentTrackTime = 0;
                                 _waveform_player->Resume();
                                 Display.ShowPlayPause();
                                 Display.ShowPlayTime();
                                 Display.ShowWaveformName(_waveform_player->GetWaveformName(Settings.NV.WaveformId));
                                 //Display.ShowTrackTitle("_waveform_player->GetWaveformName(Settings.NV.WaveformId)");  // OMT: ticker test
                                 Display.ShowWaveformId();
                                 break;
    default:                     break;
  }
}

IRAM_ATTR void PlayerClass::loop(bool ten_ms_tick, bool seconds_tick) {
  if(_audio != NULL) _audio->loop();
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD   :{  uint32_t t = _audio->getAudioCurrentTime(); // OMT + Settings.CurrentTrackTimeOffset;
                                  if(Settings.CurrentTrackTime != t) {
                                    if(t)
                                      Settings.CurrentTrackTime = t;
                                    t = _audio->getAudioFileDuration(); 
                                    if(t && Settings.TotalTrackTime != t) {
                                      Settings.TotalTrackTime = t; // Sometimes the total time updates after start
                                    }
                                    Display.ShowPlayTime(); //GuiUpdatePlayingTime();
                                    if(Settings.CurrentTrackTime > Settings.TotalTrackTime + 6) { // +6 for some tolerance
                                     Settings.AudioEnded = 1;
                                     _audio->stopSong(); // Somtimes the "audio_eof_mp3" is not generated, so force a stop
                                     audio_eof_mp3("time out on audio_eof_mp3");
                                   } 
                                 }
                               }
                                 if(Settings.AudioEnded)
                                   PlayNext();
                                 break;
    case SET_SOURCE_BLUETOOTH  : if(ten_ms_tick &&_activity_counter > 0) {
                                   _activity_counter--;
                                 }
                                 if(Settings.Play) {
                                   if(_activity_counter == 0 || _activity_sum == 0) {
                                     //Serial.printf("\n*** STOP %d,%d***\n", _activity_counter, _activity_sum);
                                     Settings.Play = 0; 
                                     Display.ShowPlayPause();
                                   }
                                 }
                                 else {
                                   if(_activity_counter > 0 && _activity_sum != 0) {
                                     //Serial.printf("\n*** PLAY %d,%d***\n", _activity_counter, _activity_sum);
                                     Settings.Play = 1; 
                                     Display.ShowPlayPause();
                                   }
                                 }
                                 if(seconds_tick) {
                                   CheckBluetoothVolume(_a2dp_sink->get_volume()); // a minimum volume is needed to run properly
                                   if(Settings.Play) {
                                     Settings.UpdateCurrentTrackTime();
                                     Display.ShowPlayTime();
                                   }
                                   if(!_a2dp_sink->is_connected() && _connected) {
                                      Display.ShowTrackTitle((const char *)Settings.NV.BtName);
                                   }
                                   _connected = _a2dp_sink->is_connected();
                                 }
                                 break;
    case SET_SOURCE_WEB_RADIO  : if(seconds_tick && _audio->isRunning()) {
                                   Settings.UpdateCurrentTrackTime();
                                   Display.ShowPlayTime();
                                   if(!Settings.WebTitleReceived && Settings.CurrentTrackTime == 2) { // Show the station name from list. 
                                     web_station_t web_station;
                                     UrlSettings.GetStation(Settings.NV.WebRadioCurrentStation, web_station);
                                     Display.ShowStationName(web_station.name);
                                   }
                                 }
                                 if(Settings.AudioEnded) 
                                   PlayNext();
                                 break;
    case SET_SOURCE_WAVE_GEN   : _waveform_player->loop();
                                 if(seconds_tick && Settings.Play) {
                                   Settings.UpdateCurrentTrackTime();
                                   Display.ShowPlayTime();
                                 }
                                 break;
    default:                     break;
  }
  Display.loop(ten_ms_tick); 
}

void PlayerClass::PlayNew(void) {
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD    : Settings.NV.DiskCurrentTrack = Settings.NewTrack; Play(); break;
    case SET_SOURCE_WEB_RADIO  : Settings.NV.WebRadioCurrentStation = Settings.NewTrack; Play(); break;
    default: break;
  }  
}

void PlayerClass::PlayNext(void) {
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD    : Settings.NextDiskTrack();    Play(); break;
    case SET_SOURCE_WEB_RADIO  : Settings.NextRadioStation(); Play(); break;
    case SET_SOURCE_BLUETOOTH  : _a2dp_sink->next();
                                 Settings.CurrentTrackTime = 0; 
                                 break; 
    case SET_SOURCE_WAVE_GEN   : Settings.NextWaveform();
                                 Play();
                                 break;
    default: break;
  }  
}

void PlayerClass::PlayPrevious(void) {
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD    : if(_audio->getAudioCurrentTime() >= 5) {
                                   //_audio->setTimeOffset(0);
                                   //Play();
                                   if(_audio->isRunning()) {
                                     _audio->pauseResume();
                                     Settings.CurrentTrackTime = 0;
                                     _audio->setAudioPlayPosition(0);
                                     Display.ShowPlayTime(true);
                                     _audio->pauseResume();
                                   }
                                   else {
                                     Settings.CurrentTrackTime = 0;
                                     _audio->setAudioPlayPosition(0);
                                     Display.ShowPlayTime(true);
                                   }
                                 }
                                 else {
                                   Settings.PrevDiskTrack();
                                   Play();
                                 }
                                 break;
    case SET_SOURCE_WEB_RADIO  : Settings.PrevRadioStation(); Play(); break;
    case SET_SOURCE_BLUETOOTH  : if(Settings.CurrentTrackTime >= 5) 
                                   _a2dp_sink->rewind();
                                 else
                                   _a2dp_sink->previous();
                                 Settings.CurrentTrackTime = 0;
                                 break;  
    case SET_SOURCE_WAVE_GEN   : Settings.PrevWaveform();
                                 Play();
                                 break;
    default: break;
  }
}

static void PrintSdDebug(Audio * _audio) {
  uint32_t t1,t2,t3;
  Serial.println("******************** Begin of SD pause debug"); 
  Serial.printf("getAudioDataStartPos: %d\n", _audio->getAudioDataStartPos()); 
  Serial.printf("getFileSize         : %d\n", _audio->getFileSize()); 
  Serial.printf("getFilePos          : %d\n", _audio->getFilePos()); 
  Serial.printf("getSampleRate       : %d\n", _audio->getSampleRate()); 
  Serial.printf("getBitsPerSample    : %d\n", _audio->getBitsPerSample()); 
  Serial.printf("getChannels         : %d\n", _audio->getChannels()); 
  Serial.printf("getBitRate          : %d\n", _audio->getBitRate()); 
  Serial.printf("getAudioFileDuration: %d\n", _audio->getAudioFileDuration()); 
  Serial.printf("getAudioCurrentTime : %d\n", _audio->getAudioCurrentTime()); 
  //Serial.printf("getTotalPlayingTime : %d\n", _audio->getTotalPlayingTime()); 
  //Serial.printf("getVUlevel          : %d\n", _audio->getVUlevel()); 
  Serial.printf("inBufferFilled      : %d\n", _audio->inBufferFilled()); 
  Serial.printf("inBufferFree        : %d\n", _audio->inBufferFree()); 
  //    Serial.printf("inBufferSize        : %d\n", _audio->inBufferSize()); 

  t1 = _audio->getFileSize() - _audio->getAudioDataStartPos();
  t2 = t1 * _audio->getAudioCurrentTime() / _audio->getAudioFileDuration();
  Serial.printf("OMT: Audio size     : %d\n", t1);
  Serial.printf("OMT: New file pos   : %d\n", t2);
   
  Serial.println("******************** End of SD pause debug"); 
}

void PlayerClass::PauseResume(void) {
  switch(Settings.NV.SourceAF) {
    case SET_SOURCE_SD_CARD    : if(Settings.NV.DiskTotalTracks == 0) break;
                                 Settings.Play = !_audio->isRunning();
                                 Serial.printf("PLAY/RESUME %d\n", (int)Settings.Play); 
                                 _audio->pauseResume();
                                 Display.ShowPlayPause(Settings.Play);
                                 if(!Settings.Play) {
                                   if(!Settings.NoCard && Settings.NV.DiskTotalTracks > 0) { 
                                     Serial.println("Save card track position");
                                     Settings.NV.DiskTrackResumeTime = _audio->getAudioCurrentTime();
                                     Settings.NV.DiskTrackResumePos  = _audio->getFilePos() - _audio->inBufferFilled();
                                     Settings.NV.DiskTrackTotalTime  = _audio->getAudioFileDuration();
                                     //Settings.NV.DiskTrackResumePos  = _audio->getAudioDataStartPos() + (_audio->getFileSize() - _audio->getAudioDataStartPos()) * Settings.NV.DiskTrackResumeTime / Settings.NV.DiskTrackTotalTime;
                                     Serial.printf("Saving pos: %d s, len: %d s\n", Settings.NV.DiskTrackResumeTime, Settings.NV.DiskTrackTotalTime); 
                                     PrintSdDebug(_audio);
                                     Settings.EepromStore();
                                   }
                                 }
                                 break;
    case SET_SOURCE_BLUETOOTH  : if(!Settings.Play) {
                                   _a2dp_sink->play();
                                   Display.ShowPlayPause(1);                                   
                                 }
                                 else {
                                   _a2dp_sink->pause();
                                   Display.ShowPlayPause(0);
                                 }
                                 break;
    case SET_SOURCE_WEB_RADIO  : if(Settings.NV.WebRadioTotalStations == 0) break;
                                 Settings.Play = !_audio->isRunning();
                                 _audio->pauseResume();
                                 Display.ShowPlayPause(Settings.Play);
                                 break;
    case SET_SOURCE_WAVE_GEN   : if(Settings.Play) {
                                   _waveform_player->Pause();
                                   Serial.println("--- WG pause ---");
                                 }
                                 else {
                                   _waveform_player->Resume();
                                   Serial.println("--- WG resume ---");
                                 }
                                 Settings.Play = !Settings.Play;
                                 Display.ShowPlayPause();
                                 break;
    default: break;
  }
}

uint8_t PlayerClass::CheckBluetoothVolume(int v) {
  if(_a2dp_sink != NULL)  { 
    #if 0
      _a2dp_sink->set_volume(BLUETOOTH_VOLUME_MAX);
    #else
      if(v < BLUETOOTH_VOLUME_MIN) { // a minimum volume is needed to run properly
        v = BLUETOOTH_VOLUME_MIN;
        _a2dp_sink->set_volume(v);
        Serial.println("Bluetooth zero or low volume detected");
      }
      return v;
    #endif
  }
  return BLUETOOTH_VOLUME_MAX;
}

//******************************************************//
// optional functions                                   //
//******************************************************//

// optional (weak function override)

void audio_showstation(const char *info)     { Display.ShowStation(info);     }
void audio_showstreamtitle(const char *info) { Settings.WebTitleReceived = 1; Display.ShowStreamTitle(info); }
void audio_lasthost(const char *info)        { Display.ShowLastHost(info);    }
void audio_eof_mp3(const char *info)         { Display.ShowTrackEnded(info);  Settings.AudioEnded = 1; }
void audio_eof_stream(const char* info)      { Display.ShowStreamEnded(info); Settings.AudioEnded = 1; }

//#define SHOW_MORE_INFO

#ifdef SHOW_MORE_INFO
void audio_info(const char *info){
  Serial.print("info        "); Serial.println(info);
}

void audio_id3data(const char *info){  //id3 metadata
  Serial.print("id3data     ");Serial.println(info);
}

void audio_id3image(File& file, const size_t pos, const size_t size) { //ID3 metadata image
  Serial.println("icydescription");
}

void audio_icydescription(const char* info) {
  Serial.print("audio_icydescription");Serial.println(info);
}

#endif

void audio_commercial(const char *info){  //duration in sec
  Serial.print("commercial  ");Serial.println(info);
}

void audio_showstreaminfo(const char *info) {
  Serial.print("streaminfo  "); Serial.println(info);
}

void audio_bitrate(const char *info) {
  Serial.print("bitrate     "); Serial.println(info);
}

void audio_icyurl(const char *info){  //homepage
  Serial.print("icyurl      ");Serial.println(info);
}

void audio_eof_speech(const char *info){
  Serial.print("eof_speech  ");Serial.println(info);
}
