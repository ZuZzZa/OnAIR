#pragma once
#include "globals.h"

void applyJsonToConfig(JsonDocument& doc, const char* defaultApiUrl = "");
void loadConfig();
void saveConfig();
