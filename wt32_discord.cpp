#include "wt32_discord.h"

#include "wt32_support.h"

#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

extern DeviceConfig g_cfg;

static QueueHandle_t g_discordQueue = nullptr;

struct DiscordQueueItem
{
  char text[192];
};

static bool sendDiscordWebhookBlocking(const String &content)
{
  if (!g_cfg.discordEnabled || !validateDiscordUrl(g_cfg.discordUrl))
    return false;

  HTTPClient http;
  http.setTimeout(3000);
  if (!http.begin(g_cfg.discordUrl))
    return false;
  http.addHeader("Content-Type", "application/json");
  String user = g_cfg.discordUsername.isEmpty() ? "PC Control" : g_cfg.discordUsername;
  String body = String("{\"username\":\"") + jsonEscape(user) + "\",\"content\":\"" + jsonEscape(content) + "\"}";
  int code = http.POST(body);
  http.end();
  return code >= 200 && code < 300;
}

static void discordWorkerTask(void *)
{
  DiscordQueueItem item;
  for (;;)
  {
    if (xQueueReceive(g_discordQueue, &item, portMAX_DELAY) != pdTRUE)
      continue;
    if (!sendDiscordWebhookBlocking(String(item.text)))
      Serial.println("[Discord] webhook POST failed");
    vTaskDelay(1);
  }
}

void discordWorkerInit()
{
  if (g_discordQueue)
    return;
  g_discordQueue = xQueueCreate(4, sizeof(DiscordQueueItem));
  if (!g_discordQueue)
  {
    Serial.println("[Discord] queue create failed");
    return;
  }
  xTaskCreatePinnedToCore(discordWorkerTask, "discord", 8192, nullptr, 0, nullptr, 0);
}

bool discordEnqueue(const String &msg)
{
  if (!g_discordQueue || msg.isEmpty() || msg.length() >= sizeof(DiscordQueueItem::text))
    return false;
  DiscordQueueItem item;
  memset(item.text, 0, sizeof(item.text));
  msg.toCharArray(item.text, sizeof(item.text));
  return xQueueSend(g_discordQueue, &item, 0) == pdTRUE;
}
