#include "display.h"
#include "globals.h"
#include "audio.h"
#include "network.h"
#include "storage.h"

void displayShow() {
  aalec.display.display();
}

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

void showConnectingFrame(RgbColor ledColor) {
  // Zeitsteuerung: Überlauf-sicherer Vergleich (vgl. Vorlesung V06 Slide 18)
  // Die Differenzberechnung mit unsigned long verhindert Fehler beim Zähler-Rollover.
  unsigned long now = millis();
  if (now - _connLast < 120) return;
  _connLast = now;
  _connDots = (_connDots + 1) % 4;

  pulseLEDs(now, ledColor);

  aalec.display.clear();
  aalec.display.setFont(ArialMT_Plain_10);
  aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
  aalec.display.drawString(64, 0, "AALeC Quiz");
  aalec.display.drawLine(0, 13, 128, 13);
  aalec.display.setTextAlignment(TEXT_ALIGN_LEFT);
  String dotStr = "Verbinde";
  for (int i = 0; i < _connDots; i++) dotStr += ".";
  aalec.display.drawString(4, 18, dotStr);
  aalec.display.drawString(4, 33, ">" + String(apSSID));
  displayShow();
}

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
  aalec.button_changed(); // Sync/consume button state to avoid registering initial press as a character select click

  while (true) {
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

void showWaiting() {
  unsigned long lastUpdate = 0;
  unsigned long nameResetHoldStart = 0;
  bool waitForButtonReleaseAfterReset = false;
  unsigned long lastPressedMs = 0;

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
        lastPressedMs = now;
      } else {
        if (millis() - lastPressedMs > 150) {
          nameResetHoldStart = 0;
        }
      }
    }

    unsigned long now = millis();
    if (now - lastUpdate < 80) { delay(10); continue; }
    lastUpdate = now;

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

    displayShow();
    delay(20);
  }
}

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
    delay(20);
  }
}

void showVoting() {
  while (quizState == STATE_VOTING) {
    checkConnection();
    mqtt.loop();

    // ── Countdown-LEDs (5 = voll, 0 = Zeit abgelaufen) ──────────────
    {
      unsigned long elapsed = millis() - votingStartMs;
      unsigned long limitMs = (unsigned long)timeLimitS * 1000UL;
      if (elapsed > limitMs) elapsed = limitMs;
      int ledsOn = (int)(((limitMs - elapsed) * 5UL + limitMs - 1) / limitMs);

      for (int i = 0; i < 5; i++) {
        if (i < ledsOn) {
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

    // Display zeichnen
    aalec.display.clear();
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
    aalec.display.drawString(64, 0, "Frage");
    aalec.display.drawLine(0, 13, 128, 13);

    // 2x2 Grid
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

      aalec.display.setFont(ArialMT_Plain_16);
      aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
      aalec.display.drawString(gx[i] + gw / 2, gy[i] + 3, String(labels[i]));
      aalec.display.setColor(WHITE);
    }

    displayShow();
    delay(30);
  }
}

void showVoted() {
  unsigned long lastUpdate = 0;

  while (quizState == STATE_VOTED) {
    checkConnection();
    mqtt.loop();

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

    aalec.display.clear();
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);
    aalec.display.drawString(64, 0, "AALeC Quiz");
    aalec.display.drawLine(0, 13, 128, 13);

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
    displayShow();
  }
}

void showReveal() {
  bool correct = revealWasCorrect;

  for (int i = 0; i < 5; i++)
    setLED(i,correct ? c_green : c_red);

  if (correct) soundCorrect(); else soundWrong();

  while (quizState == STATE_REVEAL) {
    checkConnection();
    mqtt.loop();
    aalec.display.clear();
    aalec.display.setFont(ArialMT_Plain_10);
    aalec.display.setTextAlignment(TEXT_ALIGN_CENTER);

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
      int total = 0;
      for (int i = 0; i < 4; i++) total += answerCounts[i];
      if (total == 0) total = 1;

      const char labels[4] = {'A','B','C','D'};
      for (int i = 0; i < 4; i++) {
        int y   = 13 + i * 12;
        int pct = (answerCounts[i] * 100) / total;
        int bar = map(pct, 0, 100, 0, 90);

        aalec.display.setTextAlignment(TEXT_ALIGN_LEFT);
        aalec.display.drawString(0, y, String(labels[i]));

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

void showEnded() {
  unsigned long lastUpdate = 0;

  for (int i = 0; i < 5; i++) setLED(i,c_yellow);
  soundWinner();

  while (quizState == STATE_ENDED) {
    checkConnection();
    mqtt.loop();

    unsigned long now = millis();
    if (now - lastUpdate < 80) { delay(10); continue; }
    lastUpdate = now;

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

    if (aalec.button_changed() && aalec.get_button() == 1) {
      Serial.println("[INPUT] Neustart angefordert");
      JsonDocument ctrl;
      ctrl["action"] = "restart";
      char buf[64];
      serializeJson(ctrl, buf);
      mqtt.publish("quiz/control", buf);
      delay(500);
    }
  }

  for (int i = 0; i < 5; i++) setLED(i,c_off);
}
