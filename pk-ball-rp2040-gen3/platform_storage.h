#ifndef PLATFORM_STORAGE_H_
#define PLATFORM_STORAGE_H_

#include <EEPROM.h>

static int storageConfiguredLength = 0;

static inline void storageBegin(int length) {
  storageConfiguredLength = length;
#if defined(ARDUINO_ARCH_RP2040)
  EEPROM.begin(length);
#else
  (void)length;
#endif
}

static inline uint8_t storageRead(int address) {
  return EEPROM.read(address);
}

static inline void storageWrite(int address, uint8_t value) {
  EEPROM.write(address, value);
}

static inline int storageLength(void) {
  return storageConfiguredLength;
}

static inline void storageCommit(void) {
#if defined(ARDUINO_ARCH_RP2040)
  EEPROM.commit();
#endif
}

#endif
