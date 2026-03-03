#include "led.h"

void forceClearBuffer() {
  noInterrupts();
  for (int attempt = 0; attempt < 5; attempt++) {
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, 0);
    }
    strip.show();
  }
  interrupts();
  delay(5);
}

void setAllLEDs(uint8_t r, uint8_t g, uint8_t b) {
  noInterrupts();
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
  interrupts();
}

void setLEDColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
  if (index < strip.numPixels()) {
    strip.setPixelColor(index, strip.Color(r, g, b));
  }
}

void clearAllLEDs() {
  currentLEDMode = LED_OFF;
  if (lastLEDMode != currentLEDMode) {
    forceClearBuffer();
    lastLEDMode = currentLEDMode;
  }
}

void ledBlink(LEDMode mode, uint8_t r, uint8_t g, uint8_t b) {
  currentLEDMode = mode;

  const unsigned long blinkInterval = 250;
  static unsigned long lastToggle  = 0;
  static bool          isOn        = false;
  static bool          initialized = false;

  if (lastLEDMode != currentLEDMode) {
    initialized = false;
    lastLEDMode = currentLEDMode;
  }

  if (!initialized) {
    forceClearBuffer();
    initialized = true;
    lastToggle  = millis();
    isOn        = false;
  }

  if (millis() - lastToggle >= blinkInterval) {
    lastToggle = millis();
    isOn       = !isOn;

    noInterrupts();
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, isOn ? strip.Color(r, g, b) : 0);
    }
    strip.show();
    interrupts();
  }
}

void ledSolid(LEDMode mode, uint8_t r, uint8_t g, uint8_t b) {
  currentLEDMode = mode;

  const unsigned long refreshInterval = 1000;
  static unsigned long lastRefresh  = 0;
  static bool          initialized  = false;

  if (lastLEDMode != currentLEDMode) {
    initialized = false;
    lastLEDMode = currentLEDMode;
  }

  if (!initialized || (millis() - lastRefresh >= refreshInterval)) {
    if (!initialized) forceClearBuffer();
    initialized = true;
    lastRefresh = millis();

    noInterrupts();
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
    interrupts();
  }
}

void showRainbow() {
  currentLEDMode = LED_RAINBOW;

  const unsigned long rainbowSpeed = 80;
  static unsigned long lastToggle  = 0;
  static int           rainbowPos  = 0;
  static bool          initialized = false;

  if (lastLEDMode != currentLEDMode) {
    initialized = false;
    lastLEDMode = currentLEDMode;
  }

  if (!initialized) {
    forceClearBuffer();
    initialized = true;
    lastToggle  = millis();
    rainbowPos  = 0;
  }

  const int rainbowColors[7][3] = {
    {255,   0,   0},
    {255, 127,   0},
    {255, 255,   0},
    {  0, 255,   0},
    {  0, 127, 255},
    {  0,   0, 255},
    {127,   0, 255}
  };

  if (millis() - lastToggle >= rainbowSpeed) {
    lastToggle = millis();
    if (++rainbowPos >= NUM_LEDS * 2) rainbowPos = 0;
  }

  noInterrupts();
  for (int i = 0; i < NUM_LEDS; i++) {
    int colorIndex = (i + rainbowPos) % 7;
    strip.setPixelColor(i, strip.Color(
      rainbowColors[colorIndex][0],
      rainbowColors[colorIndex][1],
      rainbowColors[colorIndex][2]
    ));
  }
  strip.show();
  interrupts();
}

void updateLEDDisplay() {
  if (!isScheduleActive()) {
    clearAllLEDs();
    return;
  }

  if (isInErrorState) {
    showRainbow();
    return;
  }

  if (isApiTransientError) {
    ledBlink(LED_BLUE_BLINK, 0, 0, 255);
    return;
  }

  if (callState == CALL_INACTIVE) {
    clearAllLEDs();
  } else {
    if (muteState == MIC_LIVE) {
      ledBlink(LED_RED_BLINK, 255, 0, 0);  // on call, mic live → fast red blink
    } else {
      ledSolid(LED_RED_SOLID, 255, 0, 0);  // on call, muted   → steady red
    }
  }
}

bool isScheduleActive() {
  if (!schedule.enabled) return true;

  time_t    now      = time(nullptr);
  struct tm *timeinfo = localtime(&now);

  int dayOfWeek   = timeinfo->tm_wday;
  int scheduleDay = (dayOfWeek == 0) ? 6 : (dayOfWeek - 1);

  if (!schedule.days[scheduleDay]) return false;

  int currentHour = timeinfo->tm_hour;

  if (schedule.startHour <= schedule.endHour) {
    return (currentHour >= schedule.startHour && currentHour < schedule.endHour);
  } else {
    return (currentHour >= schedule.startHour || currentHour < schedule.endHour);
  }
}
