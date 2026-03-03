#pragma once
#include "globals.h"

void enterErrorPause();
void incrementApiErrorCount();
void fetchAndUpdateLEDs();
void parseTeamsResponse(String jsonResponse);
