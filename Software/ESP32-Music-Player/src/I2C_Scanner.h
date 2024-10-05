#ifndef _I2C_SCANNER_H_
#define _I2C_SCANNER_H_

#include <Wire.h>

void Scan_I2C(TwoWire &wire = Wire, uint8_t bus_id = 0);
bool Scan_I2C_Device(TwoWire &wire, uint8_t address);

#endif
