#ifndef _PLAYER_H_
#define _PLAYER_H_

#include "src/AudioI2S/Audio.h"
#include "src/BluetoothA2DP/BluetoothA2DPSink.h"
#include "src/Hardware/board.h"
#include "src/settings.h"
#include "SimpleWaveGenerator.h"

class PlayerClass {
  friend void bt_raw_stream_reader(const uint8_t*, uint32_t);
  public:
    PlayerClass();
    void Start(bool autoplay = false);
    void Stop(void);
    void Play(void);
    void PlayNew(void);
    void PlayNext(void);
    void PlayPrevious(void);
    void loop(bool ten_ms_tick = false, bool seconds_tick = false);
    void PauseResume(void);
    void AddLoopFunction(void (*f)(void)) { _loop = f; }
    void SetVolume(int vol);
    uint8_t CheckBluetoothVolume(int v); 
    void SD_PrintDebug(void);
    void WP_PrintDebug(bool hex = false) { if(_waveform_player != NULL) _waveform_player->Print(hex); }
    Audio * GetAudioPlayer() { return _audio; }
    void PositionEntry(void);
    void NewSsid(void);
    
  protected:
    bool PlayTrackFromSD(int n, uint32_t resume_pos = 0, uint32_t resume_time = 0, uint32_t track_time = 0, uint32_t total_time = 0);
 
    void (*_loop)(void);
    Audio * _audio;
    BluetoothA2DPSink * _a2dp_sink;
    SimpleWaveGeneratorClass * _waveform_player;
    uint16_t _activity_counter;
    uint32_t _activity_sum;
    bool     _connected;
};

extern PlayerClass Player;

#endif //_PLAYER_H_
