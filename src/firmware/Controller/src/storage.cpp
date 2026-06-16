#include "storage.h"
#include "globals.h"
#include <EEPROM.h>

void saveNameToEEPROM() {
  EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
  for (int i = 0; i < 5; i++)
    EEPROM.write(EEPROM_ADDR_NAME + i, playerName[i]);
  EEPROM.write(EEPROM_ADDR_NAME + 5, '\0');
  EEPROM.commit();
}

void loadNameFromEEPROM() {
  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) return;  // noch nie beschrieben
  for (int i = 0; i < 5; i++)
    playerName[i] = (char)EEPROM.read(EEPROM_ADDR_NAME + i);
  playerName[5] = '\0';
}

void clearNameInEEPROM() {
  EEPROM.write(EEPROM_ADDR_MAGIC, 0x00);
  for (int i = 0; i < 6; i++) {
    EEPROM.write(EEPROM_ADDR_NAME + i, '\0');
  }
  EEPROM.commit();
  playerName[0] = '\0';
}
