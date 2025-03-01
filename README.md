# ESP32-MUSIC-PLAYER
SD-Card, Webradio, Bluetooth music player based on an ESP32 Wrover module

This is an MP3 player, Web-radio player, Bluetooth player, and simple waveform generator, based on an ESP32 WROVER module. The output can be either a line-out connection or an on-board 2 x 3W @ 4 ohms power ampifier. The output type can be chosen by mounting matching module(s).

This design is derived from my project "AM/FM-modulator-with-ESP32-module" (https://github.com/Gloeidraad/AMFM-modulator-with-ESP32-module). 

The PCB design has been made with KiCad 8.0. 

There at two versions of the design: 
Version 1.1 is mostly based on through hole components. It uses the I2C version of the 2.4 inch OLED display. Some examples can be found at: https://nl.aliexpress.com/item/1005006345983913.html choose the version with the connector on the top and left side of the display. It can be equipped with a PCM5102A Module for line out audio, or with one or two MAX98357A Module(s), allowing to connect directly to loudspeakers with a maximum power of 3 Watt per channel.  
The second version V1.5 is mostly based on 1206 SMD devices, and can be built into a cheap Strapubox 2515WS or the similar TRU COMPONENTS TC-2515 housing, available at https://www.conrad.com. This version uses the SPI version of 2.4 inch OLED display, and supports only audio line output via a PCM5102A Module. The display can also be found on : https://nl.aliexpress.com/item/1005006345983913.html but choose the version  the 7 pins connector on the left side of the display. This version can also be equipped with a SC card adapter, which allows the SD-card to be inserted next to the display. In addition, a nice front cover is available. Both the SD adapter and the front cover are included in the V1.5 design files.
The software will automatically detect, which display is attached, so there is only one version of the software. Please note, that the software supports only ASCII code page 437 (OEM-US, OEM 437, PC-8, or MS-DOS Latin US) up to character 0xAF, so if you enter names and titles with special characters, please be sure that the data is stored in this format. An example of the web radio play list can be found in the project.
