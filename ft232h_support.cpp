#include "ft232h_support.h"
#include <ETH.h>

static Preferences g_prefs;

static struct
{
  bool active;
  gpio_num_t pin;
  uint32_t endMs;
} g_opto;

static struct
{
  bool running;
  uint8_t left;
  uint32_t nextToggleMs;
  bool ledOn;
} g_ledPulse;

static String readStr(const char *key, const char *defVal = "")
{
  return g_prefs.getString(key, defVal);
}

static bool writeStr(const char *key, const String &val)
{
  if (val.isEmpty())
  {
    if (g_prefs.isKey(key))
      return g_prefs.remove(key);
    return true;
  }
  return g_prefs.putString(key, val) > 0;
}

bool validateFieldLen(const String &s, size_t maxLen)
{
  return s.length() <= maxLen;
}

bool validateTheme(const String &s)
{
  return s == "system" || s == "light" || s == "dark";
}

bool validateIpMode(const String &s)
{
  return s == "dhcp" || s == "static";
}

bool validateIpv4(const String &s)
{
  if (s.isEmpty() || s.length() > 15)
    return false;
  int a, b, c, d;
  if (sscanf(s.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
    return false;
  if (a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255 || d < 0 || d > 255)
    return false;
  return true;
}

bool validatePortString(const String &s, uint16_t &out)
{
  if (s.isEmpty())
    return false;
  for (size_t i = 0; i < s.length(); i++)
  {
    if (!isDigit(s[i]))
      return false;
  }
  long v = s.toInt();
  if (v < 1 || v > 65535)
    return false;
  out = (uint16_t)v;
  return true;
}

bool validateChannelName(const String &s)
{
  if (s.isEmpty() || s.length() > 64)
    return false;
  for (size_t i = 0; i < s.length(); i++)
  {
    char c = s[i];
    if (isalnum(c) || c == '-' || c == '_')
      continue;
    return false;
  }
  return true;
}

bool validateDiscordUrl(const String &s)
{
  if (s.isEmpty() || s.length() > NVS_STR_URL)
    return false;
  return s.startsWith("https://discord.com/api/webhooks/") ||
         s.startsWith("https://discordapp.com/api/webhooks/");
}

String jsonEscape(const String &s)
{
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++)
  {
    char c = s[i];
    if (c == '\\' || c == '"')
    {
      o += '\\';
      o += c;
    }
    else if (c == '\n')
      o += "\\n";
    else if (c == '\r')
      o += "\\r";
    else
      o += c;
  }
  return o;
}

void sendJson(WebServer &server, int code, const String &body)
{
  server.send(code, "application/json", body);
}

bool nvsLoadConfig(DeviceConfig &cfg)
{
  if (!g_prefs.begin(NVS_NAMESPACE, true))
    return false;

  cfg.themeMode = readStr("theme_mode", "system");
  cfg.ipMode = readStr("ip_mode", "dhcp");
  cfg.ipAddr = readStr("ip_addr");
  cfg.subnet = readStr("subnet");
  cfg.gateway = readStr("gateway");
  cfg.dns1 = readStr("dns1");
  cfg.dns2 = readStr("dns2");
  cfg.pusherChan = readStr("pusher_chan");
  cfg.wsHost = readStr("ws_host");
  cfg.wsPort = readStr("ws_port");
  cfg.wsPath = readStr("ws_path");
  cfg.pusherKey = readStr("pusher_key");
  cfg.pusherCluster = readStr("pusher_cluster");
  cfg.discordEnabled = g_prefs.getBool("discord_en", false);
  cfg.discordUrl = readStr("discord_url");
  cfg.discordUsername = readStr("discord_user");
  cfg.discordEvents = readStr("discord_ev");

  g_prefs.end();
  return true;
}

String nvsReadPassword()
{
  if (!g_prefs.begin(NVS_NAMESPACE, true))
    return String(DEFAULT_WEB_PASS);
  String p = g_prefs.getString(NVS_KEY_WEB_PASS, DEFAULT_WEB_PASS);
  g_prefs.end();
  if (p.isEmpty())
    p = DEFAULT_WEB_PASS;
  return p;
}

bool nvsSavePassword(const String &pass)
{
  if (pass.length() < 8 || pass.length() > WEB_PASS_MAX_LEN)
    return false;
  if (!g_prefs.begin(NVS_NAMESPACE, false))
    return false;
  bool ok = g_prefs.putString(NVS_KEY_WEB_PASS, pass) > 0;
  g_prefs.end();
  return ok;
}

bool nvsSaveTheme(const String &mode)
{
  if (!validateTheme(mode))
    return false;
  if (!g_prefs.begin(NVS_NAMESPACE, false))
    return false;
  bool ok = writeStr("theme_mode", mode);
  g_prefs.end();
  return ok;
}

bool nvsSaveNetwork(const DeviceConfig &cfg)
{
  if (!validateIpMode(cfg.ipMode))
    return false;
  if (cfg.ipMode == "static")
  {
    if (!validateIpv4(cfg.ipAddr) || !validateIpv4(cfg.subnet) || !validateIpv4(cfg.gateway))
      return false;
    if (!cfg.dns1.isEmpty() && !validateIpv4(cfg.dns1))
      return false;
    if (!cfg.dns2.isEmpty() && !validateIpv4(cfg.dns2))
      return false;
  }
  if (!g_prefs.begin(NVS_NAMESPACE, false))
    return false;
  bool ok = writeStr("ip_mode", cfg.ipMode) &&
            writeStr("ip_addr", cfg.ipAddr) &&
            writeStr("subnet", cfg.subnet) &&
            writeStr("gateway", cfg.gateway) &&
            writeStr("dns1", cfg.dns1) &&
            writeStr("dns2", cfg.dns2);
  g_prefs.end();
  return ok;
}

bool nvsSavePusher(const DeviceConfig &cfg)
{
  if (!cfg.pusherChan.isEmpty() && !validateChannelName(cfg.pusherChan))
    return false;
  if (!validateFieldLen(cfg.wsHost, NVS_STR_MED) ||
      !validateFieldLen(cfg.wsPath, NVS_STR_MED) ||
      !validateFieldLen(cfg.pusherKey, NVS_STR_SHORT) ||
      !validateFieldLen(cfg.pusherCluster, 16))
    return false;
  uint16_t portTmp;
  if (!cfg.wsPort.isEmpty() && !validatePortString(cfg.wsPort, portTmp))
    return false;

  if (!g_prefs.begin(NVS_NAMESPACE, false))
    return false;
  bool ok = writeStr("pusher_chan", cfg.pusherChan) &&
            writeStr("ws_host", cfg.wsHost) &&
            writeStr("ws_port", cfg.wsPort) &&
            writeStr("ws_path", cfg.wsPath) &&
            writeStr("pusher_key", cfg.pusherKey) &&
            writeStr("pusher_cluster", cfg.pusherCluster);
  g_prefs.end();
  return ok;
}

bool nvsSaveDiscord(const DeviceConfig &cfg)
{
  if (cfg.discordEnabled && !validateDiscordUrl(cfg.discordUrl))
    return false;
  if (!validateFieldLen(cfg.discordUsername, NVS_STR_SHORT) ||
      !validateFieldLen(cfg.discordEvents, NVS_STR_MED))
    return false;

  if (!g_prefs.begin(NVS_NAMESPACE, false))
    return false;
  g_prefs.putBool("discord_en", cfg.discordEnabled);
  bool ok = writeStr("discord_url", cfg.discordUrl) &&
            writeStr("discord_user", cfg.discordUsername) &&
            writeStr("discord_ev", cfg.discordEvents);
  g_prefs.end();
  return ok;
}

bool nvsClearAll()
{
  if (!g_prefs.begin(NVS_NAMESPACE, false))
    return false;
  g_prefs.clear();
  g_prefs.end();
  return true;
}

void optoInit()
{
  pinMode(PIN_OPTO_POWER, OUTPUT);
  pinMode(PIN_OPTO_RESET, OUTPUT);
  digitalWrite(PIN_OPTO_POWER, LOW);
  digitalWrite(PIN_OPTO_RESET, LOW);
  g_opto.active = false;
}

void optoRequest(gpio_num_t pin, uint32_t ms)
{
  if (g_opto.active)
    return;
  g_opto.active = true;
  g_opto.pin = pin;
  digitalWrite(pin, HIGH);
  g_opto.endMs = millis() + ms;
}

void optoPoll()
{
  if (!g_opto.active)
    return;
  if ((int32_t)(millis() - g_opto.endMs) >= 0)
  {
    digitalWrite(g_opto.pin, LOW);
    g_opto.active = false;
  }
}

bool optoBusy()
{
  return g_opto.active;
}

void ledPulseStart(uint8_t times)
{
  g_ledPulse.running = true;
  g_ledPulse.left = times * 2;
  g_ledPulse.ledOn = false;
  g_ledPulse.nextToggleMs = millis();
}

void ledPulsePoll()
{
  if (!g_ledPulse.running)
    return;
  if ((int32_t)(millis() - g_ledPulse.nextToggleMs) < 0)
    return;
  g_ledPulse.nextToggleMs = millis() + 80;
  if (g_ledPulse.ledOn)
  {
    digitalWrite(PIN_LED_STATUS, LOW);
    g_ledPulse.ledOn = false;
  }
  else
  {
    digitalWrite(PIN_LED_STATUS, HIGH);
    g_ledPulse.ledOn = true;
  }
  if (g_ledPulse.left > 0)
    g_ledPulse.left--;
  if (g_ledPulse.left == 0)
  {
    g_ledPulse.running = false;
    if (ETH.linkUp())
      digitalWrite(PIN_LED_STATUS, HIGH);
    else
      digitalWrite(PIN_LED_STATUS, LOW);
  }
}
