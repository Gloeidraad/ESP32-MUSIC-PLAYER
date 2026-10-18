#ifndef __SD_CARD_H__
#define __SD_CARD_H__

#include "Hardware/board.h" // For macro USE_SD_MMC

#ifdef USE_SD_MMC
  #define PLAYER_SD SD_MMC
#else
  #define PLAYER_SD SD
#endif

// MMC-SD: see https://github.com/espressif/arduino-esp32/tree/master/libraries/SD_MMC/src

void InitSdCard(void);
bool MountSdCard(void);
void UnmountSdCard(void);

#endif // __SD_CARD_H__
