// WT32-ETH01 — cek status ESP dulu (serial), baru cek IP di jaringan
// FQBN: esp32:esp32:wt32-eth01
#include <ETH.h>
#include <Network.h>
#include <esp_system.h>

static bool g_ethBeginOk = false;

static void printEthStatus(const char *tag)
{
  Serial.printf("[status] %s | begin_ok=%s link=%s speed=%u duplex=%s ip=%s mac=%s\n", tag, g_ethBeginOk ? "true" : "false", ETH.linkUp() ? "UP" : "DOWN", ETH.linkSpeed(), ETH.fullDuplex() ? "FULL" : "HALF", ETH.localIP().toString().c_str(), ETH.macAddress().c_str());
  Serial.flush();
}

static void onEthEvent(arduino_event_id_t event)
{
  switch (event)
  {
  case ARDUINO_EVENT_ETH_START:
    Serial.println("[event] ETH_START");
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

void setup()
{
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println("========== ESP STATUS ==========");
  Serial.printf("[esp] reset_reason=%d (1=POWERON)\n", (int)esp_reset_reason());
  Serial.printf("[esp] chip_rev=%u cpu_mhz=%u\n", ESP.getChipRevision(), getCpuFrequencyMhz());
  Serial.println("[esp] firmware=eth_test (WT32-ETH01, LAN8720, clk=GPIO0_IN, phy=1, pwr=16)");
  Serial.flush();

  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);
  delay(200);

  Network.onEvent(onEthEvent);

  Serial.println("[eth] calling ETH.begin()...");
  Serial.flush();
  g_ethBeginOk = ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, 16, ETH_CLOCK_GPIO0_IN);
  Serial.printf("[eth] begin() => %s\n", g_ethBeginOk ? "true" : "false");
  Serial.flush();

  delay(1500);
  printEthStatus("after_begin+1.5s");
  Serial.println("========== loop: status tiap 3s (cek IP di router setelah link=UP) ==========");
  Serial.flush();
}

void loop()
{
  static uint32_t last = 0;
  if (millis() - last >= 3000)
  {
    last = millis();
    printEthStatus("tick");
  }
}
