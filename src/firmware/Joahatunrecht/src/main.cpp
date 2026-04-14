// ===== INCLUDES =====
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <AALeC-V3.h>
#include <EEPROM.h>
#include "config.h"

// ===== EEPROM LAYOUT =====
// Byte 0      : magic marker (0xA1) — prüft ob EEPROM schon beschrieben wurde
// Bytes 1–6   : playerName (5 Zeichen + \0)
#define EEPROM_MAGIC      0xA1
#define EEPROM_SIZE       7
#define EEPROM_ADDR_MAGIC 0
#define EEPROM_ADDR_NAME  1

// ===== SPIELERNAME =====
char playerName[6] = "";   // gewählter Name, leer = noch nicht gesetzt
const unsigned long NAME_RESET_HOLD_MS = 3000;

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

// ===== NAMENSLISTE (Frontend -> Backend -> quiz/namelist) =====
#define MAX_NAMES      20
#define MAX_NAME_LEN   16
char nameList[MAX_NAMES][MAX_NAME_LEN];
int  nameListCount = 0;
bool nameListReceived = false;   // true sobald relay auf quiz/namelist eingetroffen
bool nameResetRequested = false; // remote reset via MQTT control path

#ifndef MQTT_BROKER_AP
#define MQTT_BROKER_AP ""
#endif

// ===== LED-HELLIGKEIT =====
// Alle LED-Aufrufe laufen durch setLED() — Helligkeit zentral über LED_BRIGHTNESS steuern.
static const float LED_BRIGHTNESS = 0.12f;  // 12% — anpassen nach Bedarf

void setLED(int i, RgbColor c) {
  RgbColor d = {
    (uint8_t)(c.r * LED_BRIGHTNESS),
    (uint8_t)(c.g * LED_BRIGHTNESS),
    (uint8_t)(c.b * LED_BRIGHTNESS)
  };
  aalec.set_rgb_strip(i, d);
}

// ===== WiFi-KONFIGURATION =====
const char* apSSID   = WIFI_SSID;
const char* apPass   = WIFI_PASSWORD;
bool isHosting = false;   // true = wir hosten selbst den AP

// ===== QUIZ STATE =====
enum QuizState { STATE_WAITING, STATE_VOTING, STATE_VOTED, STATE_REVEAL, STATE_ENDED };
QuizState quizState       = STATE_WAITING;
bool      registeredByServer = false;   // true after quiz/ack received

// ── MCQ ──────────────────────────────────────────────────────────────────────
char answers[4][32]     = { "?", "?", "?", "?" };
char correctAnswer      = 'A';
int  answerCounts[4]    = { 0, 0, 0, 0 };
int  selectedAnswer     = 0;   // 0=A … 3=D

// ── Estimate ──────────────────────────────────────────────────────────────────
int  estimateMin   = 0;
int  estimateMax   = 100;
int  estimateValue = 50;   // current value from analog potentiometer (A0)
int  estimateCorrectValue = 0;
char estimateUnit[16] = "";

// ── Poti Target ───────────────────────────────────────────────────────────────
int  potiTarget    = 50;   // Zielwert in Prozent (0–100)
int  potiGuess     = 50;   // abgeschickter Poti-Wert
int  potiTolerance = 5;    // ±% Toleranz für "richtig"

// ── Temp Target ───────────────────────────────────────────────────────────────
float tempTarget    = 25.0f;  // Zieltemperatur in °C
float tempGuess     = 0.0f;   // abgeschickte Temperatur
float tempTolerance = 1.5f;   // ±°C Toleranz für "richtig"

// ── Higher / Lower ────────────────────────────────────────────────────────────
int  hlReference = 0;
bool hlCorrectHigher = true;
char hlUnit[16]  = "";
// selectedAnswer reused: 0 = HÖHER, 1 = NIEDRIGER

// ── Common ───────────────────────────────────────────────────────────────────
enum QuestionType { QTYPE_MCQ, QTYPE_ESTIMATE, QTYPE_HIGHER_LOWER, QTYPE_POTI_TARGET, QTYPE_TEMP_TARGET };
QuestionType questionType    = QTYPE_MCQ;
int          currentQuestionId = 0;
int          timeLimitS        = 20;
unsigned long votingStartMs    = 0;
bool         revealWasCorrect  = false;  // set by quiz/reveal handler

// ===== FORWARD DECLARATIONS =====
void checkConnection();
void displayShow();
void connectMqttAsPlayer();
void publishConnect();

// ===== SOUND HELPERS =====
// Spielt eine Melodie blockierend (nur für kurze Sequenzen am Anfang eines Screens)
void playMelody(const unsigned int* notes, const unsigned int* durs, int len) {
  for (int i = 0; i < len; i++) {
    aalec.play(notes[i], durs[i]);
    delay(durs[i] + 20);  // Note + kurze Pause zwischen Tönen
  }
  aalec.play(t_off);
}

void soundSubmit() {
  // Kurzer Bestätigungs-Beep beim Abschicken der Antwort
  aalec.play(t_c_2, 80);
  delay(100);
  aalec.play(t_off);
}

void soundCorrect() {
  // Aufsteigende Fanfare: C → E → G → C2
  static const unsigned int notes[] = { t_c_1, t_e_1, t_g_1, t_c_2 };
  static const unsigned int durs[]  = { 120,   120,   120,   250   };
  playMelody(notes, durs, 4);
}

void soundWrong() {
  // Absteigende Misserfolgs-Melodie: A → D
  static const unsigned int notes[] = { t_a_1, t_d_1 };
  static const unsigned int durs[]  = { 200,   350   };
  playMelody(notes, durs, 2);
}

void soundWinner() {
  // Winner-Fanfare: C C G E C2 — G C2
  static const unsigned int notes[] = { t_c_1, t_c_1, t_g_1, t_e_1, t_c_2, t_g_1, t_c_2 };
  static const unsigned int durs[]  = { 100,   100,   100,   100,   200,   100,   400   };
  playMelody(notes, durs, 7);
}

// ===== MQTT =====
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

String deviceId;   // aAlec-<ChipID>

bool resolveBrokerIP(IPAddress& brokerIP) {
  const char* targets[2];
  uint8_t targetCount = 0;

  // Im Host/AP-Modus zuerst optionalen AP-spezifischen Broker versuchen.
  if (isHosting && strlen(MQTT_BROKER_AP) > 0) {
    targets[targetCount++] = MQTT_BROKER_AP;
  }
  targets[targetCount++] = MQTT_BROKER;

  for (uint8_t i = 0; i < targetCount; i++) {
    const char* target = targets[i];
    if (!target || strlen(target) == 0) continue;

    Serial.print("[MQTT] Broker-Ziel pruefen: ");
    Serial.println(target);

    if (brokerIP.fromString(target)) {
      Serial.print("[MQTT] Broker-IP (direkt): ");
      Serial.println(brokerIP.toString());
      return true;
    }

    if (WiFi.hostByName(target, brokerIP)) {
      Serial.print("[MQTT] Broker-IP (DNS/mDNS): ");
      Serial.println(brokerIP.toString());
      return true;
    }
  }

  Serial.println("[MQTT] Broker-Adresse nicht aufloesbar");
  if (isHosting) {
    Serial.println("[MQTT] Tipp: MQTT_BROKER_AP in config.h auf Client-IP setzen (z.B. 192.168.4.2)");
  }
  return false;
}

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

  if (t == "quiz/question") {
    currentQuestionId = doc["id"] | 0;
    timeLimitS        = doc["time_limit_s"] | 20;
    selectedAnswer    = 0;
    correctAnswer     = 'A';
    for (int i = 0; i < 4; i++) answerCounts[i] = 0;
    if (quizState == STATE_VOTED) quizState = STATE_WAITING;

    const char* typeStr = doc["type"] | "mcq";
    if (strcmp(typeStr, "estimate") == 0) {
      questionType  = QTYPE_ESTIMATE;
      estimateMin   = doc["min"] | 0;
      estimateMax   = doc["max"] | 100;
      estimateValue = (estimateMin + estimateMax) / 2;  // start in the middle
      const char* u = doc["unit"] | "";
      strncpy(estimateUnit, u, 15); estimateUnit[15] = '\0';
      Serial.printf("[QUESTION/estimate] min=%d max=%d unit=%s\n", estimateMin, estimateMax, estimateUnit);
    } else if (strcmp(typeStr, "higher_lower") == 0) {
      questionType = QTYPE_HIGHER_LOWER;
      hlReference  = doc["reference"] | 0;
      const char* u = doc["unit"] | "";
      strncpy(hlUnit, u, 15); hlUnit[15] = '\0';
      selectedAnswer = 0;  // 0=HÖHER, 1=NIEDRIGER
      Serial.printf("[QUESTION/higher_lower] reference=%d unit=%s\n", hlReference, hlUnit);
    } else if (strcmp(typeStr, "poti_target") == 0) {
      questionType   = QTYPE_POTI_TARGET;
      potiTarget     = doc["target"]    | 50;
      potiTolerance  = doc["tolerance"] | 5;
      potiGuess      = 50;
      Serial.printf("[QUESTION/poti_target] target=%d%% tol=%d%%\n", potiTarget, potiTolerance);
    } else if (strcmp(typeStr, "temp_target") == 0) {
      questionType   = QTYPE_TEMP_TARGET;
      tempTarget     = doc["target"]    | 25.0f;
      tempTolerance  = doc["tolerance"] | 1.5f;
      tempGuess      = aalec.get_temp();
      Serial.printf("[QUESTION/temp_target] target=%.1f tol=%.1f\n", tempTarget, tempTolerance);
    } else {
      questionType = QTYPE_MCQ;
      const char* opts[4] = { "A", "B", "C", "D" };
      for (int i = 0; i < 4; i++) {
        const char* val = doc["options"][opts[i]];
        if (val) strncpy(answers[i], val, 31);
        answers[i][31] = '\0';
      }
      Serial.println("[QUESTION/mcq]");
    }
    return;
  }

  if (t == "quiz/ack/" + deviceId) {
    registeredByServer = true;
    Serial.println("[ACK] Beim Server registriert!");
    return;
  }

  if (t == "quiz/namelist") {
    nameListCount = 0;
    JsonArray arr = doc["names"].as<JsonArray>();
    for (JsonVariant v : arr) {
      if (nameListCount >= MAX_NAMES) break;
      const char* n = v.as<const char*>();
      if (!n) continue;
      strncpy(nameList[nameListCount], n, MAX_NAME_LEN - 1);
      nameList[nameListCount][MAX_NAME_LEN - 1] = '\0';
      nameListCount++;
    }
    nameListReceived = (nameListCount > 0);
    Serial.printf("[NAMELIST] %d Namen empfangen\n", nameListCount);
    return;
  }

  if (t == "quiz/name/reset") {
    bool doReset = doc["reset"] | false;
    if (doReset) {
      Serial.println("[NAME] Remote reset empfangen");
      clearNameInEEPROM();
      nameResetRequested = true;
    }
    return;
  }

  if (t == "quiz/state") {
    const char* state = doc["state"];
    Serial.print("[STATE] -> ");
    Serial.println(state);
    if      (strcmp(state, "QUESTION") == 0 || strcmp(state, "VOTING") == 0) {
      if (quizState != STATE_VOTING && quizState != STATE_VOTED)
        votingStartMs = millis();
      quizState = STATE_VOTING;
    }
    // REVEAL wird NICHT hier gesetzt — erst quiz/reveal liefert die korrekte Antwort.
    // Würde man hier STATE_REVEAL setzen, läuft showReveal() mit dem alten correctAnswer.
    else if (strcmp(state, "ENDED")   == 0)
      quizState = STATE_ENDED;
    else if (strcmp(state, "WAITING") == 0) {
      quizState = STATE_WAITING;
      selectedAnswer   = 0;
      currentQuestionId = 0;
      for (int i = 0; i < 4; i++) answerCounts[i] = 0;
    }
  } else if (t == "quiz/reveal") {
    const char* revType = doc["type"] | "mcq";

    if (strcmp(revType, "poti_target") == 0) {
      int correct = doc["correct"] | potiTarget;
      int delta   = abs(potiGuess - correct);
      revealWasCorrect = (delta <= potiTolerance);
      answerCounts[0]  = correct;   // Zielwert zum Anzeigen
      Serial.printf("[REVEAL/poti_target] correct=%d guess=%d delta=%d\n", correct, potiGuess, delta);

    } else if (strcmp(revType, "temp_target") == 0) {
      float correct = doc["correct"] | tempTarget;
      float delta   = fabsf(tempGuess - correct);
      revealWasCorrect = (delta <= tempTolerance);
      answerCounts[0]  = (int)round(correct);   // zum Anzeigen
      Serial.printf("[REVEAL/temp_target] correct=%.1f guess=%.1f delta=%.1f\n", correct, tempGuess, delta);

    } else if (strcmp(revType, "estimate") == 0) {
      int correct = doc["correct"] | 0;
      estimateCorrectValue = correct;
      int delta   = abs(estimateValue - correct);
      int rng     = max(estimateMax - estimateMin, 1);
      float relErr = (float)delta / rng;
      revealWasCorrect = (relErr <= 0.30f);  // innerhalb 30% → "richtig genug"
      answerCounts[0] = correct;             // missbraucht zum Anzeigen
      Serial.printf("[REVEAL/estimate] correct=%d guess=%d delta=%d\n", correct, estimateValue, delta);

    } else if (strcmp(revType, "higher_lower") == 0) {
      String correctStr = doc["correct"].as<String>();
      correctStr.toUpperCase();
      bool guessedHigher = (selectedAnswer == 0);
      bool correctHigher = (correctStr == "HIGHER");
      hlCorrectHigher    = correctHigher;
      revealWasCorrect   = (guessedHigher == correctHigher);
      answerCounts[0]    = doc["counts"]["HIGHER"] | 0;
      answerCounts[1]    = doc["counts"]["LOWER"]  | 0;
      Serial.printf("[REVEAL/higher_lower] correct=%s guessed=%s\n",
                    correctStr.c_str(), guessedHigher ? "HIGHER" : "LOWER");

    } else {
      correctAnswer    = doc["correct"].as<String>()[0];
      answerCounts[0]  = doc["counts"]["A"] | 0;
      answerCounts[1]  = doc["counts"]["B"] | 0;
      answerCounts[2]  = doc["counts"]["C"] | 0;
      answerCounts[3]  = doc["counts"]["D"] | 0;
      revealWasCorrect = (selectedAnswer == (correctAnswer - 'A'));
      Serial.printf("[REVEAL/mcq] correct=%c\n", correctAnswer);
    }
    quizState = STATE_REVEAL;
  }
}

bool mqttReconnect() {
  if (mqtt.connected()) return true;

  // Broker-Adresse robust auflösen: direkte IP oder DNS/mDNS.
  IPAddress brokerIP;
  if (!resolveBrokerIP(brokerIP)) {
    Serial.println("[MQTT] Verbindung abgebrochen (Broker unbekannt)");
    return false;
  }

  mqtt.setServer(brokerIP, MQTT_PORT);
  mqtt.setBufferSize(512);

  Serial.print("[MQTT] Verbinde als '");
  Serial.print(deviceId);
  Serial.println("' ...");
  String willTopic = "quiz/disconnect/" + deviceId;
  registeredByServer = false;
  if (mqtt.connect(deviceId.c_str(), nullptr, nullptr, willTopic.c_str(), 0, false, "")) {
    Serial.println("[MQTT] Verbunden!");
    mqtt.subscribe("quiz/state");
    mqtt.subscribe("quiz/question");
    mqtt.subscribe("quiz/reveal");
    mqtt.subscribe("quiz/namelist");
    mqtt.subscribe("quiz/name/reset");
    mqtt.subscribe(("quiz/ack/" + deviceId).c_str());
    Serial.println("[MQTT] Subscribed (device topics): quiz/state, quiz/question, quiz/reveal, quiz/namelist, quiz/name/reset, quiz/ack");
    publishConnect();
    return true;
  }
  Serial.print("[MQTT] Verbindung fehlgeschlagen, rc=");
  Serial.println(mqtt.state());
  return false;
}

void publishConnect() {
  const char* name = (strlen(playerName) > 0) ? playerName : deviceId.c_str();
  JsonDocument reg;
  reg["device_id"] = deviceId;
  reg["name"]      = name;
  char buf[128];
  serializeJson(reg, buf);
  mqtt.publish(("quiz/connect/" + deviceId).c_str(), buf);
  Serial.print("[MQTT] Registrierung gesendet: ");
  Serial.println(buf);
}

void connectMqttAsPlayer() {
  mqtt.setCallback(mqttCallback);
  mqttReconnect();
}

// ===== DISPLAY HELPER =====
// Zeichnet einen kleinen Statuspunkt rechts unten und flusht das Display.
//   gefüllt  = beim Server registriert
//   Rahmen   = MQTT verbunden, aber noch kein ACK
//   nichts   = nicht verbunden
void displayShow() {
  if (mqtt.connected()) {
    aalec.display.setColor(WHITE);
    if (registeredByServer) {
      aalec.display.fillRect(122, 57, 5, 5);
    } else {
      aalec.display.drawRect(122, 57, 5, 5);
    }
  }
  aalec.display.display();
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
  for (int i = 0; i < 5; i++) setLED(i,c);
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
  for (int i = 0; i < 5; i++) setLED(i,c_green);

  aalec.display.clear();
  aalec.display.setFont(ArialMT_Plain_10);
  aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
  aalec.display.drawString(64, 0, "AALeC Quiz");
  aalec.display.drawLine(0, 13, 128, 13);
  aalec.display.setFont(ArialMT_Plain_16);
  aalec.display.drawString(64, 18, "Verbunden!");
  aalec.display.setFont(ArialMT_Plain_10);
  aalec.display.drawString(64, 38, WiFi.localIP().toString());
  displayShow();

  delay(2000);
  for (int i = 0; i < 5; i++) setLED(i,c_off);
}

// ===== NAMENSEINGABE-SCREEN =====
// Der Name wird direkt am AALeC eingegeben (max. 5 Zeichen).
// Rotary ändert Zeichen, Druck springt zum nächsten Zeichen,
// auf Position 5 speichert ein Druck den Namen.
void showNameSelect() {
  static const char charset[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  const int charsetLen = (int)strlen(charset);
  int idx[5] = {1, 1, 1, 1, 1}; // Start bei 'A'
  int cursor = 0;

  // Vorbelegung aus gespeichertem Namen (falls vorhanden)
  for (int i = 0; i < 5; i++) {
    char c = (i < (int)strlen(playerName)) ? playerName[i] : ' ';
    for (int j = 0; j < charsetLen; j++) {
      if (charset[j] == c) {
        idx[i] = j;
        break;
      }
    }
  }

  aalec.reset_rotate(0);

  while (true) {
    checkConnection();
    mqtt.loop();

    if (aalec.rotate_changed()) {
      int rot = aalec.get_rotate();
      idx[cursor] = (idx[cursor] + (rot > 0 ? -1 : 1) + charsetLen) % charsetLen;
      aalec.reset_rotate(0);
    }

    if (aalec.button_changed() && aalec.get_button() == 1) {
      if (cursor < 4) {
        cursor++;
      } else {
        char draft[6];
        for (int i = 0; i < 5; i++) draft[i] = charset[idx[i]];
        draft[5] = '\0';

        // trailing spaces entfernen, mindestens 1 Zeichen behalten
        int end = 4;
        while (end >= 0 && draft[end] == ' ') end--;
        if (end < 0) {
          draft[0] = 'A';
          end = 0;
        }

        for (int i = 0; i <= end; i++) playerName[i] = draft[i];
        playerName[end + 1] = '\0';

        saveNameToEEPROM();
        publishConnect(); // Name an Server senden/aktualisieren

        for (int i = 0; i < 5; i++) setLED(i, c_green);
        aalec.display.clear();
        aalec.display.setFont(ArialMT_Plain_10);
        aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
        aalec.display.drawString(64, 0, "AALeC Quiz");
        aalec.display.drawLine(0, 13, 128, 13);
        aalec.display.setFont(ArialMT_Plain_16);
        aalec.display.drawString(64, 20, "Hi, " + String(playerName) + "!");
        aalec.display.setFont(ArialMT_Plain_10);
        aalec.display.drawString(64, 42, "Name gespeichert");
        displayShow();
        delay(1200);
        for (int i = 0; i < 5; i++) setLED(i, c_off);
        return;
      }
    }

    pulseLEDs(millis(), c_cyan);
    aalec.display.clear();
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
    aalec.display.drawString(64, 0, "Name eingeben (5)");
    aalec.display.drawLine(0, 12, 128, 12);

    char preview[6];
    for (int i = 0; i < 5; i++) preview[i] = charset[idx[i]];
    preview[5] = '\0';

    aalec.display.setFont(ArialMT_Plain_24);
    aalec.display.drawString(64, 18, String(preview));

    int cursorX = 64 - 30 + cursor * 15;
    aalec.display.fillRect(cursorX, 46, 10, 2);

    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.drawString(64, 52, "Dreh=Zeichen  Taste=Weiter");
    displayShow();
    delay(20);
  }
}

// ===== WARTE-SCREEN =====
void showWaiting() {
  int spinFrame = 0;
  unsigned long lastUpdate = 0;
  unsigned long nameResetHoldStart = 0;
  bool waitForButtonReleaseAfterReset = false;

  while (quizState == STATE_WAITING) {
    checkConnection();
    mqtt.loop();

    // Kein Name gesetzt -> lokale Namenseingabe anzeigen
    if (strlen(playerName) == 0) {
      showNameSelect();
      nameResetRequested = false;
      continue;
    }

    if (nameResetRequested && strlen(playerName) == 0) {
      showNameSelect();
      nameResetRequested = false;
      continue;
    }

    // Optionaler Reset: Button 3s halten, dann Namenswahl erneut starten.
    if (strlen(playerName) > 0) {
      if (waitForButtonReleaseAfterReset) {
        if (aalec.get_button() == 0) {
          waitForButtonReleaseAfterReset = false;
        }
      } else if (aalec.get_button() == 1) {
        unsigned long now = millis();
        if (nameResetHoldStart == 0) {
          nameResetHoldStart = now;
        } else if (now - nameResetHoldStart >= NAME_RESET_HOLD_MS) {
          Serial.println("[NAME] Reset per Long-Press");
          clearNameInEEPROM();
          waitForButtonReleaseAfterReset = true;
          nameResetHoldStart = 0;
          showNameSelect();
          nameResetRequested = false;
          continue;
        }
      } else {
        nameResetHoldStart = 0;
      }
    }

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
    // Dezent aktuellen Namen unten rechts anzeigen
    if (strlen(playerName) > 0) {
      aalec.display.setTextAlignment(TEXT_ALIGN_RIGHT);
      aalec.display.drawString(122, 54, String(playerName));

      // Hinweis: bewusster Reset-Trigger statt dauerndem Wiederanzeigen.
      aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
      aalec.display.drawString(64, 44, "Taste 3s: Name neu");
    }
    displayShow();
  }
}

// ===== SCHÄTZFRAGE-SCREEN =====
void showEstimate() {
  while (quizState == STATE_VOTING) {
    checkConnection();
    mqtt.loop();

    // ── Countdown-LEDs ──────────────────────────────────────────────────────
    {
      unsigned long elapsed = millis() - votingStartMs;
      unsigned long limitMs = (unsigned long)timeLimitS * 1000UL;
      if (elapsed > limitMs) elapsed = limitMs;
      int ledsOn = (int)(((limitMs - elapsed) * 5UL + limitMs - 1) / limitMs);
      float pct  = (float)(limitMs - elapsed) / limitMs;
      for (int i = 0; i < 5; i++) {
        if (i < ledsOn) {
          RgbColor col = (pct > 0.6f) ? c_green : (pct > 0.4f) ? c_yellow : c_red;
          setLED(i, col);
        } else {
          setLED(i, c_off);
        }
      }
    }

    // ── Drehknopf ändert Schätzwert ─────────────────────────────────────────
    {
      uint16_t raw = aalec.get_analog();  // 0–1023 vom Poti (A0)
      estimateValue = estimateMin + (int)((long)raw * (estimateMax - estimateMin) / 1023);
      estimateValue = constrain(estimateValue, estimateMin, estimateMax);
    }

    // ── Button bestätigt ────────────────────────────────────────────────────
    if (aalec.button_changed() && aalec.get_button() == 1) {
      unsigned long elapsedMs = millis() - votingStartMs;
      soundSubmit();
      quizState = STATE_VOTED;
      JsonDocument ans;
      ans["question_id"] = currentQuestionId;
      ans["answer"]      = String(estimateValue);
      ans["elapsed_ms"]  = (int)elapsedMs;
      char buf[128];
      serializeJson(ans, buf);
      mqtt.publish(("quiz/answer/" + deviceId).c_str(), buf);
      Serial.printf("[MQTT] Schätzung gesendet: %s\n", buf);
    }

    // ── Display ─────────────────────────────────────────────────────────────
    aalec.display.clear();
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
    aalec.display.drawString(64, 0, "Schaetzfrage");
    aalec.display.drawLine(0, 13, 128, 13);

    // Großer Schätzwert mittig
    aalec.display.setFont(ArialMT_Plain_24);
    String valStr = String(estimateValue);
    if (strlen(estimateUnit) > 0) { valStr += " "; valStr += estimateUnit; }
    aalec.display.drawString(64, 20, valStr);

    // Min / Max als Orientierung
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_LEFT);
    aalec.display.drawString(0, 53, String(estimateMin));
    aalec.display.setTextAlignment(TEXT_ALIGN_RIGHT);
    aalec.display.drawString(128, 53, String(estimateMax));

    displayShow();
    delay(20);
  }
}

// ===== HÖHER / NIEDRIGER SCREEN =====
void showHigherLower() {
  while (quizState == STATE_VOTING) {
    checkConnection();
    mqtt.loop();

    // ── Countdown-LEDs ──────────────────────────────────────────────────────
    {
      unsigned long elapsed = millis() - votingStartMs;
      unsigned long limitMs = (unsigned long)timeLimitS * 1000UL;
      if (elapsed > limitMs) elapsed = limitMs;
      int ledsOn = (int)(((limitMs - elapsed) * 5UL + limitMs - 1) / limitMs);
      float pct  = (float)(limitMs - elapsed) / limitMs;
      for (int i = 0; i < 5; i++) {
        if (i < ledsOn) {
          RgbColor col = (pct > 0.6f) ? c_green : (pct > 0.4f) ? c_yellow : c_red;
          setLED(i, col);
        } else {
          setLED(i, c_off);
        }
      }
    }

    // ── Drehknopf wechselt HÖHER / NIEDRIGER ────────────────────────────────
    if (aalec.rotate_changed()) {
      int rot = aalec.get_rotate();
      selectedAnswer = constrain(selectedAnswer + (rot > 0 ? -1 : 1), 0, 1);
      aalec.reset_rotate(0);
    }

    // ── Button bestätigt ────────────────────────────────────────────────────
    if (aalec.button_changed() && aalec.get_button() == 1) {
      unsigned long elapsedMs = millis() - votingStartMs;
      soundSubmit();
      quizState = STATE_VOTED;
      String chosenStr = (selectedAnswer == 0) ? "HIGHER" : "LOWER";
      JsonDocument ans;
      ans["question_id"] = currentQuestionId;
      ans["answer"]      = chosenStr;
      ans["elapsed_ms"]  = (int)elapsedMs;
      char buf[128];
      serializeJson(ans, buf);
      mqtt.publish(("quiz/answer/" + deviceId).c_str(), buf);
      Serial.printf("[MQTT] Higher/Lower gesendet: %s\n", buf);
    }

    // ── Display ─────────────────────────────────────────────────────────────
    aalec.display.clear();
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
    aalec.display.drawString(64, 0, "Hoeher / Niedriger?");
    aalec.display.drawLine(0, 13, 128, 13);

    // Referenzwert
    aalec.display.setFont(ArialMT_Plain_16);
    String refStr = String(hlReference);
    if (strlen(hlUnit) > 0) { refStr += " "; refStr += hlUnit; }
    aalec.display.drawString(64, 14, refStr);

    // Zwei Felder: HÖHER (oben) und NIEDRIGER (unten)
    for (int i = 0; i < 2; i++) {
      int gy = 33 + i * 16;
      if (i == selectedAnswer) {
        aalec.display.setColor(WHITE);
        aalec.display.fillRect(1, gy, 126, 14);
        aalec.display.setColor(BLACK);
      } else {
        aalec.display.setColor(WHITE);
        aalec.display.drawRect(1, gy, 126, 14);
      }
      aalec.display.setFont(ArialMT_Plain_10);
      aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
      aalec.display.drawString(64, gy + 2, i == 0 ? "Hoeher" : "Niedriger");
      aalec.display.setColor(WHITE);
    }

    displayShow();
    delay(20);
  }
}

// ===== POTI-TARGET-SCREEN =====
void showPotiTarget() {
  while (quizState == STATE_VOTING) {
    checkConnection();
    mqtt.loop();

    // Poti lesen und auf 0–100% mappen
    uint16_t raw = aalec.get_analog();
    int potiNow  = (int)((long)raw * 100 / 1023);

    // Button bestätigt
    if (aalec.button_changed() && aalec.get_button() == 1) {
      potiGuess = potiNow;
      soundSubmit();
      quizState = STATE_VOTED;
      JsonDocument ans;
      ans["question_id"] = currentQuestionId;
      ans["answer"]      = String(potiGuess);
      ans["elapsed_ms"]  = (int)(millis() - votingStartMs);
      char buf[128];
      serializeJson(ans, buf);
      mqtt.publish(("quiz/answer/" + deviceId).c_str(), buf);
      Serial.printf("[MQTT] Poti-Target gesendet: %d%%\n", potiGuess);
    }

    // ── Display ─────────────────────────────────────────────────────────────
    aalec.display.clear();
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
    aalec.display.drawString(64, 0, "Poti-Challenge");
    aalec.display.drawLine(0, 13, 128, 13);

    // Ziel gross in der Mitte
    aalec.display.setFont(ArialMT_Plain_24);
    aalec.display.drawString(64, 16, "Ziel: " + String(potiTarget) + "%");

    // Aktueller Poti-Wert klein unten
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.drawString(64, 43, "Jetzt: " + String(potiNow) + "%");

    // Fortschrittsbalken (Poti-Position)
    int barW = map(potiNow, 0, 100, 0, 124);
    aalec.display.drawRect(2, 54, 124, 8);
    aalec.display.fillRect(2, 54, barW, 8);

    // Zielmarkierung auf dem Balken
    int targetX = map(potiTarget, 0, 100, 2, 126);
    aalec.display.drawLine(targetX, 52, targetX, 64);

    displayShow();
  }
}

// ===== TEMP-TARGET-SCREEN =====
void showTempTarget() {
  while (quizState == STATE_VOTING) {
    checkConnection();
    mqtt.loop();

    float tempNow = aalec.get_temp();
    float delta   = tempNow - tempTarget;

    // LEDs als Richtungsanzeige: kalt=blau, warm=rot, nah=grün
    float absDelta = fabsf(delta);
    for (int i = 0; i < 5; i++) {
      if      (absDelta <= tempTolerance)  setLED(i, { 0, 255, 0 });   // grün = nah dran
      else if (delta < 0)                  setLED(i, { 0, 0, 255 });   // blau = zu kalt
      else                                 setLED(i, { 255, 0, 0 });   // rot = zu warm
    }

    // Button bestätigt
    if (aalec.button_changed() && aalec.get_button() == 1) {
      tempGuess = tempNow;
      soundSubmit();
      quizState = STATE_VOTED;
      JsonDocument ans;
      ans["question_id"] = currentQuestionId;
      char tempBuf[10];
      dtostrf(tempGuess, 4, 1, tempBuf);
      ans["answer"]    = String(tempBuf);
      ans["elapsed_ms"] = (int)(millis() - votingStartMs);
      char buf[128];
      serializeJson(ans, buf);
      mqtt.publish(("quiz/answer/" + deviceId).c_str(), buf);
      Serial.printf("[MQTT] Temp-Target gesendet: %.1f°C\n", tempGuess);
    }

    // ── Display ─────────────────────────────────────────────────────────────
    aalec.display.clear();
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
    aalec.display.drawString(64, 0, "Temperatur-Challenge");
    aalec.display.drawLine(0, 13, 128, 13);

    // Zieltemperatur gross
    aalec.display.setFont(ArialMT_Plain_24);
    char tgtBuf[12];
    dtostrf(tempTarget, 4, 1, tgtBuf);
    aalec.display.drawString(64, 14, "Ziel: " + String(tgtBuf) + "C");

    // Aktuelle Temperatur + Richtungspfeil
    aalec.display.setFont(ArialMT_Plain_10);
    char nowBuf[12];
    dtostrf(tempNow, 4, 1, nowBuf);
    String hint = (absDelta <= tempTolerance) ? "OK!" : (delta < 0 ? "waermer!" : "kaelter!");
    aalec.display.drawString(64, 41, "Jetzt: " + String(nowBuf) + "C  " + hint);

    // Abweichungs-Balken: Mitte = Ziel, links/rechts = Abweichung
    int barCenter = 64;
    int deviation = (int)constrain(delta * 5, -60, 60);  // ±12°C = voller Balken
    aalec.display.drawRect(2, 54, 124, 8);
    aalec.display.drawLine(barCenter, 52, barCenter, 64);  // Zielmarkierung Mitte
    if (deviation > 0)
      aalec.display.fillRect(barCenter, 54, deviation, 8);
    else
      aalec.display.fillRect(barCenter + deviation, 54, -deviation, 8);

    displayShow();
  }
}

// ===== ANTWORT-SCREEN (A/B/C/D mit Drehknopf) =====
void showVoting() {
  while (quizState == STATE_VOTING) {
    checkConnection();
    mqtt.loop();

    // ── Countdown-LEDs (5 = voll, 0 = Zeit abgelaufen) ──────────────
    // Farbe: grün → gelb → rot je nach verbleibender Zeit
    {
      unsigned long elapsed = millis() - votingStartMs;
      unsigned long limitMs = (unsigned long)timeLimitS * 1000UL;
      if (elapsed > limitMs) elapsed = limitMs;
      // Wie viele LEDs noch leuchten sollen (5 → 0)
      int ledsOn = (int)(((limitMs - elapsed) * 5UL + limitMs - 1) / limitMs);

      for (int i = 0; i < 5; i++) {
        if (i < ledsOn) {
          // Farbe abhängig von verbleibender Zeit
          // >60%: grün  40-60%: gelb  <40%: rot
          float pct = (float)(limitMs - elapsed) / limitMs;
          RgbColor col;
          if      (pct > 0.6f) col = c_green;
          else if (pct > 0.4f) col = c_yellow;
          else                  col = c_red;
          setLED(i,col);
        } else {
          setLED(i,c_off);
        }
      }
    }

    // Drehknopf navigiert A/B/C/D
    if (aalec.rotate_changed()) {
      int rot = aalec.get_rotate();
      selectedAnswer = constrain(selectedAnswer + (rot > 0 ? -1 : 1), 0, 3);
      aalec.reset_rotate(0);
    }

    // Button bestätigt Auswahl
    if (aalec.button_changed() && aalec.get_button() == 1) {
      char chosen = 'A' + selectedAnswer;
      unsigned long elapsedMs = millis() - votingStartMs;
      Serial.print("[INPUT] Antwort gewaehlt: ");
      Serial.println(chosen);
      soundSubmit();
      quizState = STATE_VOTED;
      JsonDocument ans;
      ans["question_id"] = currentQuestionId;
      ans["answer"]      = String(chosen);
      ans["elapsed_ms"]  = (int)elapsedMs;
      char buf[128];
      serializeJson(ans, buf);
      mqtt.publish(("quiz/answer/" + deviceId).c_str(), buf);
      Serial.print("[MQTT] Antwort gesendet: ");
      Serial.println(buf);
    }

    // Display zeichnen — nur A/B/C/D, kein Antworttext
    aalec.display.clear();

    // Header
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
    aalec.display.drawString(64, 0, "Frage");
    aalec.display.drawLine(0, 13, 128, 13);

    // 4 Felder in 2×2 Grid — nur Buchstabe zentriert
    const char labels[4] = {'A', 'B', 'C', 'D'};
    const int gx[4] = {0,  64, 0,  64};
    const int gy[4] = {14, 14, 39, 39};
    const int gw = 62, gh = 23;

    for (int i = 0; i < 4; i++) {
      if (i == selectedAnswer) {
        aalec.display.setColor(WHITE);
        aalec.display.fillRect(gx[i] + 1, gy[i], gw - 1, gh);
        aalec.display.setColor(BLACK);
      } else {
        aalec.display.setColor(WHITE);
        aalec.display.drawRect(gx[i] + 1, gy[i], gw - 1, gh);
      }

      // Buchstabe groß, mittig im Feld
      aalec.display.setFont(ArialMT_Plain_16);
      aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
      aalec.display.drawString(gx[i] + gw / 2, gy[i] + 3, String(labels[i]));

      aalec.display.setColor(WHITE);
    }

    displayShow();
    delay(30);
  }
}

// ===== ABGESTIMMT-WARTE-SCREEN =====
void showVoted() {
  int spinFrame = 0;
  unsigned long lastUpdate = 0;

  while (quizState == STATE_VOTED) {
    checkConnection();
    mqtt.loop();

    // Countdown-LEDs laufen weiter, auch wenn schon abgestimmt wurde.
    {
      unsigned long elapsed = millis() - votingStartMs;
      unsigned long limitMs = (unsigned long)timeLimitS * 1000UL;
      if (elapsed > limitMs) elapsed = limitMs;
      int ledsOn = (int)(((limitMs - elapsed) * 5UL + limitMs - 1) / limitMs);
      float pct  = (float)(limitMs - elapsed) / limitMs;
      for (int i = 0; i < 5; i++) {
        if (i < ledsOn) {
          RgbColor col = (pct > 0.6f) ? c_green : (pct > 0.4f) ? c_yellow : c_red;
          setLED(i, col);
        } else {
          setLED(i, c_off);
        }
      }
    }

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
    if (questionType == QTYPE_HIGHER_LOWER) {
      chosen += (selectedAnswer == 0) ? "Hoeher" : "Niedriger";
    } else if (questionType == QTYPE_ESTIMATE) {
      chosen += String(estimateValue);
      if (strlen(estimateUnit) > 0) {
        chosen += " ";
        chosen += estimateUnit;
      }
    } else {
      chosen += (char)('A' + selectedAnswer);
    }
    aalec.display.drawString(64, 16, chosen);

    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.drawString(64, 35, "Warte auf Ergebnis...");
    drawSpinner(64, 50, 5, spinFrame);
    displayShow();
  }
}

// ===== ERGEBNIS-SCREEN =====
void showReveal() {
  bool correct = revealWasCorrect;

  // LEDs: Grün = richtig, Rot = falsch
  for (int i = 0; i < 5; i++)
    setLED(i,correct ? c_green : c_red);

  // Sound: Fanfare bei richtig, Misserfolg bei falsch
  if (correct) soundCorrect(); else soundWrong();

  while (quizState == STATE_REVEAL) {
    checkConnection();
    mqtt.loop();
    aalec.display.clear();
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);

    // Header
    aalec.display.drawString(64, 0, correct ? "RICHTIG!" : "FALSCH!");
    aalec.display.drawLine(0, 11, 128, 11);

    if (questionType == QTYPE_ESTIMATE) {
      int delta = abs(estimateValue - estimateCorrectValue);
      String unit = strlen(estimateUnit) > 0 ? String(" ") + estimateUnit : "";

      aalec.display.setFont(ArialMT_Plain_10);
      aalec.display.drawString(64, 14, "Tipp: " + String(estimateValue) + unit);
      aalec.display.drawString(64, 27, "Korrekt: " + String(estimateCorrectValue) + unit);
      aalec.display.drawString(64, 40, "Abweichung: " + String(delta) + unit);
      aalec.display.drawString(64, 52, correct ? "Im Toleranzbereich" : "Zu weit daneben");

    } else if (questionType == QTYPE_HIGHER_LOWER) {
      int total = answerCounts[0] + answerCounts[1];
      if (total == 0) total = 1;

      const char* labels[2] = {"Hoeher", "Niedriger"};
      int correctIdx = hlCorrectHigher ? 0 : 1;

      aalec.display.setFont(ArialMT_Plain_10);
      aalec.display.drawString(64, 12, String("Korrekt: ") + labels[correctIdx]);

      for (int i = 0; i < 2; i++) {
        int y   = 22 + i * 14;
        int pct = (answerCounts[i] * 100) / total;
        int bar = map(pct, 0, 100, 0, 66);

        aalec.display.setTextAlignment(TEXT_ALIGN_LEFT);
        aalec.display.drawString(0, y, i == 0 ? "H" : "N");

        if (i == correctIdx) {
          aalec.display.fillRect(11, y + 1, bar, 10);
          aalec.display.setColor(BLACK);
          if (bar > 18) aalec.display.drawString(13, y, String(pct) + "%");
          aalec.display.setColor(WHITE);
          if (bar <= 18) aalec.display.drawString(80, y, String(pct) + "%");
        } else {
          aalec.display.drawRect(11, y + 1, 66, 10);
          aalec.display.fillRect(11, y + 1, bar, 10);
          aalec.display.setColor(BLACK);
          if (bar > 18) aalec.display.drawString(13, y, String(pct) + "%");
          aalec.display.setColor(WHITE);
          if (bar <= 18) aalec.display.drawString(80, y, String(pct) + "%");
        }
      }

      aalec.display.setTextAlignment(TEXT_ALIGN_LEFT);
      aalec.display.drawString(0, 50, String("Du: ") + (selectedAnswer == 0 ? "Hoeher" : "Niedriger"));
      aalec.display.setTextAlignment(TEXT_ALIGN_RIGHT);
      aalec.display.drawString(128, 50, String("Geg.: ") + labels[correctIdx]);
      aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);

    } else {
      // MCQ-Reveal mit A/B/C/D-Balkendiagramm
      int total = 0;
      for (int i = 0; i < 4; i++) total += answerCounts[i];
      if (total == 0) total = 1;

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
    }

    displayShow();
    delay(100);
  }
}

// ===== QUIZ ENDE =====
void showEnded() {
  int spinFrame  = 0;
  unsigned long lastUpdate = 0;

  // Goldene LEDs für Gewinner-Atmosphäre
  for (int i = 0; i < 5; i++) setLED(i,c_yellow);

  // Winner-Melodie einmal abspielen
  soundWinner();

  while (quizState == STATE_ENDED) {
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

    aalec.display.setFont(ArialMT_Plain_16);
    aalec.display.drawString(64, 16, "Quiz beendet!");

    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.drawString(64, 36, "Neu starten?");
    aalec.display.drawString(64, 48, "Druecken zum Neustart");

    displayShow();

    // Button → Neustart anfordern
    if (aalec.button_changed() && aalec.get_button() == 1) {
      Serial.println("[INPUT] Neustart angefordert");
      JsonDocument ctrl;
      ctrl["action"] = "restart";
      char buf[64];
      serializeJson(ctrl, buf);
      mqtt.publish("quiz/control", buf);
      // Warte kurz auf die STATE→WAITING Antwort vom Server
      delay(500);
    }
  }

  for (int i = 0; i < 5; i++) setLED(i,c_off);
}

// ===== RECONNECT NACH VERBINDUNGSVERLUST =====

// Wie viele aufeinanderfolgende Checks WiFi nicht-verbunden sein muss,
// bevor ein echter Reconnect gestartet wird (verhindert Flicker-Disconnects).
static uint8_t _wifiFailCount = 0;
static const uint8_t WIFI_FAIL_THRESHOLD = 3;

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

      connectMqttAsPlayer();

      // Kurz "Verbunden!" zeigen
      for (int i = 0; i < 5; i++) setLED(i,c_green);
      aalec.display.clear();
      aalec.display.setFont(ArialMT_Plain_10);
      aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
      aalec.display.drawString(64, 0, "AALeC Quiz");
      aalec.display.drawLine(0, 13, 128, 13);
      aalec.display.setFont(ArialMT_Plain_16);
      aalec.display.drawString(64, 18, "Verbunden!");
      aalec.display.setFont(ArialMT_Plain_10);
      aalec.display.drawString(64, 38, WiFi.localIP().toString());
      displayShow();
      delay(1500);
      for (int i = 0; i < 5; i++) setLED(i,c_off);

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
      displayShow();
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
  connectMqttAsPlayer();

  for (int i = 0; i < 5; i++) setLED(i,c_yellow);
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
  displayShow();
  delay(2000);
  for (int i = 0; i < 5; i++) setLED(i,c_off);

  return false;
}

// Prüft WiFi (mit Debounce) und MQTT getrennt.
// WiFi-Flicker (kurzes WL_DISCONNECTED) löst keinen Reconnect aus.
void checkConnection() {
  if (isHosting) {
    // Als Host bleiben wir trotzdem MQTT-Client und spielen mit.
    if (!mqtt.connected()) {
      Serial.println("[MQTT] Host-Modus ohne MQTT — reconnect …");
      mqttReconnect();
    }
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    _wifiFailCount++;
    if (_wifiFailCount >= WIFI_FAIL_THRESHOLD) {
      _wifiFailCount = 0;
      handleConnectionLoss();
    }
    return;
  }
  _wifiFailCount = 0;

  // WiFi ist ok — prüfe MQTT separat
  if (!mqtt.connected()) {
    Serial.println("[MQTT] Verbindung verloren — reconnect …");
    mqttReconnect();
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(100);
  EEPROM.begin(EEPROM_SIZE);
  loadNameFromEEPROM();
  deviceId = "aAlec-" + String(ESP.getChipId(), HEX);
  Serial.println("\n\n===== AALeC Quiz =====");
  Serial.print("[BOOT] Device ID: ");
  Serial.println(deviceId);
  aalec.init();
  aalec.display.clear();
  displayShow();
  for (int i = 0; i < 5; i++) setLED(i,c_off);
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
      displayShow();
    }

    delay(10);
  }

  if (connected) {
    isHosting = false;
    Serial.print("[WiFi] Verbunden! IP: ");
    Serial.println(WiFi.localIP());

    // MQTT einrichten
    connectMqttAsPlayer();

    // Verbunden-Screen mit "CLIENT"-Badge
    for (int i = 0; i < 5; i++) setLED(i,c_green);
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
    displayShow();
    delay(2500);
    for (int i = 0; i < 5; i++) setLED(i,c_off);

  } else {
    Serial.println("[WiFi] Timeout — starte eigenen AP");
    isHosting = true;
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPass);
    connectMqttAsPlayer();
    Serial.print("[WiFi] AP gestartet. IP: ");
    Serial.println(WiFi.softAPIP());

    // Hosting-Screen mit gelbem LED-Flash + Info
    for (int i = 0; i < 5; i++) setLED(i,c_yellow);
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
    displayShow();
    delay(2500);
    for (int i = 0; i < 5; i++) setLED(i,c_off);
  }
}

// ===== LOOP =====
void loop() {
  switch (quizState) {
    case STATE_WAITING: showWaiting(); break;
    case STATE_VOTING:
      if      (questionType == QTYPE_ESTIMATE)     showEstimate();
      else if (questionType == QTYPE_HIGHER_LOWER) showHigherLower();
      else if (questionType == QTYPE_POTI_TARGET)  showPotiTarget();
      else if (questionType == QTYPE_TEMP_TARGET)  showTempTarget();
      else                                         showVoting();
      break;
    case STATE_VOTED:   showVoted();   break;
    case STATE_REVEAL:  showReveal();  break;
    case STATE_ENDED:   showEnded();   break;
  }
}
