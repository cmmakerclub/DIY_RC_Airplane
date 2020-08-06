#include "CMMC_BootMode.h"

void CMMC_BootMode::init() {
  pinMode(this->_button_pin, INPUT_PULLUP);
}

void CMMC_BootMode::save() {
    x.crc32 = calculateCRC32((uint8_t*) &x.data[0], sizeof(x.data));
    if (ESP.rtcUserMemoryWrite(0, (uint32_t*) &x, sizeof(x))) {
      Serial.println("Write: ");
      printMemory();
      Serial.println();
    }
}

void CMMC_BootMode::load() {
  // Read struct from RTC memory
  if (ESP.rtcUserMemoryRead(0, (uint32_t*) &(this->x), sizeof((this->x)))) {
    Serial.println("Read: ");
    printMemory();
    Serial.println();
    uint32_t crcOfData = calculateCRC32((uint8_t*) &(x.data[0]), sizeof(x.data));
    Serial.print("CRC32 of data: ");
    Serial.println(crcOfData, HEX);
    Serial.print("CRC32 read from RTC: ");
    Serial.println(x.crc32, HEX);
    if (crcOfData != x.crc32) {
      Serial.println("CRC32 in RTC memory doesn't match CRC32 of data. Data is probably invalid!");
    } else {
      Serial.println("CRC32 check ok, data is probably valid.");
    }
  }
}

void CMMC_BootMode::check(cmmc_boot_mode_cb_t cb, uint32_t wait) {
  delay(wait);
  if (digitalRead(this->_button_pin) == LOW) {
    *_target_mode  = MODE_CONFIG;
  }
  else {
    *_target_mode  = MODE_RUN;
  }
  // printMemory();
  // finally fire the cb
  cb(*_target_mode);
}

uint32_t CMMC_BootMode::calculateCRC32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xffffffff;
  while (length--) {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1) {
      bool bit = crc & 0x80000000;
      if (c & i) {
        bit = !bit;
      }
      crc <<= 1;
      if (bit) {
        crc ^= 0x04c11db7;
      }
    }
  }
  return crc;
}

//prints all rtcData, including the leading crc32
void CMMC_BootMode::printMemory() {
  char buf[3];
  uint8_t *ptr = (uint8_t *)&(this->x);
  for (size_t i = 0; i < sizeof(rtcData); i++) {
    sprintf(buf, "%02X", ptr[i]);
    Serial.print(buf);
    if ((i + 1) % 32 == 0) {
      Serial.println();
    } else {
      Serial.print(" ");
    }
  }
  Serial.println();
}
