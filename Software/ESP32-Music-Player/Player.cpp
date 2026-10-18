// Remark:  Please check in "Hardware/Board.h" that:
// USE_SD_MMC is defined
// DAC_ID is set to DAC_ID_MAX98357A

#include "Player.h"

#include "src/Settings.h"

#include "src/AudioI2S/Audio.h"
#include "src/BluetoothA2DP/BluetoothA2DPSink.h"
#include "SimpleWaveGenerator.h"

#include <ESP_I2S.h>

#define PL_SD SET_SOURCE_SD_CARD
#define PL_WR SET_SOURCE_WEB_RADIO
#define PL_WG SET_SOURCE_WAVE_GEN
#define PL_BT SET_SOURCE_BLUETOOTH

#define DEBUG_BT_EXTRA_INFO     0 // 0 = no extra info, 1 = some extra info, 2 = more extra info, etc
#define DEBUG_AUDIO_EXTRA_INFO  1 // 0 = no extra info, 1 = some extra info, 2 = more extra info, etc

Audio                    audio;
I2SClass                 i2s;

#define USE_BLUETOOTH_POINTER

#ifdef USE_BLUETOOTH_POINTER
  BluetoothA2DPSink * p_a2dp_sink = NULL;
  #define a2dp_sink (*p_a2dp_sink)
#else
  BluetoothA2DPSink        a2dp_sink(i2s);
#endif


#define USE_WAVEGEN_POINTER

#ifdef USE_WAVEGEN_POINTER
  SimpleWaveGeneratorClass * p_WaveGenerator;
  #define WaveGenerator (*p_WaveGenerator)
#else
  SimpleWaveGeneratorClass WaveGenerator;
#endif

//******************************************************//
// Callbacks                                            //
//******************************************************//

static String callback_msg;

//******************************************************//
// audio callback (for SD-card and Webradio players)    //
//******************************************************//

//#define AUDIO_VERSION_3_2_1

#ifdef AUDIO_VERSION_3_2_1

// optional (weak function override)

void audio_eof_mp3(const char *info){  //end of file
  Serial.printf("end of file:  %s\n", info);
  Settings.V().AudioEnded = 1;
}
void audio_showstreamtitle(const char *info) {
  Serial.printf("stream title: %s\n", info);
  callback_msg = info;
  Settings.V().TrackTitleReceived = 1; 
}

void audio_eof_stream(const char* info) { 
  Serial.printf("end of stream: %s\n", info);
  Settings.V().AudioEnded = 1;
}

#if DEBUG_AUDIO_EXTRA_INFO > 0
  void audio_lasthost(const char *info)                                { Serial.print("last host   : "); Serial.println(info); }
  void audio_showstation(const char *info)                             { Serial.print("station name: "); Serial.println(info); }
  void audio_info(const char *info)                                    { Serial.print("info        "); Serial.println(info); }
  void audio_id3data(const char *info)                                 { Serial.print("id3data     "); Serial.println(info); }  //id3 metadata
  void audio_id3image(File& file, const size_t pos, const size_t size) { Serial.println("icydescription"); } //ID3 metadata image
  void audio_icydescription(const char* info)                          { Serial.print("audio_icydescription"); Serial.println(info); }
  void audio_commercial(const char *info)                              { Serial.print("commercial  "); Serial.println(info);  } //duration in sec
  void audio_showstreaminfo(const char *info)                          { Serial.print("streaminfo  "); Serial.println(info); }
  void audio_bitrate(const char *info)                                 { Serial.print("bitrate     "); Serial.println(info); }
  void audio_icyurl(const char *info)                                  { Serial.print("icyurl      "); Serial.println(info); } //homepage
  void audio_eof_speech(const char *info)                              { Serial.print("eof_speech  "); Serial.println(info);  }
#endif

#else

static void my_audio_callback(Audio::msg_t m) {
  const char * msg = m.msg == NULL ? "[NO MSG]" : m.msg;
  switch (m.e) {
    case Audio::evt_eof:
      Serial.printf("end of file:  %s\n", msg);
      Settings.V().AudioEnded = 1;
      break;
    case Audio::evt_lasthost:
      Serial.printf("last URL: ... %s\n", msg);
      break;
    case Audio::evt_name:
      Serial.printf("station name: %s\n", msg);
      break;  // station name or icy-name
    case Audio::evt_streamtitle:
      Serial.printf("stream title: %s\n", msg);
      callback_msg = msg;
      Settings.V().TrackTitleReceived = 1;
      break;
#if DEBUG_AUDIO_EXTRA_INFO > 0
    #if DEBUG_AUDIO_EXTRA_INFO > 1
      case Audio::evt_info:    Serial.printf("info: ....... %s\n", msg); break;
    #endif
    case Audio::evt_id3data: Serial.printf("ID3 data: ... %s\n", msg); break;  // id3-data or metadata
    case Audio::evt_bitrate: Serial.printf("bitrate: .... %s\n", msg); break;  // icy-bitrate or bitrate from metadata
    case Audio::evt_icyurl:  Serial.printf("icy URL: .... %s\n", msg); break;
    case Audio::evt_icylogo: Serial.printf("icy logo: ... %s\n", msg); break;
    case Audio::evt_icydescription: Serial.printf("icy descr: .. %s\n", msg); break;
    //case Audio::evt_image:
    //  for(int i = 0; i < m.vec.size(); i += 2) {
    //    Serial.printf("cover image:  segment %02i, pos %07lu, len %05lu\n", i / 2, m.vec[i], m.vec[i + 1]);
    //  }
    //  break;  // APIC
    case Audio::evt_lyrics:  Serial.printf("sync lyrics:  %s\n", msg); break;
    case Audio::evt_log:     Serial.printf("audio_logs:   %s\n", msg); break;
    default:                 Serial.printf("message:..... %s\n", msg); break;
#endif
  }
}

#endif

//******************************************************//
// Bluetooth callbacks                                  //
//******************************************************//

static void avrc_metadata_callback(uint8_t attribute, const uint8_t *data) {
  switch(attribute) {
    case 0x01: Serial.printf("AVRC metadata: title \"%s\"\n", data);
               Settings.V().CurrentTrackTime = -1; 
               callback_msg = strlen((const char *)data) > 0 ? (const char *)data : (const char *)Settings.NV().BtName;
               Settings.V().TrackTitleReceived = 1;
               break;
    #if DEBUG_BT_EXTRA_INFO > 0
    default  : Serial.printf("AVRC metadata: rsp: attribute id 0x%x, %s\n", attribute, data); break;
    #endif
  }
}

static void avrc_playstatus_callback(esp_avrc_playback_stat_t playback) {
  Serial.printf("AVRC playstatus: %d\n", playback);
  switch(playback) {
    case ESP_AVRC_PLAYBACK_STOPPED: Settings.V().NewPlayStatusReceived = SET_PLAYBACK_STOPPED; break;
    case ESP_AVRC_PLAYBACK_PLAYING: Settings.V().NewPlayStatusReceived = SET_PLAYBACK_PLAYING; break;
    case ESP_AVRC_PLAYBACK_PAUSED:  Settings.V().NewPlayStatusReceived = SET_PLAYBACK_PAUSED;  break; 
  }
}

#define BT_ACTIVITY_TIMER 10  // in 10 ms slots

#define BLUETOOTH_VOLUME_MIN 24
#define BLUETOOTH_VOLUME_MAX 127

extern PlayerClass Player;

uint8_t bt_check_bluetooth_volume(int v) {
  #if DAC_ID == DAC_ID_PT8211
    a2dp_sink.set_volume(BLUETOOTH_VOLUME_MAX); // Volume always at maximum when using PT8211
  #else
    if(v < BLUETOOTH_VOLUME_MIN) { // a minimum volume is needed to run properly
      v = BLUETOOTH_VOLUME_MIN;
      a2dp_sink.set_volume(v);
      Serial.println("Bluetooth zero or low volume detected");
    }
    return v;
  #endif
  return BLUETOOTH_VOLUME_MAX;
}

static void bt_volumechange(int v) { 
  Serial.printf("bt_volumechange: %d\n",v);
  v = bt_check_bluetooth_volume(v);
  for(int v2 = 1; v2 <= Settings.NV().VolumeSteps; v2++) {
    if(Settings.GetLinVolume(v2, BLUETOOTH_VOLUME_MAX) >= v) {
      Settings.NV().Volume = v2; 
      Settings.V().NewVolumeReceived = 1;
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
    #if DEBUG_BT_EXTRA_INFO > 1
      //Serial.printf("%d ", len);
      //Serial.printf("%-2d", Player._activity_sum != 0);
      Serial.printf("%8d ", Player._activity_sum);
      if(!(i & 0xFF))
        Serial.println();
    #endif
  }
}

//******************************************************//
// Player Class functions                               //
//******************************************************//

PlayerClass::PlayerClass() {
  _time_updated = false;
  _activity_counter = 0;
  _activity_sum     = 1;
  _connected        = false;
  #ifndef AUDIO_VERSION_3_2_1
    Audio::audio_info_callback = my_audio_callback;
  #endif
}

void PlayerClass::Init() {
  Serial.printf("Size: audio         %d\n", sizeof(audio));
  Serial.printf("Size: a2dp_sink     %d\n", sizeof(a2dp_sink));
  Serial.printf("Size: WaveGenerator %d\n", sizeof(WaveGenerator));
  switch(Settings.NV().SourceAF) {
    case PL_SD:               
    case PL_WR: audio.setPinout(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT);
                audio.setVolume(Settings.NV().Volume); 
                audio.forceMono(Settings.NV().OutputFormat);
                #if DAC_ID == DAC_ID_PT8211
                  audio.setI2SCommFMT_LSB(true);
                #endif
                break;
    case PL_WG:
                #ifdef USE_WAVEGEN_POINTER
                  p_WaveGenerator = new SimpleWaveGeneratorClass;
                #endif
                WaveGenerator.Init(i2s_port_t(I2S_NUM_1)); // Use I2S_NUM_1 because I2S_NUM_0 is claimed by Bluetooth
                WaveGenerator.Volume(Settings.GetLogVolume(WAVEFORM_VOLUME_MAX));
                break;
    case PL_BT: 
                #ifdef USE_BLUETOOTH_POINTER
                  p_a2dp_sink = new BluetoothA2DPSink(i2s);
                #endif
                i2s.setPins(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT);
                if(!i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
                  Serial.println("Failed to initialize I2S!");
                  while(1) delay(100); // do nothing
                }
                Serial.println("I2S initialized for Bluetooth player!");
                if(DAC_ID == DAC_ID_PT8211) {
                  i2s_std_config_t i2s_std_cfg;
                  i2s_chan_handle_t h = i2s.txChan();
                  i2s_channel_disable(h);
                  i2s_std_cfg.slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
                  i2s_channel_reconfig_std_slot(h, &i2s_std_cfg.slot_cfg);
                  i2s_channel_enable(h);
                }
                a2dp_sink.set_mono_downmix(Settings.NV().OutputFormat);                             
                a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
                a2dp_sink.set_avrc_rn_playstatus_callback(avrc_playstatus_callback);
                //a2dp_sink.set_sample_rate_callback(sample_rate_callback);
                a2dp_sink.set_on_volumechange(bt_volumechange);
                //a2dp_sink.set_rssi_callback(rssi_callback);
                a2dp_sink.set_raw_stream_reader(bt_raw_stream_reader);
                //_a2dp_sink.set_on_data_received(bt_on_data_received);
                a2dp_sink.start((const char *)Settings.NV().BtName, true);
                a2dp_sink.reconnect(); // OMT new
                 //_a2dp_sink.set_volume(BLUETOOTH_VOLUME_MAX);
                _activity_counter = 0;
                break;
  }
}

void PlayerClass::Stop() {
  switch(Settings.NV().SourceAF) {
    case PL_SD:        
    case PL_WR: audio.stopSong(); break;
    case PL_WG: WaveGenerator.Pause(); break;
    case PL_BT: a2dp_sink.pause(); break;  // Still need to pause BT, because it's a separate thread
  }
}

void PlayerClass::SetVolume(int vol) {
  switch(Settings.NV().SourceAF) {
    case PL_SD:        
    case PL_WR: audio.setVolume(vol); break;
    case PL_WG: WaveGenerator.Volume(Settings.GetLogVolume(vol, WAVEFORM_VOLUME_MAX));
                break;
    case PL_BT: if(Settings.GetLinVolume(vol, BLUETOOTH_VOLUME_MAX) < BLUETOOTH_VOLUME_MIN)
                  a2dp_sink.set_volume(BLUETOOTH_VOLUME_MIN);
                else
                  a2dp_sink.set_volume(Settings.GetLinVolume(vol, BLUETOOTH_VOLUME_MAX));
                break;
  }
}

bool PlayerClass::VolumeDown() {
  if(Settings.NV().Volume > 0) {
    SetVolume(--Settings.NV().Volume);
   return true;
  }
  return false;
}

bool PlayerClass::VolumeUp() {
  if(Settings.NV().Volume < Settings.NV().VolumeSteps) {
    SetVolume(++Settings.NV().Volume);
    return true;
  }
  return false;
}

bool PlayerClass::IsRunning() {
  switch(Settings.NV().SourceAF) {
    case PL_SD:        
    case PL_WR: return audio.isRunning(); break;
    case PL_WG: return WaveGenerator.IsRunning(); break;
    case PL_BT: return _activity_sum != 0 && Settings.V().FirstTime == 0; break;
  }
  return false;
}

void PlayerClass::PlayNext()     { // Only for Bluetooth
  if(Settings.NV().SourceAF == PL_BT) {
    a2dp_sink.next();
  }
}

void PlayerClass::PlayPrevious() { // Only for Bluetooth
  if(Settings.NV().SourceAF == PL_BT) {
    if(Settings.V().CurrentTrackTime >= 5) {
      Serial.println("a2dp_sink.rewind()");
      a2dp_sink.rewind();
      Settings.V().CurrentTrackTime = 0;
    }
    else {
      Serial.println("a2dp_sink.previous()");
      a2dp_sink.previous();
    }
  }
}

bool PlayerClass::PauseResume(/*bool time_save*/) {
  bool running = false;
  switch(Settings.NV().SourceAF) {
    case PL_SD: running = audio.isRunning();
                audio.pauseResume();
                if(running /*&& time_save*/) {
                  Settings.NV().DiskTrackResumeTime = audio.getAudioCurrentTime();
                  #ifdef AUDIO_VERSION_3_2_1
                    Settings.NV().DiskTrackResumePos = audio.getFilePos() - audio.inBufferFilled();
                  #else
                    Settings.NV().DiskTrackResumePos = audio.getAudioFilePosition();
                  #endif
                }
                running = !running;
                break;
    case PL_WR: running = audio.isRunning();
                audio.pauseResume();
                running = !running;
                break;
    case PL_WG: if(WaveGenerator.IsRunning()) {
                  WaveGenerator.Pause();
                }
                else {
                  WaveGenerator.Resume();
                }
                break;
    case PL_BT: if(_activity_sum && Settings.V().FirstTime == 0) 
                  a2dp_sink.pause();
                else
                  a2dp_sink.play();
                break;
  }
  return running;
}

void PlayerClass::UpdateOutputFormat() {
  switch(Settings.NV().SourceAF) {
    case PL_SD: 
    case PL_WR: audio.forceMono(Settings.NV().OutputFormat);
                break;
    case PL_BT: a2dp_sink.set_mono_downmix(Settings.NV().OutputFormat);
                break;
  }
}

void PlayerClass::PlayWaveform() {
  WaveGenerator.Start(Settings.NV().WaveformId, true);
}

const char * PlayerClass::WaveformName(int id) {
  return WaveGenerator.GetWaveformName(id);
}

void PlayerClass::RepositionTrack(bool to_file_pos) {
  bool was_running = audio.isRunning();
  if(!was_running) audio.pauseResume();; // Position works only when track is playing, so temporarily play track
  if(to_file_pos) {
    #ifdef AUDIO_VERSION_3_2_1
      audio.setFilePos(Settings.NV().DiskTrackResumePos);
    #else
      audio.setAudioFilePosition(Settings.NV().DiskTrackResumePos);
    #endif
    Settings.V().CurrentTrackTime = 0;
  }
  else {
    #ifdef AUDIO_VERSION_3_2_1
      audio.setAudioPlayPosition(Settings.NV().DiskTrackResumeTime);
    #else
      audio.setAudioPlayTime(Settings.NV().DiskTrackResumeTime);
    #endif
    Settings.V().CurrentTrackTime = Settings.NV().DiskTrackResumeTime;
  }
  if(!was_running) audio.pauseResume();; // Pause again if applicable
}

void PlayerClass::loop(bool ten_ms_tick, bool seconds_tick) {
  switch(Settings.NV().SourceAF) {
    case PL_SD: audio.loop();
                if(audio.isRunning() && audio.getAudioCurrentTime() != Settings.V().CurrentTrackTime) {
                  Settings.V().CurrentTrackTime = audio.getAudioCurrentTime();
                  Settings.V().TotalTrackTime   = audio.getAudioFileDuration();
                  _time_updated = true;
                }
                break;
    case PL_WR: audio.loop();
                if(seconds_tick && audio.isRunning()) {
                  Settings.UpdateCurrentTrackTime();
                  _time_updated = true;
                }
                break;
    case PL_WG: WaveGenerator.loop();
                if(seconds_tick && WaveGenerator.IsRunning()) {
                  Settings.UpdateCurrentTrackTime();
                  _time_updated = true;
                }
                break;
    case PL_BT: if(seconds_tick && IsRunning()) {
                  Settings.UpdateCurrentTrackTime();
                  _time_updated = true;
                }
                break;
  }
  //if(_loop) _loop();
}

void PlayerClass::PlayTrack(fs::FS& fs, const char* filename) {
  if(Settings.NV().SourceAF == PL_SD) {
    audio.connecttoFS(fs, filename);
    callback_msg = filename;
  }
}

void PlayerClass::PlayWebStation(const char* url) {
  if(Settings.NV().SourceAF == PL_WR) {
    audio.connecttohost(url);
    callback_msg = url;
  }
}

const char * PlayerClass::GetCallbackTrackTitle() { 
  return callback_msg.c_str();
}
