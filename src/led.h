#pragma once
#include "globals.h"

void forceClearBuffer();
void setAllLEDs(uint8_t r, uint8_t g, uint8_t b);
void setLEDColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
void clearAllLEDs();
void ledBlink(LEDMode mode, uint8_t r, uint8_t g, uint8_t b);
void ledSolid(LEDMode mode, uint8_t r, uint8_t g, uint8_t b);
void showRainbow();
void updateLEDDisplay();
bool isScheduleActive();
