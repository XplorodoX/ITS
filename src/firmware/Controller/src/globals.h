#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <AALeC-V3.h>
#include "config.h"

// ===== EEPROM LAYOUT =====
#define EEPROM_MAGIC      0xA1
#define EEPROM_SIZE       7
#define EEPROM_ADDR_MAGIC 0
#define EEPROM_ADDR_NAME  1

// ===== Player Info =====
extern char playerName[6];
extern const unsigned long NAME_RESET_HOLD_MS;

// ===== Names List =====
#define MAX_NAMES      20
#define MAX_NAME_LEN   16
extern char nameList[MAX_NAMES][MAX_NAME_LEN];
extern int  nameListCount;
extern bool nameListReceived;
extern bool nameResetRequested;

// ===== LED brightness =====
extern const float LED_BRIGHTNESS;
void setLED(int i, RgbColor c);

// ===== WiFi =====
extern const char* apSSID;
extern const char* apPass;
extern bool isHosting;

// ===== Quiz State =====
enum QuizState { STATE_WAITING, STATE_VOTING, STATE_VOTED, STATE_REVEAL, STATE_ENDED };
extern QuizState quizState;
extern bool registeredByServer;

// ===== Question Types & Data =====
enum QuestionType { QTYPE_MCQ, QTYPE_ESTIMATE, QTYPE_HIGHER_LOWER, QTYPE_POTI_TARGET, QTYPE_TEMP_TARGET };
extern QuestionType questionType;
extern int          currentQuestionId;
extern int          timeLimitS;
extern unsigned long votingStartMs;
extern bool         revealWasCorrect;

// MCQ
extern char answers[4][32];
extern char correctAnswer;
extern int  answerCounts[4];
extern int  selectedAnswer;

// Estimate
extern int  estimateMin;
extern int  estimateMax;
extern int  estimateValue;
extern int  estimateCorrectValue;
extern char estimateUnit[16];

// Poti Target
extern int  potiTarget;
extern int  potiGuess;
extern int  potiTolerance;

// Temp Target
extern float tempTarget;
extern float tempGuess;
extern float tempTolerance;

// Higher / Lower
extern int  hlReference;
extern bool hlCorrectHigher;
extern char hlUnit[16];

// ===== MQTT & Network =====
extern WiFiClient   wifiClient;
extern PubSubClient mqtt;
extern String deviceId;

// ===== Screen state variables =====
extern int _connDots;
extern unsigned long _connLast;

// ===== MODULE FUNCTION PROTOTYPES =====

// Storage Module
void saveNameToEEPROM();
void loadNameFromEEPROM();
void clearNameInEEPROM();

// Audio Module
void playMelody(const unsigned int* notes, const unsigned int* durs, int len);
void soundSubmit();
void soundCorrect();
void soundWrong();
void soundWinner();

// Display Module
void displayShow();
void drawSpinner(int cx, int cy, int r, int frame);
void drawProgressBar(int x, int y, int w, int h, int percent);
void pulseLEDs(unsigned long t, RgbColor base);
void showConnectingFrame();
void showConnected();
void showNameSelect();
void showWaiting();
void showEstimate();
void showHigherLower();
void showPotiTarget();
void showTempTarget();
void showVoting();
void showVoted();
void showReveal();
void showEnded();

// Network Module
bool resolveBrokerIP(IPAddress& brokerIP);
void mqttCallback(char* topic, byte* payload, unsigned int length);
bool mqttReconnect();
void publishConnect();
void connectMqttAsPlayer();
bool handleConnectionLoss();
void checkConnection();

#endif // GLOBALS_H
