// ===== INCLUDES =====
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <AALeC-V3.h>
#include <EEPROM.h>

#include "globals.h"
#include "storage.h"
#include "audio.h"
#include "display.h"
#include "network.h"

// ===== GLOBAL VARIABLE DEFINITIONS =====

// Player Info
char playerName[6] = "";   // gewählter Name, leer = noch nicht gesetzt
const unsigned long NAME_RESET_HOLD_MS = 3000;

// Names List
char nameList[MAX_NAMES][MAX_NAME_LEN];
int  nameListCount = 0;
bool nameListReceived = false;
bool nameResetRequested = false;

// LED Brightness
const float LED_BRIGHTNESS = 0.12f;  // 12%

void setLED(int i, RgbColor c) {
  RgbColor d = {
    (uint8_t)(c.r * LED_BRIGHTNESS),
    (uint8_t)(c.g * LED_BRIGHTNESS),
    (uint8_t)(c.b * LED_BRIGHTNESS)
  };
  aalec.set_rgb_strip(i, d);
}

// WiFi Configuration
const char* apSSID   = WIFI_SSID;
const char* apPass   = WIFI_PASSWORD;
bool isHosting = false;   // true = wir hosten selbst den AP

// Quiz State
QuizState quizState       = STATE_WAITING;
bool      registeredByServer = false;   // true after quiz/ack received

// MCQ Data
char answers[4][32]     = { "?", "?", "?", "?" };
char correctAnswer      = 'A';
int  answerCounts[4]    = { 0, 0, 0, 0 };
int  selectedAnswer     = 0;   // 0=A … 3=D

// Estimate Data
int  estimateMin   = 0;
int  estimateMax   = 100;
int  estimateValue = 50;   // current value from analog potentiometer (A0)
int  estimateCorrectValue = 0;
char estimateUnit[16] = "";

// Poti Target
int  potiTarget    = 50;   // Zielwert in Prozent (0–100)
int  potiGuess     = 50;   // abgeschickter Poti-Wert
int  potiTolerance = 5;    // ±% Toleranz für "richtig"

// Temp Target
float tempTarget    = 25.0f;  // Zieltemperatur in °C
float tempGuess     = 0.0f;   // abgeschickte Temperatur
float tempTolerance = 1.5f;   // ±°C Toleranz für "richtig"

// Higher / Lower
int  hlReference = 0;
bool hlCorrectHigher = true;
char hlUnit[16]  = "";

// Common Question Info
QuestionType questionType    = QTYPE_MCQ;
int          currentQuestionId = 0;
int          timeLimitS        = 20;
unsigned long votingStartMs    = 0;
bool         revealWasCorrect  = false;  // set by quiz/reveal handler

// MQTT & Network Clients
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
String deviceId;   // aAlec-<ChipID>

// Screen state variables
int  _connDots = 0;
unsigned long _connLast = 0;

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(100);
  EEPROM.begin(EEPROM_SIZE);
  loadNameFromEEPROM();
  char chipIdHex[7];
  sprintf(chipIdHex, "%06X", ESP.getChipId());
  deviceId = "aAlec-" + String(chipIdHex);
  Serial.println("\n\n===== AALeC Quiz =====");
  Serial.print("[BOOT] Device ID: ");
  Serial.println(deviceId);
  aalec.init();
  aalec.display.clear();
  displayShow();
  for (int i = 0; i < 5; i++) setLED(i,c_off);
  aalec.reset_rotate(0);

  // Wenn kein Name im EEPROM vorhanden ist (z. B. beim ersten Start),
  // fordern wir den Spieler direkt zur Namenseingabe auf, noch vor dem WLAN-Verbindungsaufbau.
  if (strlen(playerName) == 0) {
    Serial.println("[BOOT] Kein Name im EEPROM vorhanden — fordere Eingabe an");
    showNameSelect();
  }

  Serial.print("[WiFi] Verbinde mit '");
  Serial.print(apSSID);
  Serial.println("' ...");
  WiFi.persistent(false);      // kein Flash-Write bei jedem Verbindungsversuch
  WiFi.setAutoReconnect(true); // Automatisches Reconnect durch den ESP ermöglichen
  WiFi.mode(WIFI_STA);
  WiFi.begin(apSSID, apPass);

  // Endlosschleife zur Verbindungssuche. Kein automatischer AP-Fallback mehr.
  while (WiFi.status() != WL_CONNECTED) {
    showConnectingFrame(c_blue);
    delay(10);
  }

  isHosting = false;
  Serial.print("[WiFi] Verbunden! IP: ");
  Serial.println(WiFi.localIP());

  // MQTT einrichten
  connectMqttAsPlayer();

  // Verbunden-Screen anzeigen
  for (int i = 0; i < 5; i++) setLED(i, c_green);
  aalec.display.clear();
  aalec.display.setFont(ArialMT_Plain_10);
  aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
  aalec.display.drawString(64, 0, "AALeC Quiz");
  aalec.display.drawLine(0, 13, 128, 13);
  aalec.display.setFont(ArialMT_Plain_16);
  aalec.display.drawString(64, 16, "Verbunden!");
  aalec.display.setFont(ArialMT_Plain_10);
  aalec.display.drawString(64, 35, WiFi.localIP().toString());
  displayShow();
  delay(2500);
  for (int i = 0; i < 5; i++) setLED(i, c_off);
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
