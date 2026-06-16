#include "network.h"
#include "globals.h"
#include "storage.h"
#include "audio.h"
#include "display.h"
#include <ArduinoJson.h>

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
      estimateValue = (estimateMin + estimateMax) / 2;
      const char* u = doc["unit"] | "";
      strncpy(estimateUnit, u, 15); estimateUnit[15] = '\0';
      Serial.printf("[QUESTION/estimate] min=%d max=%d unit=%s\n", estimateMin, estimateMax, estimateUnit);
    } else if (strcmp(typeStr, "higher_lower") == 0) {
      questionType = QTYPE_HIGHER_LOWER;
      hlReference  = doc["reference"] | 0;
      const char* u = doc["unit"] | "";
      strncpy(hlUnit, u, 15); hlUnit[15] = '\0';
      selectedAnswer = 0;
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
      answerCounts[0]  = correct;
      Serial.printf("[REVEAL/poti_target] correct=%d guess=%d delta=%d\n", correct, potiGuess, delta);

    } else if (strcmp(revType, "temp_target") == 0) {
      float correct = doc["correct"] | tempTarget;
      float delta   = fabsf(tempGuess - correct);
      revealWasCorrect = (delta <= tempTolerance);
      answerCounts[0]  = (int)round(correct);
      Serial.printf("[REVEAL/temp_target] correct=%.1f guess=%.1f delta=%.1f\n", correct, tempGuess, delta);

    } else if (strcmp(revType, "estimate") == 0) {
      int correct = doc["correct"] | 0;
      estimateCorrectValue = correct;
      int delta   = abs(estimateValue - correct);
      int rng     = max(estimateMax - estimateMin, 1);
      float relErr = (float)delta / rng;
      revealWasCorrect = (relErr <= 0.30f);
      answerCounts[0] = correct;
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

bool resolveBrokerIP(IPAddress& brokerIP) {
  const char* targets[2];
  uint8_t targetCount = 0;

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

bool mqttReconnect() {
  if (mqtt.connected()) return true;

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
  if (!mqtt.connected()) {
    Serial.println("[MQTT] publishConnect übersprungen: nicht mit Broker verbunden");
    return;
  }
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

static uint8_t _wifiFailCount = 0;
static const uint8_t WIFI_FAIL_THRESHOLD = 3;

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

void checkConnection() {
  if (isHosting) {
    if (!mqtt.connected()) {
      static unsigned long lastHostRetry = 0;
      if (millis() - lastHostRetry >= 5000) {
        lastHostRetry = millis();
        Serial.println("[MQTT] Host-Modus ohne MQTT — reconnect …");
        mqttReconnect();
      }
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

  if (!mqtt.connected()) {
    static unsigned long lastMqttRetry = 0;
    if (millis() - lastMqttRetry >= 5000) {
      lastMqttRetry = millis();
      Serial.println("[MQTT] Verbindung verloren — reconnect …");
      mqttReconnect();
    }
  }
}
