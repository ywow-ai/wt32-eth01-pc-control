// WT32-ETH01 — tes Ethernet (serial + ping dari laptop)
// FQBN: esp32:esp32:wt32-eth01
//
// Static default: WT32 192.168.1.200, laptop eth0 192.168.1.100/24
// DHCP: arduino-cli compile ... --build-property compiler.cpp.extra_flags=-DETH_TEST_DHCP=1

#include <Arduino.h>
#include <ETH.h>
#include <WiFi.h>
#include <esp_arduino_version.h>
#include <esp_system.h>

#ifndef ETH_TEST_DHCP
#define ETH_TEST_USE_STATIC 1
#else
#define ETH_TEST_USE_STATIC 0
#endif

#if ETH_TEST_USE_STATIC
static IPAddress LocalIP(192, 168, 1, 200);
static IPAddress Gateway(192, 168, 1, 1);
static IPAddress Subnet(255, 255, 255, 0);
static IPAddress DNS(192, 168, 1, 1);
#endif

#define PHY_ADDR 1
#define PHY_POWER 16
#define PHY_MDC 23
#define PHY_MDIO 18

static bool g_ethBeginOk = false;

static void printEthStatus(const char *tag)
{
  Serial.printf("[status] %s | begin_ok=%s link=%s speed=%u duplex=%s ip=%s mac=%s\n",
                tag,
                g_ethBeginOk ? "true" : "false",
                ETH.linkUp() ? "UP" : "DOWN",
                ETH.linkSpeed(),
                ETH.fullDuplex() ? "FULL" : "HALF",
                ETH.localIP().toString().c_str(),
                ETH.macAddress().c_str());
  Serial.flush();
}

static void onEthEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case ARDUINO_EVENT_ETH_START:
    Serial.println("[event] ETH_START");
    ETH.setHostname("wt32-eth-test");
    break;
  case ARDUINO_EVENT_ETH_CONNECTED:
    Serial.println("[event] ETH_CONNECTED (link up)");
    break;
  case ARDUINO_EVENT_ETH_GOT_IP:
    Serial.print("[event] ETH_GOT_IP ");
    Serial.println(ETH.localIP());
    break;
  case ARDUINO_EVENT_ETH_LOST_IP:
    Serial.println("[event] ETH_LOST_IP");
    break;
  case ARDUINO_EVENT_ETH_DISCONNECTED:
    Serial.println("[event] ETH_DISCONNECTED");
    break;
  default:
    break;
  }
  Serial.flush();
}

static bool ethBeginWt32()
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  return ETH.begin(ETH_PHY_LAN8720, PHY_ADDR, PHY_MDC, PHY_MDIO, PHY_POWER, ETH_CLOCK_GPIO0_IN);
#else
  return ETH.begin(PHY_ADDR, PHY_POWER, PHY_MDC, PHY_MDIO, ETH_PHY_LAN8720, ETH_CLOCK_GPIO0_IN);
#endif
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("========== ESP STATUS ==========");
  Serial.printf("[esp] reset_reason=%d (1=POWERON)\n", (int)esp_reset_reason());
  Serial.printf("[esp] chip_rev=%u cpu_mhz=%u core=%d\n",
                ESP.getChipRevision(),
                getCpuFrequencyMhz(),
                ESP_ARDUINO_VERSION_MAJOR);
  Serial.println("[esp] firmware=eth_test (WT32-ETH01 LAN8720 GPIO0_IN phy=1 pwr=16)");
#if ETH_TEST_USE_STATIC
  Serial.println("[esp] ip_mode=static 192.168.1.200 (laptop eth 192.168.1.100/24)");
#else
  Serial.println("[esp] ip_mode=DHCP");
#endif
  Serial.flush();

  WiFi.onEvent(onEthEvent);

  Serial.println("[eth] ETH.begin()...");
  Serial.flush();
  g_ethBeginOk = ethBeginWt32();
  Serial.printf("[eth] begin() => %s\n", g_ethBeginOk ? "true" : "false");
  if (!g_ethBeginOk)
  {
    Serial.println("[eth] Ethernet Init Failed (cek power up timeout / PHY)");
    Serial.flush();
    return;
  }

#if ETH_TEST_USE_STATIC
  if (!ETH.config(LocalIP, Gateway, Subnet, DNS))
  {
    Serial.println("[eth] Static IP Config Failed");
  }
  else
  {
    Serial.print("[eth] Static IP: ");
    Serial.println(LocalIP);
  }
#else
  Serial.println("[eth] waiting DHCP...");
#endif

  delay(500);
  printEthStatus("after_begin");
  Serial.println("========== loop: LAN status (ping IP dari laptop) ==========");
  Serial.flush();
}

void loop()
{
  static uint32_t last = 0;
  if (millis() - last < 5000)
    return;
  last = millis();

  Serial.print("LAN: ");
  Serial.print(ETH.linkUp() ? "UP" : "DOWN");
  Serial.print(" - IP: ");
  Serial.println(ETH.localIP());
  printEthStatus("tick");
}
