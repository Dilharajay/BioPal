#pragma once
#include <Arduino.h>

extern String current_wifi_ssid;
extern String current_wifi_pass;
extern String current_api_endpoint;
extern String current_api_token;
extern bool serialLoggingEnabled;

void loadConfig();
void saveConfig(const String& ssid, const String& pass, const String& api_ep, const String& api_token);

void addLog(const String& msg);
void printLogs();
