
#include "SdCard.h"
#include "Settings.h"

#if 1
// MMC-SD: see https://github.com/espressif/arduino-esp32/tree/master/libraries/SD_MMC/src

void InitSdCard(void) {
  #ifdef USE_SD_MMC
    pinMode(SD_MMC_D0, INPUT_PULLUP);
    SD_MMC.setPins(SD_MMC_CLK,SD_MMC_CMD, SD_MMC_D0);
  #else
    pinMode(PIN_SPI_SS, OUTPUT);
    digitalWrite(PIN_SPI_SS, HIGH);
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
    SPI.setFrequency(1000000);
  #endif
}

bool MountSdCard(void) {
  #ifdef USE_SD_MMC
    if(!SD_MMC.begin( "/sdcard", true)) {
      Settings.V().NoCard = 1;
      return false;
    }
  #else
    if (!SD.begin(PIN_SPI_SS, SPI, SPI_SD_SPEED)) {
      Settings.V().NoCard = 1;
      return false;
    }
  #endif
  return true;
}

void UnmountSdCard(void) {
  if(Settings.V().NoCard != 1) {
    #ifdef USE_SD_MMC
      SD_MMC.end(); 
    #else
      SD.end();
    #endif
  }
}
#endif
