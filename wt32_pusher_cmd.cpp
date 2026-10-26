#include "wt32_pusher_cmd.h"

#include <cstring>

static bool readJsonQuotedValue(const char *start, const char *end, String &out)
{
  if (start >= end || *start != '"')
    return false;

  out = "";
  for (const char *p = start + 1; p < end; p++)
  {
    char c = *p;
    if (c == '\\')
    {
      if (p + 1 >= end)
        return false;
      out += *++p;
      continue;
    }
    if (c == '"')
      return true;
    out += c;
  }
  return false;
}

static const char *findCmdKeyColon(const char *data, size_t length)
{
  static const char *patterns[] = {
      "\"cmd\":",
      "\\\"cmd\\\":",
      "\"cmd\" :",
      "\\\"cmd\\\" :",
  };

  const char *best = nullptr;
  for (const char *pat : patterns)
  {
    const char *search = data;
    size_t patLen = strlen(pat);
    while (search && (search = strstr(search, pat)) != nullptr)
    {
      if ((size_t)(search - data) >= length)
        break;
      best = search + patLen;
      search++;
    }
  }
  return best;
}

static void sanitizePusherCmd(String &cmd)
{
  cmd.trim();
  int cut = (int)cmd.length();
  for (int i = 0; i < cut; i++)
  {
    char c = cmd.charAt(i);
    if (c == '"' || c == '}' || c == '\\' || (unsigned char)c < 32)
    {
      cut = i;
      break;
    }
  }
  cmd = cmd.substring(0, cut);
  cmd.trim();
}

bool extractPusherCommand(const char *data, size_t length, String &outCmd)
{
  outCmd = "";
  if (!data || length == 0)
    return false;

  while (length > 0 && (data[length - 1] == '\n' || data[length - 1] == '\r'))
    length--;

  const char *end = data + length;
  const char *cmdVal = findCmdKeyColon(data, length);
  if (cmdVal && cmdVal < end)
  {
    while (cmdVal < end && (*cmdVal == ' ' || *cmdVal == '\\'))
      cmdVal++;

    if (cmdVal < end && *cmdVal == '"')
    {
      if (readJsonQuotedValue(cmdVal, end, outCmd))
      {
        sanitizePusherCmd(outCmd);
        return !outCmd.isEmpty();
      }
    }
    else
    {
      while (cmdVal < end)
      {
        char c = *cmdVal;
        if (c == '\\' && cmdVal + 1 < end)
        {
          if (cmdVal[1] == '"')
            break;
          outCmd += cmdVal[1];
          cmdVal += 2;
          continue;
        }
        if (c == '"' || c == '}')
          break;
        outCmd += c;
        cmdVal++;
      }
      outCmd.trim();
      sanitizePusherCmd(outCmd);
      if (!outCmd.isEmpty())
        return true;
    }
  }

  if (data[0] != '{')
  {
    outCmd = String(data, length);
    sanitizePusherCmd(outCmd);
    return !outCmd.isEmpty();
  }

  const char *press = strstr(data, "press,");
  if (press && (size_t)(press - data) < length)
  {
    size_t remain = length - (size_t)(press - data);
    outCmd = String(press, remain);
    sanitizePusherCmd(outCmd);
    return outCmd.startsWith("press,");
  }

  return false;
}
