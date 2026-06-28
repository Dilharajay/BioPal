#include "config_store.h"
#include <Preferences.h>

String current_wifi_ssid = "";
String current_wifi_pass = "";
String current_api_endpoint = "";
String current_api_token = "";

static Preferences preferences;

void loadConfig() {
    preferences.begin("health-mon", true); // read-only mode first
    current_wifi_ssid = preferences.getString("wifi_ssid", "SLT-4G-87D7");
    current_wifi_pass = preferences.getString("wifi_pass", "HJBT5JD1NY0");
    current_api_endpoint = preferences.getString("api_ep", "http://192.168.1.100:3000/api/vitals");
    current_api_token = preferences.getString("api_token", "Bearer your-token-here");
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
    
    preferences.end();
}
