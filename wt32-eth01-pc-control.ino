#include <ETH.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WebServer.h>
#include "PusherClient.h"
#include "wt32_config.h"
#include "wt32_discord.h"
#include "wt32_net.h"
#include "wt32_pusher_cmd.h"
#include "wt32_serial_cmd.h"
#include "wt32_support.h"

WebServer server(80);
PusherClient pusher;

DeviceConfig g_cfg;
static bool g_fsOk = false;
static String g_sessionId;
static uint32_t g_factoryHoldStart = 0;
static bool g_factoryTriggered = false;
static bool g_factorySeenRelease = false;
static bool g_pusherStarted = false;
static uint32_t g_loopDelayMs = DEFAULT_LOOP_DELAY_MS;

static bool sessionValid()
{
  if (g_sessionId.isEmpty() || !server.hasHeader("Cookie"))
    return false;
  return server.header("Cookie").indexOf("sid=" + g_sessionId) >= 0;
}

static void clearSession()
{
  g_sessionId = "";
}

static void startSession()
{
  g_sessionId = String(esp_random(), HEX) + String(esp_random(), HEX);
  server.sendHeader("Set-Cookie", "sid=" + g_sessionId + "; Path=/; HttpOnly; SameSite=Lax");
}

static const char *mimeForPath(const String &path)
{
  if (path.endsWith(".css"))
    return "text/css";
  if (path.endsWith(".js"))
    return "application/javascript";
  if (path.endsWith(".html"))
    return "text/html";
  if (path.endsWith(".ico"))
    return "image/x-icon";
  return "application/octet-stream";
}

static bool serveStaticFile(const String &path)
{
  if (!g_fsOk || !LittleFS.exists(path))
    return false;
  File f = LittleFS.open(path, "r");
  if (!f)
    return false;
  server.streamFile(f, mimeForPath(path));
  f.close();
  return true;
}

static void apiUnauthorized()
{
  sendJson(server, 401, "{\"ok\":false,\"error\":\"Unauthorized\"}");
}

static bool requireAuthApi()
{
  if (sessionValid())
    return true;
  apiUnauthorized();
  return false;
}

static void redirectLogin()
{
  server.sendHeader("Location", "/login.html");
  server.send(302, "text/plain", "");
}

static bool isPublicWebUri(const String &uri)
{
  return uri == "/login.html" || uri.startsWith("/css/") || uri.startsWith("/js/");
}

static void queueDiscordPress(const String &target, const String &msg)
{
  if (!g_cfg.discordEnabled || g_cfg.discordUrl.isEmpty())
    return;
  if (!discordAllowsTarget(g_cfg.discordEvents, target))
  {
    Serial.printf("[Discord] skip (filter) target=%s ev=%s\n", target.c_str(),
                  g_cfg.discordEvents.c_str());
    return;
  }
  if (!discordEnqueue(msg))
    Serial.println("[Discord] queue full");
}

static bool parsePressArgs(const String &line, String &target, String &dur)
{
  if (!line.startsWith("press,"))
    return false;
  int c1 = line.indexOf(',');
  int c2 = line.indexOf(',', c1 + 1);
  target = c1 >= 0 ? line.substring(c1 + 1, c2 > 0 ? c2 : line.length()) : "";
  dur = c2 >= 0 ? line.substring(c2 + 1) : "short";
  target.trim();
  dur.trim();
  target.toLowerCase();
  dur.toLowerCase();
  if (dur != "short" && dur != "long")
    return false;
  return target == "power" || target == "reset";
}

static bool runOptoPress(const String &target, const String &dur)
{
  gpio_num_t pin;
  if (target == "power")
    pin = PIN_OPTO_POWER;
  else if (target == "reset")
    pin = PIN_OPTO_RESET;
  else
    return false;

  uint32_t ms = (dur == "long") ? PULSE_LONG_MS : PULSE_SHORT_MS;
  optoRequest(pin, ms);
  ledPulseStart(2);
  return true;
}

static void pusherOnMessage(const char *data, size_t length)
{
  if (!data || length == 0)
    return;

  String line;
  if (!extractPusherCommand(data, length, line))
    return;
  line.trim();

  String target, dur;
  if (!parsePressArgs(line, target, dur))
    return;

  if (optoBusy())
  {
    Serial.println("[Pusher] opto busy");
    return;
  }

  if (!runOptoPress(target, dur))
    return;

  queueDiscordPress(target, String("PC control: ") + line);
  Serial.printf("[Pusher] rx: %s\n", line.c_str());
}

void pusherReconnect()
{
  g_pusherStarted = false;
  pusher.disconnect();
  if (g_cfg.wsHost.isEmpty() || g_cfg.pusherChan.isEmpty())
    return;

  uint16_t port = 443;
  if (!g_cfg.wsPort.isEmpty())
    validatePortString(g_cfg.wsPort, port);

  bool tls = port == 443;
  String path = g_cfg.wsPath;
  if (path.isEmpty() && !g_cfg.pusherKey.isEmpty())
    path = String("/app/") + g_cfg.pusherKey;
  if (path.isEmpty())
    path = "/";
  pusher.setMessageHandler(pusherOnMessage);
  pusher.setReconnectInterval(5000);
  pusher.begin(g_cfg.wsHost.c_str(), port, path.c_str(), g_cfg.pusherChan.c_str(), tls);
  g_pusherStarted = true;
  Serial.println("[Pusher] (re)connect requested");
}

static void applyPerformanceDefaults()
{
  if (!validateCpuMhz(g_cfg.cpuMhz))
    g_cfg.cpuMhz = DEFAULT_CPU_MHZ;
  if (!validateLoopDelayMs(g_cfg.loopDelayMs))
    g_cfg.loopDelayMs = DEFAULT_LOOP_DELAY_MS;
  g_loopDelayMs = g_cfg.loopDelayMs;
  applyCpuMhz(g_cfg.cpuMhz);
}

void syncRuntimeFromConfig()
{
  applyPerformanceDefaults();
}

static void loadConfig()
{
  if (!nvsLoadConfig(g_cfg))
  {
    g_cfg.cpuMhz = DEFAULT_CPU_MHZ;
    g_cfg.loopDelayMs = DEFAULT_LOOP_DELAY_MS;
  }
  applyPerformanceDefaults();
}

static String buildConfigJson()
{
  String j = "{";
  j += "\"ok\":true,";
  j += "\"theme_mode\":\"" + jsonEscape(g_cfg.themeMode.isEmpty() ? "system" : g_cfg.themeMode) + "\",";
  j += "\"net_link\":\"" + jsonEscape(g_cfg.netLink.isEmpty() ? "auto" : g_cfg.netLink) + "\",";
  j += "\"ip_mode\":\"" + jsonEscape(g_cfg.ipMode.isEmpty() ? "dhcp" : g_cfg.ipMode) + "\",";
  j += "\"wifi_ssid\":\"" + jsonEscape(g_cfg.wifiSsid) + "\",";
  j += "\"wifi_pass\":\"" + jsonEscape(g_cfg.wifiPass.isEmpty() ? "" : "***") + "\",";
  j += "\"ip_addr\":\"" + jsonEscape(g_cfg.ipAddr) + "\",";
  j += "\"subnet\":\"" + jsonEscape(g_cfg.subnet) + "\",";
  j += "\"gateway\":\"" + jsonEscape(g_cfg.gateway) + "\",";
  j += "\"dns1\":\"" + jsonEscape(g_cfg.dns1) + "\",";
  j += "\"dns2\":\"" + jsonEscape(g_cfg.dns2) + "\",";
  j += "\"pusher_chan\":\"" + jsonEscape(g_cfg.pusherChan) + "\",";
  j += "\"ws_host\":\"" + jsonEscape(g_cfg.wsHost) + "\",";
  j += "\"ws_port\":\"" + jsonEscape(g_cfg.wsPort) + "\",";
  j += "\"ws_path\":\"" + jsonEscape(g_cfg.wsPath) + "\",";
  j += "\"pusher_key\":\"" + jsonEscape(g_cfg.pusherKey) + "\",";
  j += "\"pusher_cluster\":\"" + jsonEscape(g_cfg.pusherCluster) + "\",";
  j += "\"discord_enabled\":" + String(g_cfg.discordEnabled ? "true" : "false") + ",";
  j += "\"discord_url\":\"" + jsonEscape(g_cfg.discordUrl) + "\",";
  j += "\"discord_username\":\"" + jsonEscape(g_cfg.discordUsername) + "\",";
  j += "\"discord_events\":\"" + jsonEscape(g_cfg.discordEvents) + "\",";
  j += "\"cpu_mhz\":" + String(validateCpuMhz(g_cfg.cpuMhz) ? g_cfg.cpuMhz : DEFAULT_CPU_MHZ) + ",";
  j += "\"loop_delay_ms\":" + String(validateLoopDelayMs(g_cfg.loopDelayMs) ? g_cfg.loopDelayMs : DEFAULT_LOOP_DELAY_MS);
  j += "}";
  return j;
}

static void handleApiSession()
{
  if (sessionValid())
    sendJson(server, 200, "{\"ok\":true}");
  else
    apiUnauthorized();
}

static void handleApiLogin()
{
  if (!server.hasArg("password"))
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Password required\"}");
    return;
  }
  if (server.arg("password") == nvsReadPassword())
  {
    startSession();
    sendJson(server, 200, "{\"ok\":true}");
    return;
  }
  sendJson(server, 401, "{\"ok\":false,\"error\":\"Invalid password\"}");
}

static void handleApiLogout()
{
  clearSession();
  server.sendHeader("Set-Cookie", "sid=; Path=/; HttpOnly; Max-Age=0");
  sendJson(server, 200, "{\"ok\":true}");
}

static void handleApiConfig()
{
  if (!requireAuthApi())
    return;
  sendJson(server, 200, buildConfigJson());
}

static void handleApiTheme()
{
  if (!requireAuthApi())
    return;
  String mode = server.arg("theme_mode");
  if (!validateTheme(mode))
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Invalid theme\"}");
    return;
  }
  if (!nvsSaveTheme(mode))
  {
    sendJson(server, 500, "{\"ok\":false,\"error\":\"NVS save failed\"}");
    return;
  }
  g_cfg.themeMode = mode;
  sendJson(server, 200, "{\"ok\":true}");
}

static void handleApiStatus()
{
  if (!requireAuthApi())
    return;

  String ip, gw, mask, dns;
  netGetPrimaryIpv4(ip, gw, mask, dns);
  String linkText = netLinkStatusText();
  if (linkText.isEmpty())
    linkText = EMPTY_LABEL;
  String ethIp = netEthHasIp() ? ETH.localIP().toString() : "";
  String wifiIp = netWifiHasIp() ? WiFi.localIP().toString() : "";

  String json = "{";
  json += "\"ok\":true,";
  json += "\"link\":" + String(netHasIp() ? "true" : "false") + ",";
  json += "\"link_text\":\"" + jsonEscape(linkText) + "\",";
  json += "\"active_if\":\"" + jsonEscape(netActiveInterface()) + "\",";
  json += "\"eth_ip\":\"" + jsonEscape(ethIp) + "\",";
  json += "\"wifi_ip\":\"" + jsonEscape(wifiIp) + "\",";
  json += "\"ip\":\"" + jsonEscape(ip) + "\",";
  json += "\"gateway\":\"" + jsonEscape(gw) + "\",";
  json += "\"subnet\":\"" + jsonEscape(mask) + "\",";
  json += "\"dns\":\"" + jsonEscape(dns) + "\",";
  json += "\"mac\":\"" + jsonEscape(netPrimaryMac()) + "\",";
  json += "\"hostname\":\"" + jsonEscape(netPrimaryHostname()) + "\",";
  json += "\"chip\":\"" + jsonEscape(String(ESP.getChipModel())) + "\",";
  json += "\"uptime_sec\":" + String(millis() / 1000) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap());
  json += "}";

  sendJson(server, 200, json);
}

static void handleApiPress()
{
  if (!requireAuthApi())
    return;

  String target = server.arg("target");
  String dur = server.arg("dur");
  target.trim();
  dur.trim();
  target.toLowerCase();
  dur.toLowerCase();

  if (target != "power" && target != "reset")
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Invalid target\"}");
    return;
  }
  if (dur != "short" && dur != "long")
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Invalid duration\"}");
    return;
  }
  if (optoBusy())
  {
    sendJson(server, 409, "{\"ok\":false,\"error\":\"Opto busy\"}");
    return;
  }

  runOptoPress(target, dur);
  queueDiscordPress(target, String("Web press ") + target + " " + dur);
  sendJson(server, 200, "{\"ok\":true}");
}

static void handleApiPassword()
{
  if (!requireAuthApi())
    return;

  String p1 = server.arg("newpass");
  String p2 = server.arg("newpass2");
  if (p1.length() < 8)
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Min 8 characters\"}");
    return;
  }
  if (p1.length() > WEB_PASS_MAX_LEN)
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Max 64 characters\"}");
    return;
  }
  if (p1 != p2)
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Passwords do not match\"}");
    return;
  }
  if (!nvsSavePassword(p1))
  {
    sendJson(server, 500, "{\"ok\":false,\"error\":\"NVS save failed\"}");
    return;
  }
  sendJson(server, 200, "{\"ok\":true}");
}

static void fillCfgNetworkFromArgs(DeviceConfig &cfg)
{
  cfg.netLink = server.arg("net_link");
  cfg.ipMode = server.arg("ip_mode");
  cfg.ipAddr = server.arg("ip_addr");
  cfg.subnet = server.arg("subnet");
  cfg.gateway = server.arg("gateway");
  cfg.dns1 = server.arg("dns1");
  cfg.dns2 = server.arg("dns2");
  cfg.wifiSsid = server.arg("wifi_ssid");
  if (server.hasArg("wifi_pass") && server.arg("wifi_pass").length() > 0)
    cfg.wifiPass = server.arg("wifi_pass");
}

static void handleApiNetwork()
{
  if (!requireAuthApi())
    return;
  DeviceConfig tmp = g_cfg;
  fillCfgNetworkFromArgs(tmp);
  if (!nvsSaveNetwork(tmp))
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Invalid network config\"}");
    return;
  }
  g_cfg = tmp;
  sendJson(server, 200, "{\"ok\":true,\"reboot\":true}");
  delay(300);
  ESP.restart();
}

static void handleApiNetworkRenew()
{
  if (!requireAuthApi())
    return;
  if (g_cfg.ipMode != "dhcp")
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Not in DHCP mode\"}");
    return;
  }
  sendJson(server, 200, "{\"ok\":true}");
  delay(300);
  ESP.restart();
}

static void handleApiPusher()
{
  if (!requireAuthApi())
    return;
  DeviceConfig tmp = g_cfg;
  tmp.pusherChan = server.arg("pusher_chan");
  tmp.wsHost = server.arg("ws_host");
  tmp.wsPort = server.arg("ws_port");
  tmp.wsPath = server.arg("ws_path");
  tmp.pusherKey = server.arg("pusher_key");
  tmp.pusherCluster = server.arg("pusher_cluster");
  if (!nvsSavePusher(tmp))
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Invalid Pusher config\"}");
    return;
  }
  g_cfg = tmp;
  pusherReconnect();
  sendJson(server, 200, "{\"ok\":true}");
}

static void handleApiDiscord()
{
  if (!requireAuthApi())
    return;
  DeviceConfig tmp = g_cfg;
  tmp.discordEnabled = server.arg("discord_enabled") == "1" || server.arg("discord_enabled") == "true";
  tmp.discordUrl = server.arg("discord_url");
  tmp.discordUsername = server.arg("discord_username");
  tmp.discordEvents = server.arg("discord_events");
  if (!nvsSaveDiscord(tmp))
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Invalid Discord config\"}");
    return;
  }
  g_cfg = tmp;
  sendJson(server, 200, "{\"ok\":true}");
}

static void handleApiDiscordTest()
{
  if (!requireAuthApi())
    return;
  if (!g_cfg.discordEnabled)
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Discord disabled\"}");
    return;
  }
  if (!validateDiscordUrl(g_cfg.discordUrl))
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Invalid webhook URL\"}");
    return;
  }
  if (!discordEnqueue("PC Control webhook test"))
  {
    sendJson(server, 503, "{\"ok\":false,\"error\":\"Discord queue full\"}");
    return;
  }
  sendJson(server, 200, "{\"ok\":true,\"queued\":true}");
}

static void handleApiPerformance()
{
  if (!requireAuthApi())
    return;
  if (!server.hasArg("cpu_mhz") || !server.hasArg("loop_delay_ms"))
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Missing fields\"}");
    return;
  }
  long cpuArg = server.arg("cpu_mhz").toInt();
  long delayArg = server.arg("loop_delay_ms").toInt();
  if (cpuArg < 0 || delayArg < 0)
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Invalid number\"}");
    return;
  }
  uint32_t cpuMhz = (uint32_t)cpuArg;
  uint32_t loopMs = (uint32_t)delayArg;
  if (!validateCpuMhz(cpuMhz) || !validateLoopDelayMs(loopMs))
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Invalid performance values\"}");
    return;
  }
  DeviceConfig tmp = g_cfg;
  tmp.cpuMhz = cpuMhz;
  tmp.loopDelayMs = loopMs;
  if (!nvsSavePerformance(tmp))
  {
    sendJson(server, 500, "{\"ok\":false,\"error\":\"NVS save failed\"}");
    return;
  }
  g_cfg = tmp;
  g_loopDelayMs = loopMs;
  applyCpuMhz(cpuMhz);
  Serial.printf("[SYS] performance cpu=%u MHz loop_delay=%u ms\n", cpuMhz, loopMs);
  sendJson(server, 200, "{\"ok\":true}");
}

static void handleApiRestart()
{
  if (!requireAuthApi())
    return;
  Serial.println("[SYS] restart (web)");
  sendJson(server, 200, "{\"ok\":true}");
  delay(300);
  ESP.restart();
}

static void handleApiNvsClear()
{
  if (!requireAuthApi())
    return;
  if (server.arg("confirm") != "yes")
  {
    sendJson(server, 400, "{\"ok\":false,\"error\":\"Confirm required\"}");
    return;
  }
  Serial.println("[NVS] clear (web)");
  nvsClearAll();
  clearSession();
  sendJson(server, 200, "{\"ok\":true}");
  delay(300);
  ESP.restart();
}

static void handleStatic()
{
  String uri = server.uri();
  if (uri == "/")
    uri = "/index.html";

  if (uri == "/index.html" && !sessionValid())
  {
    redirectLogin();
    return;
  }

  if (uri == "/login.html" && sessionValid())
  {
    server.sendHeader("Location", "/index.html");
    server.send(302, "text/plain", "");
    return;
  }

  if (serveStaticFile(uri))
    return;

  if (uri.startsWith("/api/"))
    sendJson(server, 404, "{\"ok\":false,\"error\":\"Not found\"}");
  else if (isPublicWebUri(uri))
    server.send(404, "text/plain", "Not found (upload LittleFS?)");
  else if (!sessionValid())
    redirectLogin();
  else
    serveStaticFile("/index.html");
}

static void setupWebRoutes()
{
  server.on("/api/session", HTTP_GET, handleApiSession);
  server.on("/api/login", HTTP_POST, handleApiLogin);
  server.on("/api/logout", HTTP_POST, handleApiLogout);
  server.on("/api/config", HTTP_GET, handleApiConfig);
  server.on("/api/theme", HTTP_POST, handleApiTheme);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/press", HTTP_POST, handleApiPress);
  server.on("/api/password", HTTP_POST, handleApiPassword);
  server.on("/api/network", HTTP_POST, handleApiNetwork);
  server.on("/api/network/renew", HTTP_POST, handleApiNetworkRenew);
  server.on("/api/pusher", HTTP_POST, handleApiPusher);
  server.on("/api/discord", HTTP_POST, handleApiDiscord);
  server.on("/api/discord/test", HTTP_POST, handleApiDiscordTest);
  server.on("/api/performance", HTTP_POST, handleApiPerformance);
  server.on("/api/restart", HTTP_POST, handleApiRestart);
  server.on("/api/nvs/clear", HTTP_POST, handleApiNvsClear);
  server.onNotFound(handleStatic);
}

static void pollFactoryButton()
{
  bool down = digitalRead(PIN_BTN_FACTORY) == LOW;
  uint32_t now = millis();
  if (!down)
  {
    g_factorySeenRelease = true;
    g_factoryHoldStart = 0;
    g_factoryTriggered = false;
    return;
  }
  if (!g_factorySeenRelease)
    return;
  if (g_factoryTriggered)
    return;
  if (g_factoryHoldStart == 0)
    g_factoryHoldStart = now;
  uint32_t held = now - g_factoryHoldStart;
  if ((held / 300) % 2 == 0)
    digitalWrite(PIN_LED_STATUS, HIGH);
  else
    digitalWrite(PIN_LED_STATUS, LOW);
  if (held >= FACTORY_HOLD_MS)
  {
    g_factoryTriggered = true;
    nvsClearAll();
    clearSession();
    Serial.println("[NVS] factory clear");
    delay(200);
    ESP.restart();
  }
}

void setup()
{
  pinMode(PIN_LED_STATUS, OUTPUT);
  pinMode(PIN_BTN_FACTORY, INPUT);
  digitalWrite(PIN_LED_STATUS, LOW);
  optoInit();

  Serial.begin(SERIAL_BAUD);
  delay(200);

  g_fsOk = LittleFS.begin(false);
  if (!g_fsOk)
    Serial.println("[FS] LittleFS mount failed (upload data/ image)");

  loadConfig();
  discordWorkerInit();
  if (g_cfg.themeMode.isEmpty())
    g_cfg.themeMode = "system";
  Serial.printf("[BOOT] pc_control LAN/WLAN (CPU %u MHz, loop %u ms)\n", g_cfg.cpuMhz, g_loopDelayMs);

  netBegin(g_cfg);
  setupWebRoutes();
  {
    static const char *headerKeys[] = {"Cookie"};
    server.collectHeaders(headerKeys, 1);
  }
  server.begin();

  pusherReconnect();
  serialCmdPrintBootHint();

  Serial.println("[WEB] HTTP :80 session cookie (browser close = re-login)");
}

void loop()
{
  server.handleClient();
  serialCmdPoll();
  optoPoll();
  ledPulsePoll();
  pollFactoryButton();
  if (g_pusherStarted)
    pusher.loop();

  static uint32_t lastBeat = 0;
  if (millis() - lastBeat >= 5000)
  {
    lastBeat = millis();
    if (netHasIp())
    {
      Serial.printf("[alive] http://%s/ (%s)\n", netPrimaryIp().c_str(), netActiveInterface().c_str());
      if (netEthHasIp() && netWifiHasIp())
        Serial.printf("[alive] wifi http://%s/\n", WiFi.localIP().toString().c_str());
    }
  }

  if (g_loopDelayMs > 0)
    delay(g_loopDelayMs);
}
