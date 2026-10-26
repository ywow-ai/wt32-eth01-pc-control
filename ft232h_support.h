#pragma once

#include <Preferences.h>
#include <WebServer.h>
#include "ft232h_config.h"

struct DeviceConfig
{
  String themeMode;
  String ipMode;
  String ipAddr;
  String subnet;
  String gateway;
  String dns1;
  String dns2;
  String pusherChan;
  String wsHost;
  String wsPort;
  String wsPath;
  String pusherKey;
  String pusherCluster;
  bool discordEnabled;
  String discordUrl;
  String discordUsername;
  String discordEvents;
};

bool nvsLoadConfig(DeviceConfig &cfg);
bool nvsSaveTheme(const String &mode);
bool nvsSavePassword(const String &pass);
bool nvsSaveNetwork(const DeviceConfig &cfg);
bool nvsSavePusher(const DeviceConfig &cfg);
bool nvsSaveDiscord(const DeviceConfig &cfg);
bool nvsClearAll();

String nvsReadPassword();

bool validateTheme(const String &s);
bool validateIpMode(const String &s);
bool validateIpv4(const String &s);
bool validatePortString(const String &s, uint16_t &out);
bool validateChannelName(const String &s);
bool validateDiscordUrl(const String &s);
bool validateFieldLen(const String &s, size_t maxLen);

String jsonEscape(const String &s);
void sendJson(WebServer &server, int code, const String &body);

void optoInit();
void optoRequest(gpio_num_t pin, uint32_t ms);
void optoPoll();
bool optoBusy();

void ledPulseStart(uint8_t times);
void ledPulsePoll();
