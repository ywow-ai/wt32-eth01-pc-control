#pragma once

#include <WebSocketsClient.h>
#include <functional>

class PusherClient
{
public:
  using MessageHandler = std::function<void(const char *data, size_t length)>;

  void setMessageHandler(MessageHandler handler);
  void setReconnectInterval(uint32_t ms);
  void disconnect();
  void loop();
  void begin(const char *host, uint16_t port, const char *path, const char *channel, bool useTls);

private:
  WebSocketsClient _ws;
  String _channel;
  MessageHandler _handler;
  static PusherClient *_active;

  void handleEvent(WStype_t type, uint8_t *payload, size_t length);
  static void onEventStatic(WStype_t type, uint8_t *payload, size_t length);
  void subscribeChannel();
  void sendPong();
  bool containsEvent(const char *data, size_t length, const char *eventName) const;
  bool isValidChannelName(const String &name) const;
};
