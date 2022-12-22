/*
Remote REST dome controller
https://github.com/societa-astronomica-g-v-schiaparelli/remote_REST_dome_controller

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2020-2022, Società Astronomica G. V. Schiaparelli <https://www.astrogeo.va.it/>.
Authors: Paolo Galli <paolo.galli@astrogeo.va.it>
         Luca Ghirotto <luca.ghirotto@astrogeo.va.it>

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "global_definitions.hpp"

//////////

void connectToWiFi() {
    logMessage("WiFi", "Connecting to wifi...");
    WiFi.mode(WIFI_STA);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(HOSTNAME);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    // disable power saving because it has a HEAVY impact on network performance and reliability
    esp_wifi_set_ps(WIFI_PS_NONE);
    // connect and save connection result
    const uint8_t wifi_status{WiFi.waitForConnectResult()};
    logMessage("WiFi", (static_cast<wl_status_t>(wifi_status) == WL_CONNECTED) ? "Wifi connected" : (String{"No wifi: "} + wifi_status));
    logMessage("WiFi", "Hostname: " HOSTNAME);
    logMessage("WiFi", "MAC: " + WiFi.macAddress());
    logMessage("WiFi", "IP: " + WiFi.localIP().toString());
}

//////////

void flashLed(const uint32_t &_color, const int &_delayInterval) {
    KMPProDinoESP32.setStatusLed(_color);
    delay(_delayInterval);
    KMPProDinoESP32.offStatusLed();
}

//////////

ShutterStatus getShutterStatus() {
    if (IS_OPENING)
        return ShutterStatus::Opening;
    else if (IS_CLOSING)
        return ShutterStatus::Closing;
    else if (OPENED_SENSOR)
        return ShutterStatus::Opened;
    else if (CLOSED_SENSOR)
        return ShutterStatus::Closed;
    else
        return ShutterStatus::PartiallyOpened;
}

//////////

// semaphore for serial log, to avoid serial concurrent writing
SemaphoreHandle_t xSemaphore_log{xSemaphoreCreateMutex()};
// buffer for logging messages
char log_message[1024]{};

void logMessage(const char *_identifier, const char *_msg) {
    if (xSemaphoreTake(xSemaphore_log, pdMS_TO_TICKS(50)) != pdTRUE) return;
    snprintf(log_message, sizeof(log_message), "[%s] %s", _identifier, _msg);
    Serial.println(log_message);
    SSELogger.send(log_message);
    xSemaphoreGive(xSemaphore_log);
}

void logMessage(const char *_identifier, const String &_msg) {
    logMessage(_identifier, _msg.c_str());
}

template <typename T, LOGMESSAGE_ENABLEIF(T)>
void logMessage(const char *_identifier, const T &_msg) {
    logMessage(_identifier, String{_msg});
}

void logMessage(const char *_identifier1, const char *_identifier2, const char *_msg) {
    if (xSemaphoreTake(xSemaphore_log, pdMS_TO_TICKS(50)) != pdTRUE) return;
    snprintf(log_message, sizeof(log_message), "[%s] (%s) %s", _identifier1, _identifier2, _msg);
    Serial.println(log_message);
    SSELogger.send(log_message);
    xSemaphoreGive(xSemaphore_log);
}

void logMessage(const char *_identifier1, const char *_identifier2, const String &_msg) {
    logMessage(_identifier1, _identifier2, _msg.c_str());
}

template <typename T, LOGMESSAGE_ENABLEIF(T)>
void logMessage(const char *_identifier1, const char *_identifier2, const T &_msg) {
    logMessage(_identifier1, _identifier2, String{_msg});
}

void logMessage(const char *_identifier1, const String &_identifier2, const char *_msg) {
    logMessage(_identifier1, _identifier2.c_str(), _msg);
}

void logMessage(const char *_identifier1, const String &_identifier2, const String &_msg) {
    logMessage(_identifier1, _identifier2.c_str(), _msg.c_str());
}

template <typename T, LOGMESSAGE_ENABLEIF(T)>
void logMessage(const char *_identifier1, const String &_identifier2, const T &_msg) {
    logMessage(_identifier1, _identifier2.c_str(), String{_msg});
}

template <typename T, LOGMESSAGE_ENABLEIF(T)>
void logMessage(const char *_identifier1, const T &_identifier2, const char *_msg) {
    logMessage(_identifier1, String{_identifier2}, _msg);
}

template <typename T, LOGMESSAGE_ENABLEIF(T)>
void logMessage(const char *_identifier1, const T &_identifier2, const String &_msg) {
    logMessage(_identifier1, String{_identifier2}, _msg.c_str());
}

template <typename T, typename V, LOGMESSAGE_ENABLEIF(T), LOGMESSAGE_ENABLEIF(V)>
void logMessage(const char *_identifier1, const T &_identifier2, const V &_msg) {
    logMessage(_identifier1, String{_identifier2}, String{_msg});
}

//////////

void startOTA() {
    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

#define OTA_TYPE (ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem")
    ArduinoOTA
        .onStart([]() {
            blink_led_loop = false;
            if (ArduinoOTA.getCommand() == U_SPIFFS) SPIFFS.end();
            logMessage("ArduinoOTA", OTA_TYPE, "Start updating");
        })
        .onProgress([](unsigned int progress, unsigned int total) {
            KMPProDinoESP32.processStatusLed(green, 200);
            static int last_percent{-1};
            const unsigned int new_percent{progress / (total / 100)};
            if (new_percent != last_percent) {
                logMessage("ArduinoOTA", OTA_TYPE, String{"Upload progress: "} + new_percent + "%");
                last_percent = new_percent;
            }
        })
        .onEnd([]() {
            blink_led_loop = true;
            if (ArduinoOTA.getCommand() == U_SPIFFS) SPIFFS.begin();
            logMessage("ArduinoOTA", OTA_TYPE, "End");
        })
        .onError([](ota_error_t error) {
            if (ArduinoOTA.getCommand() == U_SPIFFS) SPIFFS.begin();
            String rsp{"Error: "};
            if (error == OTA_AUTH_ERROR)
                rsp += "auth failed";
            else if (error == OTA_BEGIN_ERROR)
                rsp += "begin failed";
            else if (error == OTA_CONNECT_ERROR)
                rsp += "connect failed";
            else if (error == OTA_RECEIVE_ERROR)
                rsp += "receive failed";
            else if (error == OTA_END_ERROR)
                rsp += "end failed";
            else
                rsp += "unknown error";
            logMessage("ArduinoOTA", OTA_TYPE, rsp);
            flashLed(red, 2000);
            blink_led_loop = true;
        });

    ArduinoOTA.begin();
}
