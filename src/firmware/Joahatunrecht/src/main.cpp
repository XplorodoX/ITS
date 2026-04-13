// ===== INCLUDES =====
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <AALeC-V3.h>

// ===== WiFi-KONFIGURATION =====
const char* ssid     = "leckeier";
const char* password = "12345678";

// ===== GLOBALE VARIABLEN =====
IPAddress clientIPs[20];
int clientCount = 0;
int selectedIndex = 0;

// ===== HILFSFUNKTION: Client-Liste aktualisieren =====
void updateClients() {
  clientCount = 0;
  struct station_info* station = wifi_softap_get_station_info();
  while (station != nullptr && clientCount < 20) {
    clientIPs[clientCount++] = IPAddress(station->ip.addr);
    station = STAILQ_NEXT(station, next);
  }
  wifi_softap_free_station_info();

  // selectedIndex im gültigen Bereich halten
  if (clientCount == 0) selectedIndex = 0;
  else if (selectedIndex >= clientCount) selectedIndex = clientCount - 1;
}

// ===== HILFSFUNKTION: Display aktualisieren =====
void updateDisplay() {
  aalec.clear_display();

  if (clientCount == 0) {
    aalec.print_line(0, "Clients: 0");
    aalec.print_line(1, "Niemand verbunden");
    return;
  }

  aalec.print_line(0, "Clients: " + String(clientCount));

  // 3 Einträge anzeigen, selected in der Mitte wenn möglich
  int start = selectedIndex - 1;
  if (start < 0) start = 0;
  if (start + 3 > clientCount) start = max(0, clientCount - 3);

  for (int i = 0; i < 3 && (start + i) < clientCount; i++) {
    int idx = start + i;
    String line = (idx == selectedIndex ? "> " : "  ") + clientIPs[idx].toString();
    aalec.print_line(i + 1, line);
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(9600);

  aalec.init();
  aalec.clear_display();
  aalec.set_rgb_strip(0, c_off);
  aalec.reset_rotate(0);

  WiFi.softAP(ssid, password);

  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(apIP);

  aalec.print_line(0, "AP gestartet");
  aalec.print_line(1, apIP.toString());
  delay(2000);
}

// ===== LOOP =====
unsigned long lastRefresh = 0;

void loop() {
  // Alle 3 Sekunden Client-Liste neu laden
  if (millis() - lastRefresh > 3000) {
    updateClients();
    lastRefresh = millis();
    updateDisplay();
  }

  // Drehknopf auslesen
  if (aalec.rotate_changed()) {
    int rot = aalec.get_rotate();
    if (rot > 0 && selectedIndex < clientCount - 1) selectedIndex++;
    else if (rot < 0 && selectedIndex > 0) selectedIndex--;
    aalec.reset_rotate(0);
    updateDisplay();
  }
}
