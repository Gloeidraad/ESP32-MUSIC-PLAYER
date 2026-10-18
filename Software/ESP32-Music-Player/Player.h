#ifndef _PLAYER_NEW_H_
#define _PLAYER_NEW_H_

#include <stdint.h>
#include <FS.h>

class PlayerClass {
  friend void bt_raw_stream_reader(const uint8_t*, uint32_t);
  public:
    PlayerClass();
    void Init();
    void Stop();
    void SetVolume(int vol);
    bool VolumeDown();
    bool VolumeUp();
    bool IsRunning();
    bool PauseResume();
    bool TimeUpdated() { bool update = _time_updated; _time_updated = false; return update; }
    void PlayTrack(fs::FS& fs, const char* filename);
    void PlayNext();     // Only for Bluetooth
    void PlayPrevious(); // Only for Bluetooth
    void RepositionTrack(bool to_file_pos);
    void PlayWaveform();
    void PlayWebStation(const char* url);
    void UpdateOutputFormat();
    const char * WaveformName(int id);
    const char * GetCallbackTrackTitle();
    void loop(bool ten_ms_tick = false, bool seconds_tick = false);
  protected:
    bool     _time_updated;
    uint16_t _activity_counter;
    uint32_t _activity_sum;
    bool     _connected;
};

#endif //_PLAYER_NEW_H_
