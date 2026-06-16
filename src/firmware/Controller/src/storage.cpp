#include "storage.h"
#include "globals.h"
#include <EEPROM.h>

// EEPROM Best Practices (vgl. Vorlesung V06)
// Da der ESP8266 keinen echten EEPROM hat, wird dieser im Flash emuliert.
// Um unnötiges Schreiben zu vermeiden und die Lebenszeit des Flashs zu schonen,
// wird EEPROM.commit() nur bei expliziten Namensänderungen aufgerufen.

void saveNameToEEPROM() {
  // Magic Marker schreiben, um Initialisierung beim Booten zu erkennen
  EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
  for (int i = 0; i < 5; i++)
    EEPROM.write(EEPROM_ADDR_NAME + i, playerName[i]);
  EEPROM.write(EEPROM_ADDR_NAME + 5, '\0');
  
  // Änderungen werden erst durch commit() tatsächlich ins Flash geschrieben
  EEPROM.commit();
}

void loadNameFromEEPROM() {
  // Magic Marker prüfen: Verhindert das Laden von uninitialisiertem Rauschen/Müll
  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) return;  // noch nie beschrieben
  for (int i = 0; i < 5; i++)
    playerName[i] = (char)EEPROM.read(EEPROM_ADDR_NAME + i);
  playerName[5] = '\0';
}

void clearNameInEEPROM() {
  // Magic Marker ungültig machen
  EEPROM.write(EEPROM_ADDR_MAGIC, 0x00);
  for (int i = 0; i < 6; i++) {
    EEPROM.write(EEPROM_ADDR_NAME + i, '\0');
  }
  EEPROM.commit();
  playerName[0] = '\0';
}
