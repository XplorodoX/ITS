#include <Arduino.h>
#include <WiFiS3.h>
#include <Arduino_LED_Matrix.h>

const char AP_SSID[] = "AALeC-Quiz";
const char AP_PASS[] = "12345678";

ArduinoLEDMatrix matrix;

const unsigned long AP_RETRY_DELAY_MS = 2500UL;
const unsigned long AP_HEALTH_CHECK_INTERVAL_MS = 2000UL;
const unsigned long DISPLAY_FRAME_MS = 120UL;
const unsigned long MATRIX_SELFTEST_MS = 900UL;

unsigned long lastApHealthCheckMs = 0;
unsigned long lastDisplayFrameMs = 0;
unsigned long stateSinceMs = 0;
uint8_t lastLoggedWiFiStatus = 255;

enum DisplayState : uint8_t {
  DISPLAY_BOOT,
  DISPLAY_STARTING,
  DISPLAY_OK,
  DISPLAY_CLIENT,
  DISPLAY_RECOVER,
  DISPLAY_ERROR
};

DisplayState displayState = DISPLAY_BOOT;

void clearFrame(uint8_t frame[8][12]) {
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 12; ++x) {
      frame[y][x] = 0;
    }
  }
}

void setPixel(uint8_t frame[8][12], uint8_t x, uint8_t y) {
  if (x < 12 && y < 8) {
    frame[y][x] = 1;
  }
}

void drawMask(uint8_t frame[8][12], const uint16_t mask[8]) {
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 12; ++x) {
      if (mask[y] & (1U << (11 - x))) {
        setPixel(frame, x, y);
      }
    }
  }
}

void drawThickX(uint8_t frame[8][12]) {
  for (uint8_t x = 0; x < 12; ++x) {
    int yMain = (x * 7) / 11;
    int yAnti = 7 - yMain;

    for (int o = -1; o <= 1; ++o) {
      int y1 = yMain + o;
      int y2 = yAnti + o;
      if (y1 >= 0 && y1 < 8) {
        setPixel(frame, x, (uint8_t)y1);
      }
      if (y2 >= 0 && y2 < 8) {
        setPixel(frame, x, (uint8_t)y2);
      }
    }
  }
}

void drawSweepBars(uint8_t frame[8][12], uint8_t phase) {
  uint8_t start = (uint8_t)((phase * 2) % 12);
  for (uint8_t x = 0; x < 3; ++x) {
    uint8_t col = (uint8_t)((start + x) % 12);
    for (uint8_t y = 0; y < 8; ++y) {
      setPixel(frame, col, y);
    }
  }
}

void drawClientPulse(uint8_t frame[8][12], uint8_t phase) {
  uint8_t p = phase % 4;
  setPixel(frame, 9, 1);
  if (p >= 1) {
    setPixel(frame, 8, 2);
    setPixel(frame, 10, 2);
  }
  if (p >= 2) {
    setPixel(frame, 7, 3);
    setPixel(frame, 11, 3);
  }
  if (p >= 3) {
    setPixel(frame, 6, 4);
    setPixel(frame, 11, 4);
  }
}

void runMatrixSelfTest() {
  uint8_t frame[8][12];

  unsigned long start = millis();
  while (millis() - start < MATRIX_SELFTEST_MS) {
    clearFrame(frame);
    uint8_t phase = (uint8_t)(((millis() - start) / 120UL) % 4);

    if (phase == 0) {
      for (uint8_t y = 0; y < 8; ++y) {
        for (uint8_t x = 0; x < 12; ++x) {
          setPixel(frame, x, y);
        }
      }
    } else if (phase == 1) {
      for (uint8_t y = 0; y < 8; ++y) {
        if ((y % 2) == 0) {
          for (uint8_t x = 0; x < 12; ++x) {
            setPixel(frame, x, y);
          }
        }
      }
    } else if (phase == 2) {
      for (uint8_t x = 0; x < 12; ++x) {
        if ((x % 2) == 0) {
          for (uint8_t y = 0; y < 8; ++y) {
            setPixel(frame, x, y);
          }
        }
      }
    } else {
      drawThickX(frame);
    }

    matrix.renderBitmap(frame, 8, 12);
    delay(40);
  }
}

void setDisplayState(DisplayState next) {
  if (displayState != next) {
    displayState = next;
    stateSinceMs = millis();
  }
}

void renderDisplay() {
  uint8_t frame[8][12];
  clearFrame(frame);

  unsigned long now = millis();
  uint8_t phase = (uint8_t)((now / DISPLAY_FRAME_MS) % 8);

  static const uint16_t SMILE_MASK[8] = {
    0b000111111000,
    0b001000000100,
    0b010010010010,
    0b010000000010,
    0b010000000010,
    0b010100001010,
    0b001011110100,
    0b000000000000,
  };

  static const uint16_t WAIT_MASK[8] = {
    0b000111111000,
    0b001000000100,
    0b010000000010,
    0b010000000010,
    0b010001100010,
    0b010001100010,
    0b001000000100,
    0b000111111000,
  };

  if (displayState == DISPLAY_BOOT) {
    drawSweepBars(frame, phase);
  } else if (displayState == DISPLAY_STARTING) {
    drawMask(frame, WAIT_MASK);
    drawSweepBars(frame, phase);
  } else if (displayState == DISPLAY_OK) {
    drawMask(frame, SMILE_MASK);
    if (((now / 500UL) % 2) == 0) {
      setPixel(frame, 1, 1);
      setPixel(frame, 10, 1);
    }
  } else if (displayState == DISPLAY_CLIENT) {
    drawMask(frame, SMILE_MASK);
    drawClientPulse(frame, phase);
  } else if (displayState == DISPLAY_RECOVER) {
    drawMask(frame, WAIT_MASK);
    drawThickX(frame);
  } else if (displayState == DISPLAY_ERROR) {
    if (((now / 200UL) % 2) == 0) {
      drawThickX(frame);
    } else {
      for (uint8_t x = 0; x < 12; ++x) {
        setPixel(frame, x, 0);
        setPixel(frame, x, 7);
      }
    }
  }

  matrix.renderBitmap(frame, 8, 12);
}

bool startAccessPoint() {
  WiFi.end();
  delay(200);

  int status = WiFi.beginAP(AP_SSID, AP_PASS);
  if (status != WL_AP_LISTENING) {
    Serial.print("Failed to start AP, status=");
    Serial.println(status);
    setDisplayState(DISPLAY_ERROR);
    return false;
  }

  IPAddress ip = WiFi.localIP();
  Serial.print("AP started. SSID: ");
  Serial.println(AP_SSID);
  Serial.print("IP address: ");
  Serial.println(ip);

  setDisplayState(DISPLAY_OK);
  return true;
}

void syncDisplayToWiFiStatus() {
  int status = WiFi.status();

  if (status != lastLoggedWiFiStatus) {
    lastLoggedWiFiStatus = (uint8_t)status;
    Serial.print("WiFi status changed: ");
    Serial.println(status);
  }

  if (status == WL_AP_CONNECTED) {
    setDisplayState(DISPLAY_CLIENT);
    return;
  }

  if (status == WL_AP_LISTENING) {
    setDisplayState(DISPLAY_OK);
  }
}

void ensureAccessPointRunning() {
  int status = WiFi.status();
  if (status == WL_AP_LISTENING || status == WL_AP_CONNECTED) {
    return;
  }

  Serial.print("AP not active, status=");
  Serial.print(status);
  Serial.println(". Restarting AP...");

  setDisplayState(DISPLAY_RECOVER);
  if (!startAccessPoint()) {
    setDisplayState(DISPLAY_ERROR);
  }
}

void setup() {
  matrix.begin();
  runMatrixSelfTest();
  setDisplayState(DISPLAY_BOOT);
  renderDisplay();

  Serial.begin(9600);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("WiFi module not detected");
    setDisplayState(DISPLAY_ERROR);
    while (true) {
      renderDisplay();
      delay(DISPLAY_FRAME_MS);
    }
  }

  String fw = WiFi.firmwareVersion();
  Serial.print("WiFi firmware: ");
  Serial.println(fw);

  uint8_t attempts = 0;
  setDisplayState(DISPLAY_STARTING);

  while (!startAccessPoint()) {
    attempts++;
    if (attempts >= 10) {
      Serial.println("AP start failed repeatedly. Rebooting in 2s...");
      setDisplayState(DISPLAY_ERROR);
      unsigned long rebootStart = millis();
      while (millis() - rebootStart < 2000UL) {
        renderDisplay();
        delay(DISPLAY_FRAME_MS);
      }
      NVIC_SystemReset();
    }

    delay(AP_RETRY_DELAY_MS);
    setDisplayState(DISPLAY_STARTING);
  }
}

void loop() {
  if (millis() - lastApHealthCheckMs >= AP_HEALTH_CHECK_INTERVAL_MS) {
    ensureAccessPointRunning();
    syncDisplayToWiFiStatus();
    lastApHealthCheckMs = millis();
  }

  if (millis() - lastDisplayFrameMs >= DISPLAY_FRAME_MS) {
    renderDisplay();
    lastDisplayFrameMs = millis();
  }
}