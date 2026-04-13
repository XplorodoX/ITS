/*
 * ALLEC_QUIZZ - Multiplayer Quiz for AALeC V3
 *
 * Hardware: AALeC V3 (Wemos D1 Mini / ESP8266)
 *
 * GPIO layout (AALeC V3):
 *   GPIO 0  – Rotary encoder push button
 *   GPIO 2  – WS2812B LED chain (5 LEDs)
 *   GPIO 4  – SDA (I2C → OLED, BME sensor)
 *   GPIO 5  – SCL (I2C → OLED, BME sensor)
 *   GPIO 12 – Rotary encoder channel A
 *   GPIO 14 – Rotary encoder channel B
 *   GPIO 15 – Speaker (PWM tone)
 *
 * MQTT topics:
 *   aalec/quiz/question   – incoming question payload (JSON)
 *   aalec/quiz/answer     – outgoing answer from this device
 *   aalec/quiz/result     – incoming result for this device
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <SSD1306Wire.h>

// ─── WiFi / MQTT credentials ──────────────────────────────────────────────────
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_SSID"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "YOUR_PASSWORD"
#endif
#ifndef MQTT_HOST
#define MQTT_HOST "192.168.1.100"
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

// ─── Pin definitions ──────────────────────────────────────────────────────────
#define PIN_ENC_BTN   0
#define PIN_LED_DATA  2
#define PIN_ENC_A     12
#define PIN_ENC_B     14
#define PIN_SPEAKER   15

#define NUM_LEDS      5
#define OLED_ADDR     0x3C

// ─── MQTT topics ──────────────────────────────────────────────────────────────
#define TOPIC_QUESTION  "aalec/quiz/question"
#define TOPIC_ANSWER    "aalec/quiz/answer"
#define TOPIC_RESULT    "aalec/quiz/result"

// ─── Objects ─────────────────────────────────────────────────────────────────
Adafruit_NeoPixel leds(NUM_LEDS, PIN_LED_DATA, NEO_GRB + NEO_KHZ800);
SSD1306Wire       display(OLED_ADDR, SDA, SCL);  // GPIO4=SDA, GPIO5=SCL
WiFiClient        wifiClient;
PubSubClient      mqtt(wifiClient);

// ─── Quiz state ───────────────────────────────────────────────────────────────
enum class QuizState { IDLE, QUESTION, ANSWERED, RESULT };
QuizState state = QuizState::IDLE;

String questionText = "";
String answers[4]   = {"", "", "", ""};
int    numAnswers    = 0;
int    selectedIdx   = 0;
bool   answered      = false;

// Rotary encoder state
volatile int  encPos      = 0;
volatile bool btnPressed  = false;
int  lastEncPos  = 0;
bool lastBtnState = HIGH;

// ─── Tone helper ──────────────────────────────────────────────────────────────
void playTone(unsigned int freq, unsigned int durationMs) {
    tone(PIN_SPEAKER, freq, durationMs);
    delay(durationMs + 10);
    noTone(PIN_SPEAKER);
}

void beepCorrect() {
    playTone(880, 150);
    playTone(1100, 200);
}

void beepWrong() {
    playTone(200, 300);
}

void beepSelect() {
    playTone(660, 60);
}

// ─── LED helpers ──────────────────────────────────────────────────────────────
void ledsOff() {
    leds.clear();
    leds.show();
}

void ledsColor(uint32_t color) {
    for (int i = 0; i < NUM_LEDS; i++) leds.setPixelColor(i, color);
    leds.show();
}

void ledsCorrect()  { ledsColor(leds.Color(0, 200, 0)); }
void ledsWrong()    { ledsColor(leds.Color(200, 0, 0)); }
void ledsIdle()     { ledsColor(leds.Color(0, 0, 40));  }

// Highlight current answer selection: one green LED per answer option
void ledsSelection(int idx, int total) {
    leds.clear();
    if (total > 0) {
        int ledIdx = map(idx, 0, total - 1, 0, NUM_LEDS - 1);
        leds.setPixelColor(ledIdx, leds.Color(0, 100, 200));
    }
    leds.show();
}

// ─── OLED helpers ─────────────────────────────────────────────────────────────
void drawIdle() {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 20, "AALeC Quiz");
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 44, "Waiting for question...");
    display.display();
}

void drawQuestion() {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);

    // Wrap question text across up to 3 lines
    display.drawStringMaxWidth(0, 0, 128, questionText);

    // Draw answer options
    for (int i = 0; i < numAnswers && i < 4; i++) {
        String prefix = (i == selectedIdx) ? "> " : "  ";
        display.drawString(0, 32 + i * 12, prefix + answers[i]);
    }
    display.display();
}

void drawResult(bool correct) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_24);
    display.drawString(64, 16, correct ? "RICHTIG!" : "FALSCH!");
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 48, correct ? ":)" : ":(");
    display.display();
}

// ─── JSON helpers (manual, no extra lib) ─────────────────────────────────────
// Extract a simple string value for a key from a flat JSON object.
String jsonGet(const String& json, const String& key) {
    String search = "\"" + key + "\":\"";
    int start = json.indexOf(search);
    if (start < 0) return "";
    start += search.length();
    int end = json.indexOf("\"", start);
    if (end < 0) return "";
    return json.substring(start, end);
}

// ─── MQTT callback ────────────────────────────────────────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

    if (String(topic) == TOPIC_QUESTION) {
        // Expected JSON: {"q":"Question?","a0":"Opt A","a1":"Opt B","a2":"Opt C","a3":"Opt D"}
        questionText = jsonGet(msg, "q");
        numAnswers   = 0;
        for (int i = 0; i < 4; i++) {
            String a = jsonGet(msg, "a" + String(i));
            if (a.length() > 0) answers[numAnswers++] = a;
        }
        selectedIdx = 0;
        answered    = false;
        state       = QuizState::QUESTION;
        beepSelect();
        ledsIdle();
    }

    if (String(topic) == TOPIC_RESULT) {
        // Expected JSON: {"correct":"true"} or {"correct":"false"}
        bool correct = (jsonGet(msg, "correct") == "true");
        state = QuizState::RESULT;
        drawResult(correct);
        if (correct) {
            beepCorrect();
            ledsCorrect();
        } else {
            beepWrong();
            ledsWrong();
        }
        delay(3000);
        state = QuizState::IDLE;
        ledsIdle();
        drawIdle();
    }
}

// ─── WiFi / MQTT setup ───────────────────────────────────────────────────────
void connectWiFi() {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 20, "Connecting WiFi...");
    display.drawString(64, 34, WIFI_SSID);
    display.display();

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 40) {
        delay(500);
        tries++;
    }

    display.clear();
    if (WiFi.status() == WL_CONNECTED) {
        display.drawString(64, 20, "WiFi OK");
        display.drawString(64, 34, WiFi.localIP().toString());
    } else {
        display.drawString(64, 20, "WiFi FAILED");
        display.drawString(64, 34, "Offline mode");
    }
    display.display();
    delay(1500);
}

void connectMQTT() {
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(mqttCallback);

    String clientId = "aalec-" + String(ESP.getChipId(), HEX);
    if (mqtt.connect(clientId.c_str())) {
        mqtt.subscribe(TOPIC_QUESTION);
        mqtt.subscribe(TOPIC_RESULT);
    }
}

// ─── Rotary encoder ISRs ─────────────────────────────────────────────────────
IRAM_ATTR void encoderISR() {
    bool a = digitalRead(PIN_ENC_A);
    bool b = digitalRead(PIN_ENC_B);
    if (a != b) encPos++;
    else        encPos--;
}

// ─── Arduino setup ────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // LEDs
    leds.begin();
    ledsColor(leds.Color(20, 20, 0));  // yellow = booting

    // Display
    display.init();
    display.flipScreenVertically();
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 24, "AALeC Quiz");
    display.display();
    delay(800);

    // Encoder + button
    pinMode(PIN_ENC_A,   INPUT_PULLUP);
    pinMode(PIN_ENC_B,   INPUT_PULLUP);
    pinMode(PIN_ENC_BTN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encoderISR, CHANGE);

    // Speaker
    pinMode(PIN_SPEAKER, OUTPUT);

    // Network
    connectWiFi();
    if (WiFi.status() == WL_CONNECTED) connectMQTT();

    ledsIdle();
    drawIdle();
    playTone(440, 100);
}

// ─── Arduino loop ─────────────────────────────────────────────────────────────
void loop() {
    // Keep MQTT alive
    if (WiFi.status() == WL_CONNECTED) {
        if (!mqtt.connected()) connectMQTT();
        mqtt.loop();
    }

    if (state == QuizState::QUESTION && !answered) {
        // ── Rotary encoder navigation ──────────────────────────────────────
        int delta = encPos - lastEncPos;
        if (delta != 0) {
            lastEncPos = encPos;
            selectedIdx = (selectedIdx + delta + numAnswers) % numAnswers;
            beepSelect();
            ledsSelection(selectedIdx, numAnswers);
            drawQuestion();
        }

        // ── Button: confirm answer ─────────────────────────────────────────
        bool btnNow = digitalRead(PIN_ENC_BTN);
        if (btnNow == LOW && lastBtnState == HIGH) {
            // Debounce
            delay(50);
            if (digitalRead(PIN_ENC_BTN) == LOW) {
                answered = true;
                state    = QuizState::ANSWERED;

                // Build answer payload: {"a":"<selected answer text>","i":<index>}
                String payload = "{\"a\":\"" + answers[selectedIdx]
                                 + "\",\"i\":" + String(selectedIdx) + "}";
                mqtt.publish(TOPIC_ANSWER, payload.c_str());

                // Visual feedback while waiting for result
                ledsColor(leds.Color(100, 100, 0));
                display.clear();
                display.setTextAlignment(TEXT_ALIGN_CENTER);
                display.setFont(ArialMT_Plain_16);
                display.drawString(64, 20, "Antwort:");
                display.setFont(ArialMT_Plain_10);
                display.drawString(64, 42, answers[selectedIdx]);
                display.display();
                playTone(550, 80);
            }
        }
        lastBtnState = btnNow;
    }
}
