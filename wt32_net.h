#pragma once

#include "wt32_support.h"

void netBegin(const DeviceConfig &cfg);
void netUpdateLed();

bool netEthHasIp();
bool netWifiHasIp();
bool netHasIp();

String netPrimaryIp();
String netActiveInterface();
String netLinkStatusText();

void netGetPrimaryIpv4(String &ip, String &gw, String &mask, String &dns);
String netPrimaryMac();
String netPrimaryHostname();
