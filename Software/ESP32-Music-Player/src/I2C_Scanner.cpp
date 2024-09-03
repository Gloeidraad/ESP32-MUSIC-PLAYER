
#include <Arduino.h>
#include "I2C_Scanner.h"

bool Scan_I2C_Device(TwoWire &wire, uint8_t address) {
  wire.beginTransmission (address);
  return wire.endTransmission() == 0;
}

void Scan_I2C(TwoWire &wire, uint8_t bus_id) {
  Serial.printf("I2C scanner. Scanning bus %d...\n", bus_id);
  byte devices = 0;
  for (byte i = 8; i < 120; i++) {
   if(Scan_I2C_Device(wire, i)) {
      Serial.printf("  Found address: %d (0x%02X)\n", i, i);
      devices++;
      delay (1);  // maybe not needed?
    } // end of good response
  } // end of for loop
  Serial.printf("Done. Found %d device(s)\n", devices);
}
