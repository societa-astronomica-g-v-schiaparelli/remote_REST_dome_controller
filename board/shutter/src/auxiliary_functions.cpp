/*
Remote REST dome controller
https://github.com/societa-astronomica-g-v-schiaparelli/remote_REST_dome_controller

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2020-2022, Società Astronomica G. V. Schiaparelli <https://www.astrogeo.va.it/>.
Authors: Paolo Galli <paolo97gll@gmail.com>
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


#include "global_definitions.h"


//////////


void connectToWiFi() {
    logToSerial("WiFi", "Connecting to wifi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setHostname(HOSTNAME);
    // disable power saving because it has a HEAVY impact on network performance and reliability
    esp_wifi_set_ps(WIFI_PS_NONE);
    // connect and save connection result
    const uint8_t wifi_status {WiFi.waitForConnectResult()};
    logToSerial("WiFi", (static_cast<wl_status_t>(wifi_status) == WL_CONNECTED) ? "Wifi connected" : (String {"No wifi: "} + wifi_status));
    logToSerial("WiFi", "Hostname: " HOSTNAME);
    logToSerial("WiFi", "MAC: " + WiFi.macAddress());
    logToSerial("WiFi", "IP: " + WiFi.localIP().toString());
}


//////////


void flashLed(const RgbColor &_color, const int &_delayInterval) {
    KMPProDinoESP32.setStatusLed(_color);
    delay(_delayInterval);
    KMPProDinoESP32.offStatusLed();
}


//////////


ShutterStatus getShutterStatus() {
    if (IS_OPENING) return ShutterOpening;
    else if (IS_CLOSING) return ShutterClosing;
    else if (OPENED_SENSOR) return ShutterOpened;
    else if (CLOSED_SENSOR) return ShutterClosed;
    else return ShutterPartiallyOpened;
}


//////////


// semaphore for serial log, to avoid serial concurrent writing
SemaphoreHandle_t xSemaphore_log {xSemaphoreCreateMutex()};
#define LOG_MESSAGE_SIZE 1024
// buffer for logging messages
char log_message[LOG_MESSAGE_SIZE] {};

void logToSerial(const char *_identifier, const char *_msg) {
    if (xSemaphoreTake(xSemaphore_log, pdMS_TO_TICKS(50)) != pdTRUE) return;
    snprintf(log_message, sizeof(log_message), "[%s] %s", _identifier, _msg);
    Serial.println(log_message);
    WebSerial.print(log_message);
    xSemaphoreGive(xSemaphore_log);
}

void logToSerial(const char *_identifier, const String &_msg) {
    logToSerial(_identifier, _msg.c_str());
}

template <typename T, LOGTOSERIAL_ENABLEIF(T)>
void logToSerial(const char *_identifier, const T &_msg) {
    logToSerial(_identifier, String {_msg});
}

void logToSerial(const char *_identifier1, const char *_identifier2, const char *_msg) {
    if (xSemaphoreTake(xSemaphore_log, pdMS_TO_TICKS(50)) != pdTRUE) return;
    snprintf(log_message, sizeof(log_message), "[%s] (%s) %s", _identifier1, _identifier2, _msg);
    Serial.println(log_message);
    WebSerial.print(log_message);
    xSemaphoreGive(xSemaphore_log);
}

void logToSerial(const char *_identifier1, const char *_identifier2, const String &_msg) {
    logToSerial(_identifier1, _identifier2, _msg.c_str());
}

template <typename T, LOGTOSERIAL_ENABLEIF(T)>
void logToSerial(const char *_identifier1, const char *_identifier2, const T &_msg) {
    logToSerial(_identifier1, _identifier2, String {_msg});
}

void logToSerial(const char *_identifier1, const String &_identifier2, const char *_msg) {
    logToSerial(_identifier1, _identifier2.c_str(), _msg);
}

void logToSerial(const char *_identifier1, const String &_identifier2, const String &_msg) {
    logToSerial(_identifier1, _identifier2.c_str(), _msg.c_str());
}

template <typename T, LOGTOSERIAL_ENABLEIF(T)>
void logToSerial(const char *_identifier1, const String &_identifier2, const T &_msg) {
    logToSerial(_identifier1, _identifier2.c_str(), String {_msg});
}

template <typename T, LOGTOSERIAL_ENABLEIF(T)>
void logToSerial(const char *_identifier1, const T &_identifier2, const char *_msg) {
    logToSerial(_identifier1, String {_identifier2}, _msg);
}

template <typename T, LOGTOSERIAL_ENABLEIF(T)>
void logToSerial(const char *_identifier1, const T &_identifier2, const String &_msg) {
    logToSerial(_identifier1, String {_identifier2}, _msg.c_str());
}

template <typename T, typename V, LOGTOSERIAL_ENABLEIF(T), LOGTOSERIAL_ENABLEIF(V)>
void logToSerial(const char *_identifier1, const T &_identifier2, const V &_msg) {
    logToSerial(_identifier1, String {_identifier2}, String {_msg});
}


//////////


void startOTA() {
    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    #define OTA_TYPE (ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem")
    ArduinoOTA
        .onStart([] () {
            blink_led_loop = false;
            if (ArduinoOTA.getCommand() == U_SPIFFS) SPIFFS.end();
            logToSerial("ArduinoOTA", OTA_TYPE, "Start updating");
        })
        .onProgress([] (unsigned int progress, unsigned int total) {
            KMPProDinoESP32.processStatusLed(green, 200);
            static int last_percent {-1};
            const unsigned int new_percent {progress / (total/100)};
            if (new_percent != last_percent) {
                logToSerial("ArduinoOTA", OTA_TYPE, String {"Upload progress: "} + new_percent + "%");
                last_percent = new_percent;
            }
        })
        .onEnd([] () {
            blink_led_loop = true;
            if (ArduinoOTA.getCommand() == U_SPIFFS) SPIFFS.begin();
            logToSerial("ArduinoOTA", OTA_TYPE, "End");
        })
        .onError([] (ota_error_t error) {
            if (ArduinoOTA.getCommand() == U_SPIFFS) SPIFFS.begin();
            String rsp {"Error: "};
            if (error == OTA_AUTH_ERROR) rsp += "auth failed";
            else if (error == OTA_BEGIN_ERROR) rsp += "begin failed";
            else if (error == OTA_CONNECT_ERROR) rsp += "connect failed";
            else if (error == OTA_RECEIVE_ERROR) rsp += "receive failed";
            else if (error == OTA_END_ERROR) rsp += "end failed";
            else rsp += "unknown error";
            logToSerial("ArduinoOTA", OTA_TYPE, rsp);
            flashLed(red, 2000);
            blink_led_loop = true;
        });

    ArduinoOTA.begin();
}
