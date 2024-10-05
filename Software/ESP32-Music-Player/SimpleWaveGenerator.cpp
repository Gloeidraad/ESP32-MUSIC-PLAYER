//------------------------------------------------------------------------------------------------------------------------
// Includes

#include <Arduino.h> // Only for Serial.prinf functions
#include <math.h>
#include "SimpleWaveGenerator.h"
//#include "src/Hardware/board.h" // Comment out for stand alone version

#ifndef _BOARD_H_
 //#error _BOARD_H_ not defined (uncomment this line for stand alone version)
  #define DAC_ID_PT8211       0
  #define DAC_ID_MAX98357A    1
  #if 0  // Modulator
    #define PIN_I2S_BCK         27//26
    #define PIN_I2S_WS          33//25
    #define PIN_I2S_DOUT        12//27
    #define DAC_ID              DAC_ID_PT8211 // Choose your DAC type here
  #else  // QVGA Player
    #define PIN_I2S_BCK         33
    #define PIN_I2S_WS          25
    #define PIN_I2S_DOUT        32
    #define DAC_ID              DAC_ID_MAX98357A // Choose your DAC type here
  #endif
#endif

#ifdef USE_INTERNAL_DACS
  #define I2S_MODE         i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN)
  #define I2S_COMM_FORMAT  i2s_comm_format_t(I2S_COMM_FORMAT_STAND_MSB)
  #define DAC_ADJUST       (int16_t)INT16_MIN 
  #error
#else
  #define I2S_MODE         i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX)
  #if DAC_ID == DAC_ID_MAX98357A
    #define I2S_COMM_FORMAT  i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S)    // Standard Philips format 
    #define DAC_ADJUST       (int16_t)0   
  #else
    #define I2S_COMM_FORMAT  i2s_comm_format_t(I2S_COMM_FORMAT_STAND_MSB)    // PT8211 format
    #define DAC_ADJUST       (int16_t)0 
  #endif
#endif

// MAX98357A: LRCLK ONLY supports 8kHz, 16kHz, 32kHz, 44.1kHz, 48kHz, 88.2kHz and 96kHz frequencies. 
#if DAC_ID != DAC_ID_MAX98357A 
  #define SAMPLE_RATE_440HZ          96000
  #define SAMPLE_FREQ_440HZ          440
  #define NUMBER_OF_SAMPLES_440HZ    (SAMPLE_RATE_440HZ / SAMPLE_FREQ_440HZ) 

  #define SAMPLE_RATE_1KHZ           96000
  #define SAMPLE_FREQ_1KHZ           1000
  #define NUMBER_OF_SAMPLES_1KHZ     (SAMPLE_RATE_1KHZ / SAMPLE_FREQ_1KHZ) 

  #define SAMPLE_RATE_DAC_TEST       96000
#else // PT8211 can use any frequency
  #define SAMPLE_RATE_440HZ          44000
  #define SAMPLE_FREQ_440HZ          440
  #define NUMBER_OF_SAMPLES_440HZ    (SAMPLE_RATE_440HZ / SAMPLE_FREQ_440HZ) 

  #define SAMPLE_RATE_1KHZ           64000
  #define SAMPLE_FREQ_1KHZ           1000
  #define NUMBER_OF_SAMPLES_1KHZ     (SAMPLE_RATE_1KHZ / SAMPLE_FREQ_1KHZ) 

  #define SAMPLE_RATE_DAC_TEST       0x10000  // for 1 Hz signal
#endif

#if NUMBER_OF_SAMPLES_440HZ > NUMBER_OF_SAMPLES_1KHZ
  #define NUMBER_OF_SAMPLES_MAX     NUMBER_OF_SAMPLES_440HZ
#else
  #define NUMBER_OF_SAMPLES_MAX     NUMBER_OF_SAMPLES_1KHZ
#endif

#define NUMBER_OF_SAMPLES_DAC_TEST NUMBER_OF_SAMPLES_MAX

//====================================== local functions ===============================================

void SimpleWaveGeneratorClass::CreateSineWave(samples_t * dest, int samples) {
  float scale = (float)INT16_MAX * _volume / WAVEFORM_VOLUME_MAX;
  float step = M_TWOPI / samples;
  for(int alpha = 0; alpha < samples; alpha++) {
    uint16_t v = int(sin(step*alpha) * scale) + DAC_ADJUST;
    dest[alpha].left  = v;
    dest[alpha].right = v;
  }
  _current_number_of_samples = samples;
} 

void SimpleWaveGeneratorClass::CreateTriangleWave(samples_t * dest, int samples) {
  float scale = (float)_volume / WAVEFORM_VOLUME_MAX;
  float step  = scale * (float)UINT16_MAX / (samples / 2);
  float val   = scale * (float)INT16_MIN;

  for(int i = 0; i < samples; i++) {
    dest[i].left = dest[i].right = (int16_t)val + DAC_ADJUST;
    val += i < samples / 2 ? step : -step;
  }
  _current_number_of_samples = samples;
}

void SimpleWaveGeneratorClass::CreateDacTestWave(samples_t * dest, int samples) {
  static uint16_t val = INT16_MIN;
  float scale = (float)_volume / WAVEFORM_VOLUME_MAX;
 
  for(int i = 0; i < samples; i++) {
    dest[i].left = dest[i].right = uint16_t(scale * val) + DAC_ADJUST;
    val += 64;
  }
  _current_number_of_samples = samples;
}

void SimpleWaveGeneratorClass::CreateDacTestMin(samples_t * dest, int samples) {
  for(int i = 0; i < samples; i++) {
    dest[i].left = dest[i].right = (uint16_t)(INT16_MIN + DAC_ADJUST);
  }
  _current_number_of_samples = samples;
}

void SimpleWaveGeneratorClass::CreateDacTestMax(samples_t * dest, int samples) {
  for(int i = 0; i < samples; i++) {
    dest[i].left = dest[i].right = (uint16_t)(INT16_MAX + DAC_ADJUST);
  }
  _current_number_of_samples = samples;
}

//====================================== public functions ===============================================

SimpleWaveGeneratorClass::SimpleWaveGeneratorClass() {
  _i2s_port                  = i2s_port_t(I2S_NUM_0);
  _volume                    = WAVEFORM_VOLUME_MAX;
  _current_wave_form         = WAVEFORM_NONE;
  _current_sample_rate       = 0;
  _current_number_of_samples = 0;
   _pause                    = true;
  _samples                   = NULL;
  _i2s_installed = false;
  #if ESP_IDF_VERSION_MAJOR == 5
    memset(&_i2s_chan_cfg, 0, sizeof(i2s_chan_config_t));
    _i2s_chan_cfg.id                 = _i2s_port;
    _i2s_chan_cfg.role               = I2S_ROLE_MASTER;        // I2S controller master role, bclk and lrc signal will be set to output
    _i2s_chan_cfg.dma_desc_num       = 16;                     // number of DMA buffer
    _i2s_chan_cfg.dma_frame_num      = 512;                    // I2S frame number in one DMA buffer.
    _i2s_chan_cfg.auto_clear         = true;                   // i2s will always send zero automatically if no data to send

    memset(&_i2s_std_cfg, 0, sizeof(i2s_std_config_t));
    #if DAC_ID == DAC_ID_MAX98357A  // Set to enable bit shift in Philips mode
      _i2s_std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO); 
    #else                           // Set to enable bit shift in PT8211 mode
      _i2s_std_cfg.slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO); 
    #endif
    _i2s_std_cfg.gpio_cfg.bclk       = (gpio_num_t)PIN_I2S_BCK;
    _i2s_std_cfg.gpio_cfg.din        = I2S_GPIO_UNUSED;
    _i2s_std_cfg.gpio_cfg.dout       = (gpio_num_t)PIN_I2S_DOUT;
    _i2s_std_cfg.gpio_cfg.mclk       = I2S_GPIO_UNUSED;
    _i2s_std_cfg.gpio_cfg.ws         = (gpio_num_t)PIN_I2S_WS;
    _i2s_std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    _i2s_std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    _i2s_std_cfg.gpio_cfg.invert_flags.ws_inv   = false;
    _i2s_std_cfg.clk_cfg.sample_rate_hz = 44100;
    _i2s_std_cfg.clk_cfg.clk_src        = I2S_CLK_SRC_DEFAULT;     // Select PLL_F160M as the default source clock
    _i2s_std_cfg.clk_cfg.mclk_multiple  = I2S_MCLK_MULTIPLE_256;   // mclk = sample_rate * 256
  #else
    memset(&_i2s_config, 0, sizeof(i2s_config_t));
    _i2s_config.mode                 = I2S_MODE;
    _i2s_config.sample_rate          = 44100;
    _i2s_config.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;  // the internal DAC module will only take the 8bits from MSB
    _i2s_config.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
    _i2s_config.communication_format = I2S_COMM_FORMAT; 
    _i2s_config.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;       // high interrupt priority
    _i2s_config.dma_buf_count        = 8;                          // 8 buffers
    _i2s_config.dma_buf_len          = 1024;                       // 1K per buffer, so 8K of buffer space
    _i2s_config.use_apll             = 0;
    _i2s_config.tx_desc_auto_clear   = true; 
    _i2s_config.fixed_mclk           = -1;
    _i2s_config.mclk_multiple        = (i2s_mclk_multiple_t)0;
    _i2s_config.bits_per_chan        = (i2s_bits_per_chan_t)0;

    _i2s_pin_config.mck_io_num       = I2S_PIN_NO_CHANGE; 
    _i2s_pin_config.bck_io_num       = PIN_I2S_BCK;       // The bit clock connectiom, goes to the ESP32
    _i2s_pin_config.ws_io_num        = PIN_I2S_WS;        // Word select, also known as word select or left right clock
    _i2s_pin_config.data_out_num     = PIN_I2S_DOUT;      // Data out from the ESP32, connect to DIN on 38357A
    _i2s_pin_config.data_in_num      = I2S_PIN_NO_CHANGE; // we are not interested in I2S data into the ESP32
  #endif
}

SimpleWaveGeneratorClass::~SimpleWaveGeneratorClass() {
  _pause = true;
  if(_i2s_installed) {
    #if ESP_IDF_VERSION_MAJOR == 5
      i2s_channel_disable(_i2s_tx_handle);
      delay(100);
      i2s_del_channel(_i2s_tx_handle);
    #else
      i2s_stop(_i2s_port);
      delay(100);
      i2s_driver_uninstall(_i2s_port);
    #endif
  }
  if(_samples != NULL) free(_samples);
  Serial.println("~SimpleWaveGeneratorClass");
}

void SimpleWaveGeneratorClass::Init(i2s_port_t port) {
  _i2s_port = port;
  if(_samples == NULL) {
    _samples = (samples_t *)calloc(NUMBER_OF_SAMPLES_MAX, sizeof(samples_t));
  }
  if(!_i2s_installed) {
    #if ESP_IDF_VERSION_MAJOR == 5
      _i2s_chan_cfg.id = _i2s_port;
      i2s_new_channel(&_i2s_chan_cfg, &_i2s_tx_handle, NULL);
      i2s_channel_init_std_mode(_i2s_tx_handle, &_i2s_std_cfg);
    #else
      i2s_driver_install(_i2s_port, &_i2s_config, 0, NULL); // ESP32 will allocated resources to run I2S
      i2s_set_pin(_i2s_port, &_i2s_pin_config);            // Tell it the pins you will be using 
    #endif
    _i2s_installed = true;
  }
  _pause = true;
}

void SimpleWaveGeneratorClass::Volume(int vol) { 
  if(vol < 0) vol = 0;
  if(vol > WAVEFORM_VOLUME_MAX) vol = WAVEFORM_VOLUME_MAX;
  _volume = vol;
  Serial.printf("SimpleWaveGeneratorClass::Volume %d/%d\n", _volume, WAVEFORM_VOLUME_MAX);
  CreateWaveform(_current_wave_form);
}

bool SimpleWaveGeneratorClass::CreateWaveform(int waveform) {
  _current_wave_form = waveform;
  switch(waveform) {
    case WAVEFORM_SINE_440HZ: CreateSineWave(_samples, _current_number_of_samples = NUMBER_OF_SAMPLES_440HZ); 
                              _current_sample_rate = SAMPLE_RATE_440HZ;
                               break;
    case WAVEFORM_SINE_1KHZ : CreateSineWave(_samples, _current_number_of_samples = NUMBER_OF_SAMPLES_1KHZ); 
                              _current_sample_rate = SAMPLE_RATE_1KHZ;
                              break;
    case WAVEFORM_TRI_440HZ : CreateTriangleWave(_samples, _current_number_of_samples = NUMBER_OF_SAMPLES_440HZ);
                              _current_sample_rate = SAMPLE_RATE_440HZ;
                              break;
    case WAVEFORM_TRI_1KHZ  : CreateTriangleWave(_samples, _current_number_of_samples = NUMBER_OF_SAMPLES_1KHZ);
                              _current_sample_rate = SAMPLE_RATE_1KHZ;
                              break;
    case WAVEFORM_DAC_TEST  : CreateDacTestWave(_samples, _current_number_of_samples = NUMBER_OF_SAMPLES_MAX);
                              _current_sample_rate = SAMPLE_RATE_DAC_TEST;
                              break;
    case WAVEFORM_DAC_MIN   : CreateDacTestMin(_samples, _current_number_of_samples = NUMBER_OF_SAMPLES_MAX);
                              _current_sample_rate = SAMPLE_RATE_DAC_TEST;
                              break;
    case WAVEFORM_DAC_MAX   : CreateDacTestMax(_samples, _current_number_of_samples = NUMBER_OF_SAMPLES_MAX);
                              _current_sample_rate = SAMPLE_RATE_DAC_TEST;
                              break;
    default                 : _current_wave_form = WAVEFORM_NONE;
                              return false; 
  }
  return true;
}

void SimpleWaveGeneratorClass::Start(int waveform, bool autoplay) {
  bool cur_pause = _pause;
  _pause = true;
  if(!CreateWaveform(waveform)) return; // Not a valid waveform, so exit without install.
  #if ESP_IDF_VERSION_MAJOR == 5
    i2s_channel_disable(_i2s_tx_handle);
    _i2s_std_cfg.clk_cfg.sample_rate_hz = _current_sample_rate;
    i2s_channel_reconfig_std_clock(_i2s_tx_handle, &_i2s_std_cfg.clk_cfg);
    i2s_channel_enable(_i2s_tx_handle);
  #else
    i2s_set_sample_rates(_i2s_port, _current_sample_rate);
  #endif
  if(autoplay)
    _pause = false;
  else
    _pause = cur_pause;
}

void SimpleWaveGeneratorClass::loop(void) {
  if(_current_wave_form >= 0 && !_pause) { 
    size_t BytesWritten; 
    #if ESP_IDF_VERSION_MAJOR == 5
      i2s_channel_write(_i2s_tx_handle, _samples, _current_number_of_samples * sizeof(samples_t), &BytesWritten, portMAX_DELAY);
    #else
      i2s_write(_i2s_port, _samples, _current_number_of_samples * sizeof(samples_t), &BytesWritten, portMAX_DELAY );
    #endif
    if(_current_wave_form == WAVEFORM_DAC_TEST)
      CreateDacTestWave(_samples, _current_number_of_samples); // update the buffer with new values
  }
}

#ifndef NO_WAVE_INFO

const char *SimpleWaveGeneratorClass::GetWaveformName(int id) {
  const char * WaveNames[8] = { "Sine 440Hz", "Sine 1000Hz", "Triangle 440Hz", "Triangle 1000Hz",
                                "DAC Test",   "DAC Minimum", "DAC Maximum",    "Invalid Waveform" }; 
  if(id < WAVEFORM_SINE_440HZ || id > WAVEFORM_COUNT)
    return WaveNames[WAVEFORM_COUNT];
  return WaveNames[id];
}

#endif

void SimpleWaveGeneratorClass::Print(bool hex) {
  Serial.printf("Waveform=%s, Buffer Valid=%d, Volume=%d, Rate=%d, Samples=%d, Pause=%d\n",GetWaveformName(), _samples != NULL, _volume, _current_sample_rate, _current_number_of_samples, _pause);
  for(int i = 0; i < _current_number_of_samples; i++) {
    if(hex)
        Serial.printf("%04X ", ((uint32_t)_samples[i].left) & 0xFFFF);
    else
      #if DAC_ID == DAC_ID_MAX98357A
        Serial.printf("%6d ", (int)_samples[i].left);
      #else
        Serial.printf("%5d ", ((uint32_t)_samples[i].left) & 0xFFFF);
      #endif
    if((i & 0x1F) == 0x1F)
      Serial.println();
    loop(); // Keep sound runnig while printing...
  }
}
