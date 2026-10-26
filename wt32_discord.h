#pragma once

#include <Arduino.h>

void discordWorkerInit();
bool discordEnqueue(const String &msg);
