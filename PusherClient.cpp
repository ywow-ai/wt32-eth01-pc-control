#include "PusherClient.h"

#include <Arduino.h>
#include <cctype>
#include <cstring>

PusherClient *PusherClient::_active = nullptr;

void PusherClient::setMessageHandler(MessageHandler handler)
{
  _handler = handler;
}

void PusherClient::setReconnectInterval(uint32_t ms)
{
  _ws.setReconnectInterval(ms);
}

void PusherClient::disconnect()
{
  _ws.disconnect();
}

void PusherClient::loop()
{
  _ws.loop();
}

void PusherClient::begin(const char *host, uint16_t port, const char *path, const char *channel, bool useTls)
{
  _channel = channel ? channel : "";
  _active = this;

  _ws.disconnect();
  _ws.onEvent(onEventStatic);

  if (useTls)
    _ws.beginSSL(host, port, path);
  else
    _ws.begin(host, port, path);
}

void PusherClient::onEventStatic(WStype_t type, uint8_t *payload, size_t length)
{
  if (_active)
    _active->handleEvent(type, payload, length);
}

bool PusherClient::containsEvent(const char *data, size_t length, const char *eventName) const
{
  size_t nlen = strlen(eventName);
  if (length < nlen)
    return false;
  for (size_t i = 0; i + nlen <= length; i++)
  {
    if (memcmp(data + i, eventName, nlen) == 0)
      return true;
  }
  return false;
}

bool PusherClient::isValidChannelName(const String &name) const
{
  if (name.isEmpty())
    return false;
  for (size_t i = 0; i < name.length(); i++)
  {
    char c = name.charAt(i);
    if (isalnum((unsigned char)c) || c == '-' || c == '_')
      continue;
    return false;
  }
  return true;
}

void PusherClient::sendPong()
{
  _ws.sendTXT("{\"event\":\"pusher:pong\",\"data\":{}}");
}

void PusherClient::subscribeChannel()
{
  if (_channel.isEmpty())
  {
    Serial.println("[Pusher] channel empty, skip subscribe");
    return;
  }

  if (!isValidChannelName(_channel))
  {
    Serial.println("[Pusher] invalid channel name, skip subscribe");
    return;
  }

  String msg = String("{\"event\":\"pusher:subscribe\",\"data\":{\"channel\":\"") + _channel + "\"}}";
  _ws.sendTXT(msg);
}

void PusherClient::handleEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.println("[Pusher] disconnected");
    break;
  case WStype_CONNECTED:
    Serial.println("[Pusher] socket connected");
    break;
  case WStype_TEXT:
  {
    const char *data = (const char *)payload;

    if (containsEvent(data, length, "pusher:connection_established"))
    {
      subscribeChannel();
      return;
    }

    if (containsEvent(data, length, "pusher:ping"))
    {
      sendPong();
      return;
    }

    if (containsEvent(data, length, "pusher:pong") || containsEvent(data, length, "pusher_internal"))
      return;

    if (containsEvent(data, length, "subscription_succeeded"))
    {
      Serial.println("[Pusher] ready");
      return;
    }

    if (_handler)
      _handler(data, length);
    break;
  }
  default:
    break;
  }
}
