#include "wt32_support.h"
#include "wt32_net.h"
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

bool validateCpuMhz(uint32_t mhz)
{
  return mhz == 80 || mhz == 160 || mhz == 240;
}

bool validateLoopDelayMs(uint32_t ms)
{
  return ms <= LOOP_DELAY_MS_MAX;
}

void applyCpuMhz(uint32_t mhz)
{
  if (!validateCpuMhz(mhz))
    mhz = DEFAULT_CPU_MHZ;
  setCpuFrequencyMhz(mhz);
}

bool discordAllowsTarget(const String &eventsCsv, const String &target)
{
  if (eventsCsv.isEmpty())
    return true;
  String want = target;
  want.trim();
  want.toLowerCase();
  if (want.isEmpty())
    return false;

  String list = eventsCsv;
  list.toLowerCase();
  int start = 0;
  while (start <= (int)list.length())
  {
    int comma = list.indexOf(',', start);
    String token = comma >= 0 ? list.substring(start, comma) : list.substring(start);
    token.trim();
    if (token == want)
      return true;
    if (comma < 0)
      break;
    start = comma + 1;
  }
  return false;
}

bool validateTheme(const String &s)
{
  return s == "system" || s == "light" || s == "dark";
}

bool validateNetLink(const String &s)
{
  return s == "auto" || s == "ethernet" || s == "wifi";
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
  cfg.netLink = readStr("net_link", "auto");
  cfg.ipMode = readStr("ip_mode", "dhcp");
  cfg.wifiSsid = readStr("wifi_ssid");
  cfg.wifiPass = readStr("wifi_pass");
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
  cfg.cpuMhz = g_prefs.getUInt("cpu_mhz", DEFAULT_CPU_MHZ);
  cfg.loopDelayMs = g_prefs.getUInt("loop_delay", DEFAULT_LOOP_DELAY_MS);
  if (!validateCpuMhz(cfg.cpuMhz))
    cfg.cpuMhz = DEFAULT_CPU_MHZ;
  if (!validateLoopDelayMs(cfg.loopDelayMs))
    cfg.loopDelayMs = DEFAULT_LOOP_DELAY_MS;

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
  if (!validateNetLink(cfg.netLink.isEmpty() ? "auto" : cfg.netLink))
    return false;
  if (!validateIpMode(cfg.ipMode))
    return false;
  if (!validateFieldLen(cfg.wifiSsid, 32) || !validateFieldLen(cfg.wifiPass, 64))
    return false;
  if (cfg.netLink == "wifi" && cfg.wifiSsid.isEmpty())
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
  bool ok = writeStr("net_link", cfg.netLink.isEmpty() ? "auto" : cfg.netLink) &&
            writeStr("ip_mode", cfg.ipMode) &&
            writeStr("ip_addr", cfg.ipAddr) &&
            writeStr("subnet", cfg.subnet) &&
            writeStr("gateway", cfg.gateway) &&
            writeStr("dns1", cfg.dns1) &&
            writeStr("dns2", cfg.dns2) &&
            writeStr("wifi_ssid", cfg.wifiSsid) &&
            writeStr("wifi_pass", cfg.wifiPass);
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

bool nvsSavePerformance(const DeviceConfig &cfg)
{
  if (!validateCpuMhz(cfg.cpuMhz) || !validateLoopDelayMs(cfg.loopDelayMs))
    return false;
  if (!g_prefs.begin(NVS_NAMESPACE, false))
    return false;
  g_prefs.putUInt("cpu_mhz", cfg.cpuMhz);
  g_prefs.putUInt("loop_delay", cfg.loopDelayMs);
  g_prefs.end();
  return true;
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
    netUpdateLed();
  }
}

static void serialPrintKv(const char *key, const String &val, bool secret)
{
  Serial.print(key);
  Serial.print('=');
  if (secret && !val.isEmpty())
    Serial.println("***");
  else
    Serial.println(val.isEmpty() ? "()" : val);
}

bool configPrintSerialKey(const String &key, bool includeSecrets)
{
  String k = key;
  k.trim();
  k.toLowerCase();
  DeviceConfig c;
  nvsLoadConfig(c);

  if (k == "theme_mode")
    serialPrintKv("theme_mode", c.themeMode, false);
  else if (k == "net_link")
    serialPrintKv("net_link", c.netLink.isEmpty() ? "auto" : c.netLink, false);
  else if (k == "ip_mode")
    serialPrintKv("ip_mode", c.ipMode, false);
  else if (k == "ip_addr")
    serialPrintKv("ip_addr", c.ipAddr, false);
  else if (k == "subnet")
    serialPrintKv("subnet", c.subnet, false);
  else if (k == "gateway")
    serialPrintKv("gateway", c.gateway, false);
  else if (k == "dns1")
    serialPrintKv("dns1", c.dns1, false);
  else if (k == "dns2")
    serialPrintKv("dns2", c.dns2, false);
  else if (k == "wifi_ssid")
    serialPrintKv("wifi_ssid", c.wifiSsid, false);
  else if (k == "wifi_pass")
    serialPrintKv("wifi_pass", c.wifiPass, !includeSecrets);
  else if (k == "pusher_chan")
    serialPrintKv("pusher_chan", c.pusherChan, false);
  else if (k == "ws_host")
    serialPrintKv("ws_host", c.wsHost, false);
  else if (k == "ws_port")
    serialPrintKv("ws_port", c.wsPort, false);
  else if (k == "ws_path")
    serialPrintKv("ws_path", c.wsPath, false);
  else if (k == "pusher_key")
    serialPrintKv("pusher_key", c.pusherKey, false);
  else if (k == "pusher_cluster")
    serialPrintKv("pusher_cluster", c.pusherCluster, false);
  else if (k == "discord_en")
    Serial.printf("discord_en=%s\n", c.discordEnabled ? "true" : "false");
  else if (k == "discord_url")
    serialPrintKv("discord_url", c.discordUrl, false);
  else if (k == "discord_user")
    serialPrintKv("discord_user", c.discordUsername, false);
  else if (k == "discord_ev")
    serialPrintKv("discord_ev", c.discordEvents, false);
  else if (k == "cpu_mhz")
    Serial.printf("cpu_mhz=%u\n", c.cpuMhz);
  else if (k == "loop_delay_ms")
    Serial.printf("loop_delay_ms=%u\n", c.loopDelayMs);
  else if (k == "web_pass" || k == "password")
    serialPrintKv("web_pass", nvsReadPassword(), !includeSecrets);
  else
    return false;
  return true;
}

void configPrintSerial(bool includeSecrets)
{
  DeviceConfig c;
  nvsLoadConfig(c);
  Serial.println("[NVS] config:");
  serialPrintKv("theme_mode", c.themeMode, false);
  serialPrintKv("net_link", c.netLink.isEmpty() ? "auto" : c.netLink, false);
  serialPrintKv("ip_mode", c.ipMode, false);
  serialPrintKv("ip_addr", c.ipAddr, false);
  serialPrintKv("subnet", c.subnet, false);
  serialPrintKv("gateway", c.gateway, false);
  serialPrintKv("dns1", c.dns1, false);
  serialPrintKv("dns2", c.dns2, false);
  serialPrintKv("wifi_ssid", c.wifiSsid, false);
  serialPrintKv("wifi_pass", c.wifiPass, !includeSecrets);
  serialPrintKv("pusher_chan", c.pusherChan, false);
  serialPrintKv("ws_host", c.wsHost, false);
  serialPrintKv("ws_port", c.wsPort, false);
  serialPrintKv("ws_path", c.wsPath, false);
  serialPrintKv("pusher_key", c.pusherKey, false);
  serialPrintKv("pusher_cluster", c.pusherCluster, false);
  Serial.printf("discord_en=%s\n", c.discordEnabled ? "true" : "false");
  serialPrintKv("discord_url", c.discordUrl, false);
  serialPrintKv("discord_user", c.discordUsername, false);
  serialPrintKv("discord_ev", c.discordEvents, false);
  Serial.printf("cpu_mhz=%u\n", c.cpuMhz);
  Serial.printf("loop_delay_ms=%u\n", c.loopDelayMs);
  serialPrintKv("web_pass", nvsReadPassword(), !includeSecrets);
}

bool configSetFromSerial(const String &key, const String &value, String &errOut)
{
  errOut = "";
  String k = key;
  k.trim();
  k.toLowerCase();
  String v = value;
  v.trim();

  if (k == "web_pass" || k == "password")
  {
    if (!nvsSavePassword(v))
    {
      errOut = "password: 8-64 chars";
      return false;
    }
    return true;
  }

  DeviceConfig c;
  nvsLoadConfig(c);

  if (k == "theme_mode")
  {
    if (!nvsSaveTheme(v))
    {
      errOut = "theme: system|light|dark";
      return false;
    }
    return true;
  }
  if (k == "net_link")
    c.netLink = v;
  else if (k == "ip_mode")
    c.ipMode = v;
  else if (k == "ip_addr")
    c.ipAddr = v;
  else if (k == "subnet")
    c.subnet = v;
  else if (k == "gateway")
    c.gateway = v;
  else if (k == "dns1")
    c.dns1 = v;
  else if (k == "dns2")
    c.dns2 = v;
  else if (k == "wifi_ssid")
    c.wifiSsid = v;
  else if (k == "wifi_pass")
    c.wifiPass = v;
  else if (k == "pusher_chan")
    c.pusherChan = v;
  else if (k == "ws_host")
    c.wsHost = v;
  else if (k == "ws_port")
    c.wsPort = v;
  else if (k == "ws_path")
    c.wsPath = v;
  else if (k == "pusher_key")
    c.pusherKey = v;
  else if (k == "pusher_cluster")
    c.pusherCluster = v;
  else if (k == "discord_en")
    c.discordEnabled = v == "1" || v.equalsIgnoreCase("true") || v.equalsIgnoreCase("yes");
  else if (k == "discord_url")
    c.discordUrl = v;
  else if (k == "discord_user")
    c.discordUsername = v;
  else if (k == "discord_ev")
    c.discordEvents = v;
  else if (k == "cpu_mhz")
  {
    uint32_t mhz = (uint32_t)v.toInt();
    if (!validateCpuMhz(mhz))
    {
      errOut = "cpu_mhz: 80|160|240";
      return false;
    }
    c.cpuMhz = mhz;
    if (!nvsSavePerformance(c))
    {
      errOut = "NVS save failed";
      return false;
    }
    applyCpuMhz(c.cpuMhz);
    return true;
  }
  else if (k == "loop_delay_ms")
  {
    uint32_t ms = (uint32_t)v.toInt();
    if (!validateLoopDelayMs(ms))
    {
      errOut = "loop_delay_ms: 0-100";
      return false;
    }
    c.loopDelayMs = ms;
    if (!nvsSavePerformance(c))
    {
      errOut = "NVS save failed";
      return false;
    }
    return true;
  }
  else
  {
    errOut = "unknown key (help)";
    return false;
  }

  if (k == "pusher_chan" || k == "ws_host" || k == "ws_port" || k == "ws_path" || k == "pusher_key" ||
      k == "pusher_cluster")
  {
    if (!nvsSavePusher(c))
    {
      errOut = "invalid pusher config";
      return false;
    }
    return true;
  }
  if (k == "discord_en" || k == "discord_url" || k == "discord_user" || k == "discord_ev")
  {
    if (!nvsSaveDiscord(c))
    {
      errOut = "invalid discord config";
      return false;
    }
    return true;
  }

  if (!nvsSaveNetwork(c))
  {
    errOut = "invalid network config";
    return false;
  }
  return true;
}
