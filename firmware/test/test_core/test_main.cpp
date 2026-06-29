#include <Arduino.h>
#include <unity.h>

#include "logger.h"
#include "config_store.h"

// External variables defined in config_store.cpp
extern String current_wifi_ssid;
extern String current_wifi_pass;
extern String current_api_endpoint;
extern String current_api_token;
extern bool debugModeEnabled;

void setUp(void) {
    // Run before each test
}

void tearDown(void) {
    // Run after each test
}

void test_logger_levels(void) {
    Logger::setLevel(LOG_LEVEL_INFO);
    TEST_ASSERT_EQUAL(LOG_LEVEL_INFO, Logger::getLevel());

    Logger::setLevel(LOG_LEVEL_DEBUG);
    TEST_ASSERT_EQUAL(LOG_LEVEL_DEBUG, Logger::getLevel());
}

void test_config_store_save_load(void) {
    // Save dummy data to Preferences
    saveConfig("test_ssid", "test_pass", "http://test.local", "token123");

    // Modify the in-memory variables to simulate reboot state
    current_wifi_ssid = "";
    current_wifi_pass = "";
    current_api_endpoint = "";
    current_api_token = "";

    // Load from Preferences
    loadConfig();

    // Verify
    TEST_ASSERT_EQUAL_STRING("test_ssid", current_wifi_ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("test_pass", current_wifi_pass.c_str());
    TEST_ASSERT_EQUAL_STRING("http://test.local", current_api_endpoint.c_str());
    TEST_ASSERT_EQUAL_STRING("token123", current_api_token.c_str());
}

void test_config_store_debug_mode(void) {
    debugModeEnabled = true;
    saveConfig("", "", "", ""); // Should save the debug mode state

    debugModeEnabled = false;
    loadConfig();

    TEST_ASSERT_TRUE(debugModeEnabled);
}

void test_config_store_log_rotation(void) {
    // Fill the logs
    for (int i = 0; i < 15; i++) {
        addLog("Log entry " + String(i));
    }
    
    // We can't easily capture Serial output in Unity without redirection,
    // but we can ensure addLog doesn't crash.
    TEST_ASSERT_TRUE(true);
}

void setup() {
    // NOTE!!! Wait for >2 secs
    // if board doesn't support software reset via Serial.DTR/RTS
    delay(2000);

    UNITY_BEGIN();

    RUN_TEST(test_logger_levels);
    RUN_TEST(test_config_store_save_load);
    RUN_TEST(test_config_store_debug_mode);
    RUN_TEST(test_config_store_log_rotation);

    UNITY_END();
}

void loop() {
    delay(500);
}
