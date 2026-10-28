#pragma once

#include <Preferences.h>
#include <WebServer.h>
#include "wt32_config.h"

struct DeviceConfig
{
  String themeMode;
  String netLink;
  String ipMode;
  String ipAddr;
  String subnet;
  String gateway;
  String dns1;
  String dns2;
  String wifiSsid;
  String wifiPass;
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
  uint32_t cpuMhz;
  uint32_t loopDelayMs;
};

bool nvsLoadConfig(DeviceConfig &cfg);
bool nvsSaveTheme(const String &mode);
bool nvsSavePassword(const String &pass);
bool nvsSaveNetwork(const DeviceConfig &cfg);
bool nvsSavePusher(const DeviceConfig &cfg);
bool nvsSaveDiscord(const DeviceConfig &cfg);
bool nvsSavePerformance(const DeviceConfig &cfg);
bool nvsClearAll();

String nvsReadPassword();

bool validateTheme(const String &s);
bool validateNetLink(const String &s);
bool validateIpMode(const String &s);
bool configSetFromSerial(const String &key, const String &value, String &errOut);
void configPrintSerial(bool includeSecrets);
bool configPrintSerialKey(const String &key, bool includeSecrets);
bool validateIpv4(const String &s);
bool validatePortString(const String &s, uint16_t &out);
bool validateChannelName(const String &s);
bool validateDiscordUrl(const String &s);
bool validateFieldLen(const String &s, size_t maxLen);
bool validateCpuMhz(uint32_t mhz);
bool validateLoopDelayMs(uint32_t ms);

void applyCpuMhz(uint32_t mhz);

bool discordAllowsTarget(const String &eventsCsv, const String &target);

String jsonEscape(const String &s);
void sendJson(WebServer &server, int code, const String &body);

void optoInit();
void optoRequest(gpio_num_t pin, uint32_t ms);
void optoPoll();
bool optoBusy();

void ledPulseStart(uint8_t times);
void ledPulsePoll();
