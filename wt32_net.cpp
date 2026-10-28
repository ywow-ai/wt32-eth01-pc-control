#include "wt32_net.h"
#include <ETH.h>
#include <Network.h>
#include <WiFi.h>

extern DeviceConfig g_cfg;

static bool g_ethHasIp = false;
static bool g_wifiHasIp = false;

static bool applyStaticEth(const DeviceConfig &cfg)
{
  if (cfg.ipMode != "static")
    return true;
  if (!validateIpv4(cfg.ipAddr) || !validateIpv4(cfg.subnet) || !validateIpv4(cfg.gateway))
    return false;

  IPAddress ip, gw, sn, d1(8, 8, 8, 8), d2;
  if (!ip.fromString(cfg.ipAddr) || !gw.fromString(cfg.gateway) || !sn.fromString(cfg.subnet))
    return false;
  if (!cfg.dns1.isEmpty())
    d1.fromString(cfg.dns1);
  if (!cfg.dns2.isEmpty())
    d2.fromString(cfg.dns2);
  return ETH.config(ip, gw, sn, d1, d2);
}

static bool applyStaticWifi(const DeviceConfig &cfg)
{
  if (cfg.ipMode != "static")
    return true;
  if (!validateIpv4(cfg.ipAddr) || !validateIpv4(cfg.subnet) || !validateIpv4(cfg.gateway))
    return false;

  IPAddress ip, gw, sn, d1(8, 8, 8, 8), d2;
  if (!ip.fromString(cfg.ipAddr) || !gw.fromString(cfg.gateway) || !sn.fromString(cfg.subnet))
    return false;
  if (!cfg.dns1.isEmpty())
    d1.fromString(cfg.dns1);
  if (!cfg.dns2.isEmpty())
    d2.fromString(cfg.dns2);
  return WiFi.config(ip, gw, sn, d1, d2);
}

static bool wantEthernet(const DeviceConfig &cfg)
{
  String m = cfg.netLink.isEmpty() ? "auto" : cfg.netLink;
  return m == "auto" || m == "ethernet";
}

static bool wantWifi(const DeviceConfig &cfg)
{
  String m = cfg.netLink.isEmpty() ? "auto" : cfg.netLink;
  return m == "auto" || m == "wifi";
}

static void onNetEvent(arduino_event_id_t event)
{
  switch (event)
  {
  case ARDUINO_EVENT_ETH_START:
    ETH.setHostname("pc-control-eth");
    break;
  case ARDUINO_EVENT_ETH_GOT_IP:
    g_ethHasIp = true;
    Serial.print("[ETH] IP ");
    Serial.println(ETH.localIP());
    netUpdateLed();
    break;
  case ARDUINO_EVENT_ETH_LOST_IP:
  case ARDUINO_EVENT_ETH_DISCONNECTED:
    g_ethHasIp = false;
    netUpdateLed();
    break;
  case ARDUINO_EVENT_WIFI_STA_START:
    WiFi.setHostname("pc-control-wifi");
    break;
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    g_wifiHasIp = true;
    Serial.print("[WIFI] IP ");
    Serial.println(WiFi.localIP());
    netUpdateLed();
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    g_wifiHasIp = false;
    netUpdateLed();
    break;
  default:
    break;
  }
}

void netUpdateLed()
{
  digitalWrite(PIN_LED_STATUS, netHasIp() ? HIGH : LOW);
}

void netBegin(const DeviceConfig &cfg)
{
  g_ethHasIp = false;
  g_wifiHasIp = false;

  Network.onEvent(onNetEvent);

  if (wantEthernet(cfg))
  {
    if (!applyStaticEth(cfg) && cfg.ipMode == "static")
      Serial.println("[ETH] warn: static config invalid, DHCP at boot");
    if (!ETH.begin())
      Serial.println("[ETH] begin() failed");
  }

  if (wantWifi(cfg))
  {
    if (cfg.wifiSsid.isEmpty())
    {
      if (cfg.netLink == "wifi")
        Serial.println("[WIFI] no SSID (set wifi_ssid via web or serial)");
    }
    else
    {
      WiFi.persistent(false);
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(WIFI_PS_NONE);
      if (!applyStaticWifi(cfg) && cfg.ipMode == "static")
        Serial.println("[WIFI] warn: static config invalid, DHCP at boot");
      WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
    }
  }
}

bool netEthHasIp()
{
  return g_ethHasIp && ETH.localIP() != IPAddress(0, 0, 0, 0);
}

bool netWifiHasIp()
{
  return g_wifiHasIp && WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

bool netHasIp()
{
  return netEthHasIp() || netWifiHasIp();
}

String netActiveInterface()
{
  if (netEthHasIp())
    return "ethernet";
  if (netWifiHasIp())
    return "wifi";
  return "";
}

String netPrimaryIp()
{
  if (netEthHasIp())
    return ETH.localIP().toString();
  if (netWifiHasIp())
    return WiFi.localIP().toString();
  return "";
}

String netLinkStatusText()
{
  if (!netHasIp())
    return "";
  String mode = g_cfg.ipMode == "static" ? "static" : "DHCP";
  if (netEthHasIp())
    return "Ethernet up (" + mode + ")";
  return "Wi-Fi up (" + mode + ")";
}

void netGetPrimaryIpv4(String &ip, String &gw, String &mask, String &dns)
{
  ip = gw = mask = dns = "";
  if (netEthHasIp())
  {
    ip = ETH.localIP().toString();
    gw = ETH.gatewayIP().toString();
    mask = ETH.subnetMask().toString();
    dns = ETH.dnsIP().toString();
  }
  else if (netWifiHasIp())
  {
    ip = WiFi.localIP().toString();
    gw = WiFi.gatewayIP().toString();
    mask = WiFi.subnetMask().toString();
    dns = WiFi.dnsIP().toString();
  }
  if (dns == "0.0.0.0")
    dns = "";
}

String netPrimaryMac()
{
  if (netEthHasIp())
    return ETH.macAddress();
  if (netWifiHasIp())
    return WiFi.macAddress();
  if (wantEthernet(g_cfg))
    return ETH.macAddress();
  return WiFi.macAddress();
}

String netPrimaryHostname()
{
  if (netEthHasIp())
    return ETH.getHostname() ? ETH.getHostname() : "pc-control-eth";
  if (netWifiHasIp())
    return WiFi.getHostname() ? WiFi.getHostname() : "pc-control-wifi";
  return "pc-control";
}
