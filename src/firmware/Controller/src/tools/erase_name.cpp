// Einmal-Utility: löscht nur den im EEPROM gespeicherten Spielernamen
// (Layout exakt wie in globals.h/storage.cpp), damit der nächste Boot
// wieder als "frisch geflasht" gilt und die Namenseingabe vor dem
// WLAN-Verbindungsaufbau auslöst.
//
// Bauen & flashen:   pio run -e erase_name -t upload
// Danach Serial-Monitor (115200) öffnen, Bestätigung abwarten, dann
// die normale Firmware zurückflashen: pio run -e esp12e -t upload

#include <Arduino.h>
#include <EEPROM.h>

#define EEPROM_SIZE       7
#define EEPROM_ADDR_MAGIC 0
#define EEPROM_ADDR_NAME  1

void setup() {
  Serial.begin(115200);
  delay(200);

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(EEPROM_ADDR_MAGIC, 0x00);
  for (int i = 0; i < 6; i++) EEPROM.write(EEPROM_ADDR_NAME + i, 0x00);
  EEPROM.commit();

  Serial.println("\n[ERASE] Name im EEPROM geloescht.");
  Serial.println("[ERASE] Jetzt die normale Firmware wieder flashen: pio run -e esp12e -t upload");
}

void loop() {
  // nichts zu tun
}
