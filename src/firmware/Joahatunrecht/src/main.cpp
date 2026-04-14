// ===== INCLUDES =====
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <AALeC-V3.h>
#include "config.h"

// ===== WiFi-KONFIGURATION =====
const char* apSSID   = WIFI_SSID;
const char* apPass   = WIFI_PASSWORD;
bool isHosting = false;   // true = wir hosten selbst den AP

// ===== QUIZ STATE =====
enum QuizState { STATE_WAITING, STATE_VOTING, STATE_VOTED, STATE_REVEAL };
QuizState quizState = STATE_WAITING;

const char* answers[4] = { "Berlin", "Paris", "Madrid", "Rom" };
char correctAnswer      = 'B';
int  answerCounts[4]    = { 0, 0, 0, 0 };
int  selectedAnswer     = 0;   // 0=A, 1=B, 2=C, 3=D

// ===== FORWARD DECLARATIONS =====
void checkConnection();

// ===== MQTT =====
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

String deviceId;   // aAlec-<ChipID>

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("[MQTT] Nachricht auf '");
  Serial.print(topic);
  Serial.print("': ");
  Serial.write(payload, length);
  Serial.println();

  JsonDocument doc;
  if (deserializeJson(doc, payload, length) != DeserializationError::Ok) {
    Serial.println("[MQTT] JSON-Fehler, ignoriere Nachricht");
    return;
  }

  String t = String(topic);

  if (t == "quiz/state") {
    const char* state = doc["state"];
    Serial.print("[STATE] -> ");
    Serial.println(state);
    if      (strcmp(state, "QUESTION") == 0 || strcmp(state, "VOTING") == 0)
      quizState = STATE_VOTING;
    else if (strcmp(state, "REVEAL")   == 0)
      quizState = STATE_REVEAL;
    else if (strcmp(state, "WAITING")  == 0)
      quizState = STATE_WAITING;
  } else if (t == "quiz/reveal") {
    correctAnswer    = doc["correct"].as<String>()[0];
    answerCounts[0]  = doc["counts"]["A"] | 0;
    answerCounts[1]  = doc["counts"]["B"] | 0;
    answerCounts[2]  = doc["counts"]["C"] | 0;
    answerCounts[3]  = doc["counts"]["D"] | 0;
    Serial.print("[REVEAL] Richtige Antwort: ");
    Serial.println(correctAnswer);
    quizState = STATE_REVEAL;
  }
}

bool mqttReconnect() {
  if (mqtt.connected()) return true;
  Serial.print("[MQTT] Verbinde mit Broker ");
  Serial.print(MQTT_BROKER);
  Serial.print(":");
  Serial.print(MQTT_PORT);
  Serial.print(" als '");
  Serial.print(deviceId);
  Serial.println("' ...");
  String willTopic = "quiz/disconnect/" + deviceId;
  if (mqtt.connect(deviceId.c_str(), nullptr, nullptr, willTopic.c_str(), 0, false, "")) {
    Serial.println("[MQTT] Verbunden!");
    mqtt.subscribe("quiz/state");
    mqtt.subscribe("quiz/reveal");
    Serial.println("[MQTT] Subscribed: quiz/state, quiz/reveal");
    JsonDocument reg;
    reg["device_id"] = deviceId;
    reg["name"]      = deviceId;
    char buf[128];
    serializeJson(reg, buf);
    mqtt.publish(("quiz/connect/" + deviceId).c_str(), buf);
    Serial.print("[MQTT] Registrierung gesendet: ");
    Serial.println(buf);
    return true;
  }
  Serial.print("[MQTT] Verbindung fehlgeschlagen, rc=");
  Serial.println(mqtt.state());
  return false;
}

// ===== ANIMATION HELPERS =====

void drawSpinner(int cx, int cy, int r, int frame) {
  const int dx[] = { 0,  1,  1,  1,  0, -1, -1, -1};
  const int dy[] = {-1, -1,  0,  1,  1,  1,  0, -1};
  for (int i = 0; i < 8; i++) {
    if (i >= 5) {
      int idx = (i + frame) % 8;
      int px = cx + dx[idx] * r;
      int py = cy + dy[idx] * r;
      aalec.display.setColor(WHITE);
      aalec.display.fillRect(px - 1, py - 1, 3, 3);
    }
  }
}

void drawProgressBar(int x, int y, int w, int h, int percent) {
  int filled = map(percent, 0, 100, 0, w);
  aalec.display.setColor(WHITE);
  aalec.display.drawRect(x, y, w, h);
  if (filled > 0) aalec.display.fillRect(x, y, filled, h);
}

void pulseLEDs(unsigned long t, RgbColor base) {
  float s = 0.6f + 0.4f * sin(t / 400.0);
  RgbColor c = { (uint8_t)(base.r * s), (uint8_t)(base.g * s), (uint8_t)(base.b * s) };
  for (int i = 0; i < 5; i++) aalec.set_rgb_strip(i, c);
}

// ===== VERBINDUNGS-SCREEN =====
// Zeigt einen einzelnen Animations-Frame — muss wiederholt aufgerufen werden
int  _connDots = 0, _connSpin = 0;
unsigned long _connLast = 0;

void showConnectingFrame() {
  unsigned long now = millis();
  if (now - _connLast < 120) return;
  _connLast = now;
  _connSpin = (_connSpin + 1) % 8;
  _connDots = (_connDots + 1) % 4;

  pulseLEDs(now, c_blue);

  aalec.display.clear();
  aalec.display.setFont(ArialMT_Plain_10);
  aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
  aalec.display.drawString(64, 0, "AALeC Quiz");
  aalec.display.drawLine(0, 13, 128, 13);
  aalec.display.setTextAlignment(TEXT_ALIGN_LEFT);
  String dotStr = "Verbinde";
  for (int i = 0; i < _connDots; i++) dotStr += ".";
  aalec.display.drawString(4, 18, dotStr);
  drawSpinner(112, 24, 6, _connSpin);
  aalec.display.drawString(4, 33, ">" + String(apSSID));
  // display() wird vom Aufrufer gemacht, damit Countdown-Text noch ergänzt werden kann
}

// ===== VERBUNDEN-SCREEN =====
void showConnected() {
  for (int i = 0; i < 5; i++) aalec.set_rgb_strip(i, c_green);

  aalec.display.clear();
  aalec.display.setFont(ArialMT_Plain_10);
  aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
  aalec.display.drawString(64, 0, "AALeC Quiz");
  aalec.display.drawLine(0, 13, 128, 13);
  aalec.display.setFont(ArialMT_Plain_16);
  aalec.display.drawString(64, 18, "Verbunden!");
  aalec.display.setFont(ArialMT_Plain_10);
  aalec.display.drawString(64, 38, WiFi.localIP().toString());
  aalec.display.display();

  delay(2000);
  for (int i = 0; i < 5; i++) aalec.set_rgb_strip(i, c_off);
}

// ===== WARTE-SCREEN =====
void showWaiting() {
  int spinFrame = 0;
  unsigned long lastUpdate = 0;

  while (quizState == STATE_WAITING) {
    checkConnection();
    mqtt.loop();

    unsigned long now = millis();
    if (now - lastUpdate < 80) { delay(10); continue; }
    lastUpdate = now;
    spinFrame  = (spinFrame + 1) % 8;

    pulseLEDs(now, c_cyan);

    aalec.display.clear();
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
    aalec.display.drawString(64, 0, "AALeC Quiz");
    aalec.display.drawLine(0, 13, 128, 13);
    aalec.display.setFont(ArialMT_Plain_16);
    aalec.display.drawString(64, 16, "Bereit!");
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.drawString(64, 35, "Warte auf Quiz...");
    drawSpinner(64, 54, 5, spinFrame);
    aalec.display.setTextAlignment(TEXT_ALIGN_LEFT);
    aalec.display.drawString(0, 54, isHosting ? "[HOST]" : "[CLIENT]");
    aalec.display.display();

    // Demo: Button simuliert Quiz-Start
    if (aalec.get_button() == 1) {
      Serial.println("[INPUT] Button -> STATE_VOTING (Demo)");
      quizState = STATE_VOTING;
      aalec.reset_rotate(0);
      selectedAnswer = 0;
    }
  }
}

// ===== ANTWORT-SCREEN (A/B/C/D mit Drehknopf) =====
void showVoting() {
  for (int i = 0; i < 5; i++) aalec.set_rgb_strip(i, c_white);

  while (quizState == STATE_VOTING) {
    checkConnection();
    mqtt.loop();

    // Drehknopf navigiert A/B/C/D
    if (aalec.rotate_changed()) {
      int rot = aalec.get_rotate();
      selectedAnswer = constrain(selectedAnswer + (rot > 0 ? 1 : -1), 0, 3);
      aalec.reset_rotate(0);
    }

    // Button bestätigt Auswahl
    if (aalec.button_changed() && aalec.get_button() == 1) {
      char chosen = 'A' + selectedAnswer;
      Serial.print("[INPUT] Antwort gewaehlt: ");
      Serial.println(chosen);
      quizState = STATE_VOTED;
      JsonDocument ans;
      ans["answer"] = String(chosen);
      char buf[128];
      serializeJson(ans, buf);
      mqtt.publish(("quiz/answer/" + deviceId).c_str(), buf);
      Serial.print("[MQTT] Antwort gesendet: ");
      Serial.println(buf);
    }

    // Display zeichnen
    aalec.display.clear();

    // Header mit Fragetext (abgeschnitten wenn zu lang)
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
    aalec.display.drawString(64, 0, "Frage");
    aalec.display.drawLine(0, 13, 128, 13);

    // 4 Antwort-Felder in 2×2 Grid
    // A | B
    // C | D
    const char labels[4] = {'A', 'B', 'C', 'D'};
    const int gx[4] = {0,  64, 0,  64};
    const int gy[4] = {14, 14, 39, 39};
    const int gw = 62, gh = 23;

    for (int i = 0; i < 4; i++) {
      if (i == selectedAnswer) {
        // Ausgewählt: invertierter Block
        aalec.display.setColor(WHITE);
        aalec.display.fillRect(gx[i] + 1, gy[i], gw - 1, gh);
        aalec.display.setColor(BLACK);
      } else {
        aalec.display.setColor(WHITE);
        aalec.display.drawRect(gx[i] + 1, gy[i], gw - 1, gh);
      }

      // Label (A/B/C/D) links, Antworttext rechts davon
      aalec.display.setFont(ArialMT_Plain_16);
      aalec.display.setTextAlignment(TEXT_ALIGN_LEFT);
      aalec.display.drawString(gx[i] + 4, gy[i] + 3, String(labels[i]));

      aalec.display.setFont(ArialMT_Plain_10);
      // Antworttext kürzen wenn nötig
      String ans = String(answers[i]);
      if (ans.length() > 5) ans = ans.substring(0, 5);
      aalec.display.drawString(gx[i] + 22, gy[i] + 6, ans);

      aalec.display.setColor(WHITE);
    }

    // Untere Statuszeile
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
    aalec.display.drawString(64, 54, "Drehen=Wählen  Drücken=OK");

    aalec.display.display();
    delay(30);
  }
}

// ===== ABGESTIMMT-WARTE-SCREEN =====
void showVoted() {
  int spinFrame = 0;
  unsigned long lastUpdate = 0;

  for (int i = 0; i < 5; i++) aalec.set_rgb_strip(i, c_cyan);

  while (quizState == STATE_VOTED) {
    checkConnection();
    mqtt.loop();

    unsigned long now = millis();
    if (now - lastUpdate < 80) { delay(10); continue; }
    lastUpdate = now;
    spinFrame  = (spinFrame + 1) % 8;

    aalec.display.clear();
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
    aalec.display.drawString(64, 0, "AALeC Quiz");
    aalec.display.drawLine(0, 13, 128, 13);

    // Gewählte Antwort groß anzeigen
    aalec.display.setFont(ArialMT_Plain_16);
    String chosen = "Antwort: ";
    chosen += (char)('A' + selectedAnswer);
    aalec.display.drawString(64, 16, chosen);

    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.drawString(64, 35, "Warte auf Ergebnis...");
    drawSpinner(64, 50, 5, spinFrame);
    aalec.display.display();

    // Demo: Button simuliert Ergebnis
    if (aalec.get_button() == 1) quizState = STATE_REVEAL;
  }
}

// ===== ERGEBNIS-SCREEN =====
void showReveal() {
  bool correct = (selectedAnswer == (correctAnswer - 'A'));

  // LEDs: Grün = richtig, Rot = falsch
  for (int i = 0; i < 5; i++)
    aalec.set_rgb_strip(i, correct ? c_green : c_red);

  // Gesamtanzahl für Prozentberechnung
  int total = 0;
  for (int i = 0; i < 4; i++) total += answerCounts[i];
  if (total == 0) total = 1;

  while (quizState == STATE_REVEAL) {
    checkConnection();
    mqtt.loop();
    aalec.display.clear();
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);

    // Header
    aalec.display.drawString(64, 0, correct ? "RICHTIG!" : "FALSCH!");
    aalec.display.drawLine(0, 11, 128, 11);

    // Balkendiagramm für A/B/C/D
    const char labels[4] = {'A','B','C','D'};
    for (int i = 0; i < 4; i++) {
      int y   = 13 + i * 12;
      int pct = (answerCounts[i] * 100) / total;
      int bar = map(pct, 0, 100, 0, 90);

      // Label
      aalec.display.setTextAlignment(TEXT_ALIGN_LEFT);
      aalec.display.drawString(0, y, String(labels[i]));

      // Balken — korrekter Balken invertiert
      if (i == (correctAnswer - 'A')) {
        aalec.display.fillRect(12, y + 1, bar, 10);
        aalec.display.setColor(BLACK);
        aalec.display.drawString(14, y, String(pct) + "%");
        aalec.display.setColor(WHITE);
      } else {
        aalec.display.drawRect(12, y + 1, 90, 10);
        aalec.display.fillRect(12, y + 1, bar, 10);
        aalec.display.setColor(BLACK);
        if (bar > 20) aalec.display.drawString(14, y, String(pct) + "%");
        aalec.display.setColor(WHITE);
        if (bar <= 20) aalec.display.drawString(104, y, String(pct) + "%");
      }
    }

    aalec.display.display();
    delay(100);

    // Demo: Button → zurück zu WAITING
    if (aalec.get_button() == 1) {
      quizState = STATE_WAITING;
      for (int i = 0; i < 5; i++) aalec.set_rgb_strip(i, c_off);
    }
  }
}

// ===== RECONNECT NACH VERBINDUNGSVERLUST =====
// Gibt true zurück wenn Verbindung wiederhergestellt, false wenn Timeout → AP-Modus
bool handleConnectionLoss() {
  Serial.println("[WiFi] Verbindung verloren — starte Reconnect");
  mqtt.disconnect();
  WiFi.disconnect();
  delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.begin(apSSID, apPass);

  unsigned long startTime = millis();
  const unsigned long timeout = 60000;

  while (millis() - startTime < timeout) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[WiFi] Reconnect erfolgreich! IP: ");
      Serial.println(WiFi.localIP());

      mqtt.setServer(MQTT_BROKER, MQTT_PORT);
      mqtt.setCallback(mqttCallback);
      mqttReconnect();

      // Kurz "Verbunden!" zeigen
      for (int i = 0; i < 5; i++) aalec.set_rgb_strip(i, c_green);
      aalec.display.clear();
      aalec.display.setFont(ArialMT_Plain_10);
      aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
      aalec.display.drawString(64, 0, "AALeC Quiz");
      aalec.display.drawLine(0, 13, 128, 13);
      aalec.display.setFont(ArialMT_Plain_16);
      aalec.display.drawString(64, 18, "Verbunden!");
      aalec.display.setFont(ArialMT_Plain_10);
      aalec.display.drawString(64, 38, WiFi.localIP().toString());
      aalec.display.display();
      delay(1500);
      for (int i = 0; i < 5; i++) aalec.set_rgb_strip(i, c_off);

      isHosting    = false;
      quizState    = STATE_WAITING;
      return true;
    }

    unsigned long now = millis();
    if (now - _connLast >= 120) {
      unsigned long elapsed = now - startTime;
      showConnectingFrame();
      aalec.display.setFont(ArialMT_Plain_10);
      aalec.display.setTextAlignment(TEXT_ALIGN_RIGHT);
      aalec.display.drawString(124, 18, String((timeout - elapsed) / 1000) + "s");
      aalec.display.display();
    }
    delay(10);
  }

  // Timeout → AP-Modus
  Serial.println("[WiFi] Reconnect Timeout — starte AP");
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPass);
  isHosting = true;
  quizState = STATE_WAITING;

  for (int i = 0; i < 5; i++) aalec.set_rgb_strip(i, c_yellow);
  aalec.display.clear();
  aalec.display.setFont(ArialMT_Plain_10);
  aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
  aalec.display.drawString(64, 0, "AALeC Quiz");
  aalec.display.drawLine(0, 13, 128, 13);
  aalec.display.setFont(ArialMT_Plain_16);
  aalec.display.drawString(64, 16, "Hosting!");
  aalec.display.setFont(ArialMT_Plain_10);
  aalec.display.drawString(64, 34, apSSID);
  aalec.display.drawString(64, 46, WiFi.softAPIP().toString());
  aalec.display.setTextAlignment(TEXT_ALIGN_LEFT);
  aalec.display.drawString(0, 54, "[HOST]");
  aalec.display.display();
  delay(2000);
  for (int i = 0; i < 5; i++) aalec.set_rgb_strip(i, c_off);

  return false;
}

// Prüft nur WiFi — bei Verlust wird handleConnectionLoss() aufgerufen
// MQTT-Abriss allein löst keinen Reconnect aus
void checkConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    handleConnectionLoss();
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(100);
  deviceId = "aAlec-" + String(ESP.getChipId(), HEX);
  Serial.println("\n\n===== AALeC Quiz =====");
  Serial.print("[BOOT] Device ID: ");
  Serial.println(deviceId);
  aalec.init();
  aalec.display.clear();
  aalec.display.display();
  for (int i = 0; i < 5; i++) aalec.set_rgb_strip(i, c_off);
  aalec.reset_rotate(0);

  Serial.print("[WiFi] Verbinde mit '");
  Serial.print(apSSID);
  Serial.println("' ...");
  WiFi.persistent(false);      // kein Flash-Write bei jedem Verbindungsversuch
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(apSSID, apPass);

  unsigned long startTime = millis();
  bool connected = false;
  const unsigned long timeout = 60000;

  while (millis() - startTime < timeout) {
    if (WiFi.status() == WL_CONNECTED) { connected = true; break; }

    unsigned long now = millis();
    if (now - _connLast >= 120) {
      unsigned long elapsed = now - startTime;
      showConnectingFrame();   // zeichnet intern und setzt _connLast

      // Countdown oben rechts ergänzen (Display noch nicht geflusht)
      aalec.display.setFont(ArialMT_Plain_10);
      aalec.display.setTextAlignment(TEXT_ALIGN_RIGHT);
      aalec.display.drawString(124, 18, String((timeout - elapsed) / 1000) + "s");
      aalec.display.display();
    }

    delay(10);
  }

  if (connected) {
    isHosting = false;
    Serial.print("[WiFi] Verbunden! IP: ");
    Serial.println(WiFi.localIP());

    // MQTT einrichten
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqttReconnect();

    // Verbunden-Screen mit "CLIENT"-Badge
    for (int i = 0; i < 5; i++) aalec.set_rgb_strip(i, c_green);
    aalec.display.clear();
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
    aalec.display.drawString(64, 0, "AALeC Quiz");
    aalec.display.drawLine(0, 13, 128, 13);
    aalec.display.setFont(ArialMT_Plain_16);
    aalec.display.drawString(64, 16, "Verbunden!");
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.drawString(64, 35, WiFi.localIP().toString());
    // Badge unten links
    aalec.display.setTextAlignment(TEXT_ALIGN_LEFT);
    aalec.display.drawString(0, 54, "[CLIENT]");
    aalec.display.display();
    delay(2500);
    for (int i = 0; i < 5; i++) aalec.set_rgb_strip(i, c_off);

  } else {
    Serial.println("[WiFi] Timeout — starte eigenen AP");
    isHosting = true;
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPass);
    Serial.print("[WiFi] AP gestartet. IP: ");
    Serial.println(WiFi.softAPIP());

    // Hosting-Screen mit gelbem LED-Flash + Info
    for (int i = 0; i < 5; i++) aalec.set_rgb_strip(i, c_yellow);
    aalec.display.clear();
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
    aalec.display.drawString(64, 0, "AALeC Quiz");
    aalec.display.drawLine(0, 13, 128, 13);
    aalec.display.setFont(ArialMT_Plain_16);
    aalec.display.drawString(64, 16, "Hosting!");
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.drawString(64, 34, apSSID);
    aalec.display.drawString(64, 46, WiFi.softAPIP().toString());
    // Badge unten links
    aalec.display.setTextAlignment(TEXT_ALIGN_LEFT);
    aalec.display.drawString(0, 54, "[HOST]");
    aalec.display.display();
    delay(2500);
    for (int i = 0; i < 5; i++) aalec.set_rgb_strip(i, c_off);
  }
}

// ===== LOOP =====
void loop() {
  switch (quizState) {
    case STATE_WAITING: showWaiting(); break;
    case STATE_VOTING:  showVoting();  break;
    case STATE_VOTED:   showVoted();   break;
    case STATE_REVEAL:  showReveal();  break;
  }
}
