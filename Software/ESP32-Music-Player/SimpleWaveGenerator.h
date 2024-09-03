#ifndef _SIMPLEWAVEGENERATOR_H_
#define _SIMPLEWAVEGENERATOR_H_

// How to use:
// Set the correct pins and DAC type in SimpleWaveGenerator.cpp
// Make an instance of the class. For example in the main .ino file:
// SimpleWaveGeneratorClass WaveGenerator;
//
// Start the player with:
//   WaveGenerator.Init();
//   WaveGenerator.Start(WAVEFORM_SINE_440HZ, true); // Or another of the waveforms declared below
// or
//   WaveGenerator.Start(WAVEFORM_SINE_440HZ);      // Or another of the waveforms declared below
//   WaveGenerator.Resume();
//
// You can pause the player with:
//   WaveGenerator.Pause();

#if ESP_IDF_VERSION_MAJOR == 5
  #include <driver/i2s_std.h>  // Library of I2S routines, comes with ESP32 standard install
#else
  #include <driver/i2s.h>      // Library of legacy I2S routines, comes with ESP32 standard install
#endif

//#define USE_INTERNAL_DACS
//#define NO_WAVE_INFO

#define WAVEFORM_NONE       -1
#define WAVEFORM_SINE_440HZ  0
#define WAVEFORM_SINE_1KHZ   1
#define WAVEFORM_TRI_440HZ   2
#define WAVEFORM_TRI_1KHZ    3
#define WAVEFORM_DAC_TEST    4
#define WAVEFORM_DAC_MIN     5
#define WAVEFORM_DAC_MAX     6
#define WAVEFORM_COUNT       7

#define WAVEFORM_VOLUME_MAX  127

class SimpleWaveGeneratorClass {
  public:
    SimpleWaveGeneratorClass();
    ~SimpleWaveGeneratorClass();
    void Init(i2s_port_t port = i2s_port_t(I2S_NUM_0));
    void Start(int waveform, bool autoplay = false);
    void Pause(void)  { _pause = true;  }
    void Resume(void) { _pause = false; }
    void loop(void);
    void Volume(int vol); // 0 .. WAVEFORM_VOLUME_MAX
    #ifndef NO_WAVE_INFO
      const char *GetWaveformName(int id);
      const char *GetWaveformName() {return GetWaveformName(_current_wave_form); }
    #endif
    typedef struct { int16_t left, right; } samples_t;
    void Print(bool hex = false); // for debug purposes
    
  protected:
    void CreateSineWave(samples_t * dest, int samples);
    void CreateTriangleWave(samples_t * dest, int samples);
    void CreateDacTestWave(samples_t * dest, int samples);
    void CreateDacTestMin(samples_t * dest, int samples);
    void CreateDacTestMax(samples_t * dest, int samples);
    bool CreateWaveform(int waveform);
    
    i2s_port_t _i2s_port;
    int  _volume; 
    int  _current_wave_form;
    int  _current_sample_rate;
    int  _current_number_of_samples;
    bool _pause;
    bool _i2s_installed;
    samples_t * _samples;
    #if ESP_IDF_VERSION_MAJOR == 5
      i2s_chan_handle_t _i2s_tx_handle;
      i2s_chan_config_t _i2s_chan_cfg; // stores I2S channel values
      i2s_std_config_t  _i2s_std_cfg;  // stores I2S driver values
    #else
      i2s_config_t      _i2s_config ;
      i2s_pin_config_t  _i2s_pin_config;
  #endif
};

extern SimpleWaveGeneratorClass WaveGenerator;

#endif // _SIMPLEWAVEGENERATOR_H_
