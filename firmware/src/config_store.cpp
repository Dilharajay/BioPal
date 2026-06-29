#include "config_store.h"
#include <Preferences.h>

String current_wifi_ssid = "";
String current_wifi_pass = "";
String current_api_endpoint = "";
String current_api_token = "";
bool debugModeEnabled = false;

static Preferences preferences;

void loadConfig() {
    preferences.begin("health-mon", true); // read-only mode first
    current_wifi_ssid = preferences.getString("wifi_ssid", "SLT-4G-87D7");
    current_wifi_pass = preferences.getString("wifi_pass", "HJBT5JD1NY0");
    current_api_endpoint = preferences.getString("api_ep", "http://192.168.1.100:3000/api/vitals");
    current_api_token = preferences.getString("api_token", "Bearer your-token-here");
    debugModeEnabled = preferences.getBool("debug_mode", false);
    preferences.end();
}

void saveConfig(const String& ssid, const String& pass, const String& api_ep, const String& api_token) {
    preferences.begin("health-mon", false); // read-write mode
    
    if (ssid.length() > 0) {
        preferences.putString("wifi_ssid", ssid);
        current_wifi_ssid = ssid;
    }
    if (pass.length() > 0) {
        preferences.putString("wifi_pass", pass);
        current_wifi_pass = pass;
    }
    if (api_ep.length() > 0) {
        preferences.putString("api_ep", api_ep);
        current_api_endpoint = api_ep;
    }
    if (api_token.length() > 0) {
        preferences.putString("api_token", api_token);
        current_api_token = api_token;
    }
    
    preferences.putBool("debug_mode", debugModeEnabled);
    
    preferences.end();
}

void addLog(const String& msg) {
    preferences.begin("health-mon", false);
    uint8_t head = preferences.getUChar("log_head", 0);
    
    String key = "log" + String(head);
    preferences.putString(key.c_str(), msg);
    
    head = (head + 1) % 10;
    preferences.putUChar("log_head", head);
    preferences.end();
}

void printLogs() {
    preferences.begin("health-mon", true);
    uint8_t head = preferences.getUChar("log_head", 0);
    
    Serial.println("--- EEPROM Logs (Last 10) ---");
    bool empty = true;
    for (int i = 0; i < 10; i++) {
        int idx = (head + i) % 10;
        String key = "log" + String(idx);
        String logLine = preferences.getString(key.c_str(), "");
        if (logLine.length() > 0) {
            Serial.printf("[%d] %s\n", idx, logLine.c_str());
            empty = false;
        }
    }
    if (empty) Serial.println("(No logs stored)");
    Serial.println("-----------------------------");
    preferences.end();
}
