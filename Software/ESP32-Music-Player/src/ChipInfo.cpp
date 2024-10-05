
#include <arduino.h>
#include "esp_chip_info.h"

#include "ChipInfo.h"

#if 0
void PrintChipInfo(void) {
  // See:
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/misc_system_api.html
  // https://github.com/espressif/esp-idf/blob/e7ca2f2622/components/esp_hw_support/include/esp_chip_info.h
  // http://www.londatiga.net/it/iot/how-to-get-hardware-info-of-esp32/
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  Serial.printf("ESP32 Model: %d (", chip_info.model);
  switch (chip_info.model) {
    case CHIP_ESP32   : Serial.println("ESP32)"); break;
    case CHIP_ESP32S2 : Serial.println("ESP32-S2)"); break;
    case CHIP_ESP32S3 : Serial.println("ESP32-S3)"); break;
    case CHIP_ESP32C3 : Serial.println("ESP32-C3)"); break;
    //case CHIP_ESP32H4 : Serial.println("ESP32-H4)"); break;
    //case CHIP_ESP32C2 : Serial.println("ESP32-C2)"); break;
    //case CHIP_ESP32C6 : Serial.println("ESP32-C6)"); break;
    //case CHIP_ESP32H2 : Serial.println("ESP32-H2)"); break;
    default: Serial.println("Unknown)"); break;
  }
  Serial.printf("  features: ");
  if (chip_info.features & 0x01) Serial.printf("embedded flash memory   ");
  if (chip_info.features & 0x02) Serial.printf("2.4GHz WiFi   ");
  if (chip_info.features & 0x10) Serial.printf("Bluetooth LE   ");
  if (chip_info.features & 0x20) Serial.printf("Bluetooth Classic   ");
  if (chip_info.features & 0x40) Serial.printf("IEEE 802.15.4   ");
  if (chip_info.features & 0x80) Serial.printf("embedded psram");
  Serial.println();
  Serial.printf("  revision: %d\n", chip_info.revision);
  Serial.printf("  cores   : %d\n", chip_info.cores);
}
#endif

// See: https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/Esp.h

void PrintChipInfo(int flags) {
  Serial.printf("ESP_IDF_VERSION : 0x%X\n", ESP_IDF_VERSION);
  // chip info
  if(flags & 0x01) {
    Serial.printf("ESP32 Model    : %s\n", ESP.getChipModel());
  
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    Serial.printf("  Features      : ");
    if (chip_info.features & 0x01) Serial.printf("embedded flash memory   ");
    if (chip_info.features & 0x02) Serial.printf("2.4GHz WiFi   ");
    if (chip_info.features & 0x10) Serial.printf("Bluetooth LE   ");
    if (chip_info.features & 0x20) Serial.printf("Bluetooth Classic   ");
    if (chip_info.features & 0x40) Serial.printf("IEEE 802.15.4   ");
    if (chip_info.features & 0x80) Serial.printf("embedded psram");
    Serial.println();
    Serial.printf("  Revision      : %d\n", chip_info.revision);
    Serial.printf("  Cores         : %d\n", chip_info.cores);
    Serial.printf("  Frequency     : %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("  SDK Version   : %s\n", ESP.getSdkVersion());
  }

  // Flash
  if(flags & 0x02) {
    Serial.printf("Flash szie      : %d\n", ESP.getFlashChipSize());
  }
 
  //Internal RAM
  if(flags & 0x0C) {
    Serial.printf("Total heap      : %d\n", ESP.getHeapSize());
    Serial.printf("Free heap       : %d\n", ESP.getFreeHeap());
  }
  if(flags & 0x08) {
    Serial.printf("Min free heap   : %d\n", ESP.getMinFreeHeap());
    Serial.printf("Max alloc heap  : %d\n", ESP.getMaxAllocHeap());
  }
 
  //SPI RAM
  if(flags & 0x0C) {
    Serial.printf("Total PSRAM     : %d\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM      : %d\n", ESP.getFreePsram());
  }
  if(flags & 0x08) {
    Serial.printf("Min free PSRAM  : %d\n", ESP.getMinFreePsram());
    Serial.printf("Max alloc PSRAM : %d\n", ESP.getMaxAllocPsram());
  }
}

