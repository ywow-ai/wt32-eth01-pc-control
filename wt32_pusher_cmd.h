#pragma once

#include <Arduino.h>

bool extractPusherCommand(const char *data, size_t length, String &outCmd);
