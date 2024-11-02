#ifndef _BOARD_H_
#define _BOARD_H_

#define ESP32_WROVER_QVGA   0    // Usig final QVGA PCBA
#define ESP32_S3_PLAYER     1    // Usig final Player
#define TARGET_BOARD        ESP32_WROVER_QVGA

#define DAC_ID_PT8211       0
#define DAC_ID_MAX98357A    1
#define DAC_ID              DAC_ID_MAX98357A

#ifndef NO_SERIAL_DEBUG
#define DEBUG_MSG(s)                  Serial.printf(s)
#define DEBUG_MSG_VAL(s,v)            Serial.printf(s,v)
#define DEBUG_MSG_VAL2(s,v1,v2)       Serial.printf(s,v1,v2)
#define DEBUG_MSG_VAL3(s,v1,v2,v3)    Serial.printf(s,v1,v2,v3)
#define DEBUG_MSG_VAL4(s,v1,v2,v3,v4) Serial.printf(s,v1,v2,v3,v4)
#else
#define DEBUG_MSG(s)               
#define DEBUG_MSG_VAL(s,v)         
#define DEBUG_MSG_VAL2(s,v1,v2)    
#define DEBUG_MSG_VAL3(s,v1,v2,v3)
#define DEBUG_MSG_VAL4(s,v1,v2,v3,v4) 
#endif

#define SERIAL_BAUD_RATE      115200

#define I2C_HIGH_CLOCK_SPEED  400000
#define I2C_LOW_CLOCK_SPEED   100000

#define I2C_PORT_OLED         Wire1  // Undef for SPI display
#define I2C_PORT_EEPROM       Wire

#define OLED_ROTATE_FLAG      4         // 0 = no rotation, 4 = rotate 180 degrees

#define USE_SD_MMC

#define SPI_SD_SPEED          25000000  // 25 MHz
#define AUDIO_HEADER_TIMEOUT  7500      // in milliseconds

//=====================
//== Pin definitions ==
//=====================  

#if TARGET_BOARD == ESP32_WROVER_QVGA

// Buttons

#define PIN_SW1         0
#define PIN_SW2         36
#define PIN_SW3         39
#define PIN_SW4         34
#define PIN_SW5         35
 
// I2S is used for sound 
#define PIN_I2S_DOUT    32
#define PIN_I2S_BCK     33
#define PIN_I2S_WS      25

#ifdef USE_SD_MMC
  // SD card in MMC mode
  #define SD_MMC_CMD      15  //Please do not modify it.
  #define SD_MMC_CLK      14  //Please do not modify it. 
  #define SD_MMC_D0        2  //Please do not modify it.
#else
  // SD card in SPI mode
  #define PIN_SPI_MOSI    23
  #define PIN_SPI_MISO    19
  #define PIN_SPI_SCK     18
  #define PIN_SPI_SS       5
#endif

// I2C port 1 pins
#define PIN_SCL1        22
#define PIN_SDA1        21

// I2C port 2 pins
#ifdef I2C_PORT_OLED
  #define PIN_SCL2        13 
  #define PIN_SDA2        27
  #define ROTATE_DISPLAY  0
#else
  #ifndef USE_SD_MMC
    #error Cannot use SP display and SD card in SPI mode simultaneously
  #endif
  #define PIN_MISO  -1  //not used
  #define PIN_MOSI  23
  #define PIN_SCLK  18
  #define PIN_CS     5  // Chip select control pin
  #define PIN_DC    19  // Data Command control pin
  #define PIN_RST   -1  // Set TFT_RST to -1 if display RESET is connected to ESP32 board RST
  #define ROTATE_DISPLAY OLED_INIT_ROTATE180_MASK
#endif

// Switch IDs
#define KEY_SPARE        0
#define KEY_DN           1
#define KEY_UP           2
#define KEY_OK           3
#define KEY_MENU         4
#define KEY_LONG_DN      11
#define KEY_LONG_UP      12
#define KEY_NONE         0xFF

#define BOOT_KEY1_MASK   0x01
#define BOOT_KEY2_MASK   0x02
#define BOOT_KEY3_MASK   0x04
#define BOOT_KEY4_MASK   0x08
#define BOOT_KEY5_MASK   0x10

#elif TARGET_BOARD == ESP32_S3_PLAYER
  #error ESP32-S3 does not support A2DP, remove the BT player from the project if you want to use the ESP32-S3
#else
  #error Define other ESP32 boards here
#endif

//=====================
//== Other Constants ==
//=====================  

#define TIMESTAMP_BASE_RATE   1000000   // Microseconds rate (1 MHz)
#define TIMESTAMP_RATE_10MS   100       // for 100 Hz (10 ms period) rate
#define TIMESTAMP_RATE_100MS  10        // for 100 Hz (10 ms period) rate
#define TIMESTAMP_10MS_DIV   (TIMESTAMP_BASE_RATE / TIMESTAMP_RATE_10MS) 
#define TIMESTAMP_100MS_DIV  (TIMESTAMP_BASE_RATE / TIMESTAMP_RATE_100MS) 

//=====================
//==  Useful Macros  ==
//=====================

#define USE_ESP32_FAST_IO
// Use PIN_CLR  & PIN_SET  for GPIO0  - GPIO31
// Use PIN_CLR1 & PIN_SET1 for GPIO32 & GPIO33
#ifdef USE_ESP32_FAST_IO
  #define PIN_CLR(p)   REG_WRITE(GPIO_OUT_W1TC_REG, (uint32_t)1 << p)
  #define PIN_SET(p)   REG_WRITE(GPIO_OUT_W1TS_REG, (uint32_t)1 << p)
  #define PIN_CLR1(p)  REG_WRITE(GPIO_OUT1_W1TS_REG, (uint32_t)1 << (p - 32))
  #define PIN_SET1(p)  REG_WRITE(GPIO_OUT1_W1TC_REG, (uint32_t)1 << (p - 32))
#else
  #define PIN_CLR(p ) digitalWrite(p, LOW)
  #define PIN_SET(p)  digitalWrite(p, HIGH)
  #define PIN_CLR1    PIN_CLR
  #define PIN_SET1    PIN_SET
#endif

//===============================
//==  Global variables/classes ==
//=============================== 

//extern SPIClass QVGA_SPI;  // SPI port for QVGA Display
//extern SPIClass SD_SPI;    // SPI port for micro SD Card

//#define TSC_SPI  QVGA_SPI

#endif // _BOARD_H_