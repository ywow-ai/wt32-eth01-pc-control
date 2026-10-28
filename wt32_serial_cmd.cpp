#include "wt32_serial_cmd.h"
#include "wt32_support.h"
#include <Arduino.h>

extern DeviceConfig g_cfg;
extern void pusherReconnect();
extern void syncRuntimeFromConfig();

static String g_line;

static void printHelp()
{
  Serial.println("[SERIAL] commands (115200):");
  Serial.println("  help");
  Serial.println("  show              — NVS config (secrets masked)");
  Serial.println("  show secrets      — include wifi_pass / web_pass");
  Serial.println("  get <key>         — one NVS key");
  Serial.println("  set <key> <val>   — save to NVS (reboot for network link)");
  Serial.println("  reboot | restart");
  Serial.println("  nvs clear confirm=yes");
  Serial.println("[SERIAL] keys: net_link (auto|ethernet|wifi), wifi_ssid, wifi_pass,");
  Serial.println("  ip_mode, ip_addr, subnet, gateway, dns1, dns2, theme_mode, web_pass,");
  Serial.println("  pusher_*, ws_*, discord_*, cpu_mhz, loop_delay_ms");
}

static void handleLine(String line)
{
  line.trim();
  if (line.isEmpty())
    return;

  if (line.equalsIgnoreCase("help") || line == "?")
  {
    printHelp();
    return;
  }

  if (line.equalsIgnoreCase("show") || line.equalsIgnoreCase("show secrets"))
  {
    configPrintSerial(line.indexOf("secrets") >= 0);
    return;
  }

  if (line.equalsIgnoreCase("reboot") || line.equalsIgnoreCase("restart"))
  {
    Serial.println("[SERIAL] reboot");
    delay(200);
    ESP.restart();
  }

  if (line.startsWith("nvs clear"))
  {
    if (!line.endsWith("confirm=yes"))
    {
      Serial.println("[SERIAL] use: nvs clear confirm=yes");
      return;
    }
    nvsClearAll();
    Serial.println("[SERIAL] NVS cleared, reboot");
    delay(200);
    ESP.restart();
  }

  if (line.startsWith("get "))
  {
    String key = line.substring(4);
    key.trim();
    if (!configPrintSerialKey(key, false))
      Serial.println("[SERIAL] ERR unknown key (help)");
    return;
  }

  if (line.startsWith("set "))
  {
    String rest = line.substring(4);
    rest.trim();
    int sp = rest.indexOf(' ');
    if (sp < 0)
    {
      Serial.println("[SERIAL] usage: set <key> <value>");
      return;
    }
    String key = rest.substring(0, sp);
    String val = rest.substring(sp + 1);
    val.trim();
    String err;
    if (!configSetFromSerial(key, val, err))
    {
      Serial.print("[SERIAL] ERR ");
      Serial.println(err);
      return;
    }
    nvsLoadConfig(g_cfg);
    syncRuntimeFromConfig();
    Serial.println("[SERIAL] OK (network link: reboot)");
    String k = key;
    k.toLowerCase();
    if (k.startsWith("pusher") || k.startsWith("ws_"))
      pusherReconnect();
    return;
  }

  Serial.println("[SERIAL] unknown (help)");
}

void serialCmdPrintBootHint()
{
  Serial.println("[SERIAL] NVS: type 'help' for commands");
}

void serialCmdPoll()
{
  while (Serial.available())
  {
    char c = (char)Serial.read();
    if (c == '\r')
      continue;
    if (c == '\n')
    {
      handleLine(g_line);
      g_line = "";
      continue;
    }
    if (g_line.length() < 240)
      g_line += c;
  }
}
