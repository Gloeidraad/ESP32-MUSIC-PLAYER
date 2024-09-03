#ifndef _GUI_H_
#define _GUI_H_

#define GUI_RESULT_NOP                  0
#define GUI_RESULT_TEMP_PAUSE_PLAY    101
#define GUI_RESULT_PAUSE_PLAY         102
#define GUI_RESULT_NEXT_PLAY          103
#define GUI_RESULT_PREV_PLAY          104
#define GUI_RESULT_SEEK_PLAY          105
#define GUI_RESULT_TIMEOUT            106
#define GUI_RESULT_VOLUME             107
#define GUI_RESULT_NEW_TRACK          108
#define GUI_RESULT_NEW_AF_SOURCE      109
#define GUI_RESULT_NEW_SSID           110
#define GUI_RESULT_NEW_WIFI_TX_LEVEL  111
#define GUI_RESULT_NEW_BT_TX_LEVEL    112
#define GUI_RESULT_NEW_VOLUME         113

void GuiInit();
int  GuiCallback(int key_press);

#endif // _GUI_H_
