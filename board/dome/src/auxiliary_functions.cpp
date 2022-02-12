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


////////////////////////////////////////////////////////////////////////////////
// AUXILIARY FUNCTIONS


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


httpResponseSummary httpRequest(const String &_host, const String &_uri, const uint16_t &_port) {
    logToSerial("httpRequest", "Begin request to: " + _host + _uri + " port " + _port);
    HTTPClient http {};
    httpResponseSummary httpResponse {};
    // set timeouts
    http.setConnectTimeout(HTTP_REQUEST_TIMEOUT);
    http.setTimeout(HTTP_REQUEST_TIMEOUT);
    // perform request
    http.begin(_host, _port, _uri);
    httpResponse.body = ((httpResponse.code = http.GET()) == 200) ? http.getString() : "";
    http.end();
    logToSerial("httpRequest", httpResponse.code, (httpResponse.code == 200) ? "Success" : "Error");
    return httpResponse;
}

httpResponseSummary httpRequest(const char *_host, const char *_uri, const uint16_t &_port) {
    return httpRequest(String {_host}, String {_uri}, _port);
}

bool httpRequest(JsonDocument &_json, const String &_host, const String &_uri, const uint16_t &_port) {
    logToSerial("httpRequest", "Begin request to: " + _host + _uri + " port " + _port);
    WiFiClient client {};
    HTTPClient http {};
    http.setConnectTimeout(HTTP_REQUEST_TIMEOUT);
    http.setTimeout(HTTP_REQUEST_TIMEOUT);
    http.useHTTP10(true); // needed for http.getStream() with ArduinoJson
    http.begin(client, _host, _port, _uri);
    const int code {http.GET()};
    ReadBufferingStream bufferedStream {http.getStream(), 64};
    const DeserializationError d_error {deserializeJson(_json, bufferedStream)};
    logToSerial("httpRequest", code, (code == 200 && !d_error) ? "Success" : "Error");
    return code == 200 && !d_error;
}

bool httpRequest(JsonDocument &_json, const char *_host, const char *_uri, const uint16_t &_port) {
    return httpRequest(_json, String {_host}, String {_uri}, _port);
}

bool httpRequest(JsonDocument &_json, const JsonDocument &_filter, const String &_host, const String &_uri, const uint16_t &_port) {
    logToSerial("httpRequest", "Begin request to: " + _host + _uri + " port " + _port);
    WiFiClient client {};
    HTTPClient http {};
    http.setConnectTimeout(HTTP_REQUEST_TIMEOUT);
    http.setTimeout(HTTP_REQUEST_TIMEOUT);
    http.useHTTP10(true); // needed for http.getStream() with ArduinoJson
    http.begin(client, _host, _port, _uri);
    const int code {http.GET()};
    ReadBufferingStream bufferedStream {http.getStream(), 64};
    const DeserializationError d_error {deserializeJson(_json, bufferedStream, DeserializationOption::Filter(_filter))};
    logToSerial("httpRequest", code, (code == 200 && !d_error) ? "Success" : "Error");
    return code == 200 && !d_error;
}

bool httpRequest(JsonDocument &_json, const JsonDocument &_filter, const char *_host, const char *_uri, const uint16_t &_port) {
    return httpRequest(_json, _filter, String {_host}, String {_uri}, _port);
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


std::vector<byte> readFromSerial485() {
    logToSerial("readFromSerial485", "Start reading data");
    int value {KMPProDinoESP32.rs485Read()};
    // since the encoder board is slow, try multiple readings before give up
    for (int i {}; value == -1 && i < 10; ++i) {
        delay(50);
        value = KMPProDinoESP32.rs485Read();
    }
    logToSerial("readFromSerial485", value == -1 ? "No data received" : "Receiving data...");
    std::vector<byte> buffer {};
    if (value != -1) {
        // read data from RS485
        while (value != -1) {
            buffer.push_back(static_cast<byte>(value));
            value = KMPProDinoESP32.rs485Read();
        }
        // response codification for logging
        String strbuff {};
        for (auto i : buffer) strbuff += String {i} + " ";
        logToSerial("readFromSerial485", strbuff);
    }
    return buffer;
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


////////////////////////////////////////////////////////////////////////////////
// DOME FUNCTIONS


//////////////
// LOW LEVEL


bool buttonPressed(const OptoInC &button, const unsigned long &ms) {
    if (customOptoIn.getState(button)) {
        unsigned long time {};
        // check short pression
        time = millis() + 250;
        if (customOptoIn.getState(button)) {
            logToSerial("buttonPressed", static_cast<int>(button), "Button pressed");
            while (customOptoIn.getState(button) && millis() < time) delay(50);
        }
        // long pression
        if (customOptoIn.getState(button)) {
            logToSerial("buttonPressed", static_cast<int>(button), "Long pression detected");
            if (xSemaphoreTake(xSemaphore_rs485, SEMAPHORE_RS485_TIMEOUT) != pdTRUE) {
                logToSerial("buttonPressed", static_cast<int>(button), "Error while acquiring semaphore");
            } else {
                startSiren();
                // check that the pressure lasts as long as required
                time = millis() + ms;
                while (customOptoIn.getState(button)) {
                    delay(50);
                    if (millis() > time) {
                        // the user has pressed the button for more than a specific time
                        xSemaphoreGive(xSemaphore_rs485);
                        stopSiren();
                        logToSerial("buttonPressed", static_cast<int>(button), "Button pression completed");
                        return true;
                    }
                }
                xSemaphoreGive(xSemaphore_rs485);
                stopSiren();
            }
        }
        // early release
        logToSerial("buttonPressed", static_cast<int>(button), "Button released before completion");
    }
    return false;
}


//////////


void switchboardIgnition() {
    logToSerial("switchboardIgnition", "Igniting switchboard");
    KMPProDinoESP32.setRelayState(SWITCHBOARD, true);
    delay(500);
    KMPProDinoESP32.setRelayState(SWITCHBOARD, false);
}


//////////


void startMotion(const DomeDirection &direction) {
    // stop movement if dome is moving
    if (AUTO && MOVEMENT_STATUS) {
        logToSerial("move", "Dome moving, stopping...");
        stopMotion();
        delay(750);
    }
    // start moving
    switch (direction) {
        case CW_DIRECTION:
            logToSerial("move", "Start CW motion");
            KMPProDinoESP32.setRelayState(CW_MOTOR, true);
            break;
        case CCW_DIRECTION:
            logToSerial("move", "Start CCW motion");
            KMPProDinoESP32.setRelayState(CCW_MOTOR, true);
            break;
    }
}


void stopMotion() {
    KMPProDinoESP32.setRelayState(CCW_MOTOR, false);
    KMPProDinoESP32.setRelayState(CW_MOTOR, false);
    logToSerial("stopMotion", "Motion stopped");
}


//////////


void startSiren() {
    KMPProDinoESP32.rs485Write('B');
    logToSerial("startSiren", "Siren playing...");
}


void stopSiren() {
    domePosition();
    logToSerial("stopSiren", "Siren stopped");
}


//////////


void writePositionToEncoder(const int position) {
    // convert to bytes
    char hex_string[5] {}; // 5 because of the terminator \0 (4 character + the terminator)
    snprintf(hex_string, sizeof(hex_string), "%04X", position);
    byte buf[2] {};
    buf[0] = HexToByte(hex_string[0], hex_string[1]);
    buf[1] = HexToByte(hex_string[2], hex_string[3]);
    // log
    char log_buf[32] {};
    snprintf(log_buf, sizeof(log_buf), "Writing: %d, 0x%s", position, hex_string);
    logToSerial("writePositionToEncoder", log_buf);
    // send position
    if (xSemaphoreTake(xSemaphore_rs485, SEMAPHORE_RS485_TIMEOUT) != pdTRUE) {
        logToSerial("writePositionToEncoder", "Error: mutex acquired");
    } else {
        KMPProDinoESP32.rs485Write('W');
        KMPProDinoESP32.rs485Write(buf, sizeof(buf));
        xSemaphoreGive(xSemaphore_rs485);
        logToSerial("writePositionToEncoder", "Done");
    }
}


///////////////
// HIGH LEVEL


int domePosition() {
    logToSerial("domePosition", "Request dome position...");
    // acquire semaphore
    if (xSemaphoreTake(xSemaphore_rs485, SEMAPHORE_RS485_TIMEOUT) != pdTRUE) {
        logToSerial("domePosition", "Error: mutex acquired");
        return -2;
    }
    // request position
    KMPProDinoESP32.rs485Write('R');
    const std::vector<byte> response {readFromSerial485()};
    // give semaphore
    xSemaphoreGive(xSemaphore_rs485);
    // check response
    if (response.empty()) {
        logToSerial("domePosition", "Error: no data recived");
        return -3;
    }
    // convert position to integer
    const int position {response[1] | response[0] << 8};
    logToSerial("domePosition", position);
    return position;
}


//////////


void findZero() {
    logToSerial("findZero", "Start find-zero procedure");
    std::vector<byte> response {};

    if (xSemaphoreTake(xSemaphore_rs485, SEMAPHORE_RS485_TIMEOUT) != pdTRUE) {
        logToSerial("findZero", "Error: mutex acquired");
        return;
    }

    // find zero
    startMotion(CW_DIRECTION);
    KMPProDinoESP32.rs485Write('Z');
    logToSerial("findZero", "Searching zero...");
    do {
        // no delay here since it is in the readFromSerial485 function
        response = readFromSerial485();
        logToSerial("findZero", String {response.empty()} + status_finding_zero);
    } while (response.empty() && status_finding_zero && AUTO);
    stopMotion();
    xSemaphoreGive(xSemaphore_rs485);

    // handle errors
    if (!AUTO) {
        xSemaphoreTake(xSemaphore, portMAX_DELAY);
        status_finding_zero = false;
        logToSerial("findZero", "Manual mode, aborting.");
        xSemaphoreGive(xSemaphore);
        return;
    } else if (!status_finding_zero) {
        logToSerial("findZero", "Zero aborted");
        return;
    } else if (response[0] != 'Z' || response.size() != 3) {
        logToSerial("findZero", "Wrong encoder response");
        return;
    }

    // find zero ok
    xSemaphoreTake(xSemaphore, portMAX_DELAY);
    logToSerial("findZero", "Zero found");
    status_finding_zero = false;
    current_az = response[2] | response[1] << 8;
    if (current_az >= 0 && current_az < 360) {
        EEPROM.writeInt(EEPROM_DOME_POSITION_ADDRESS, current_az);
        EEPROM.commit();
    }
    xSemaphoreGive(xSemaphore);
}


//////////


void shutDown() {
    logToSerial("shutDown", "Shutting down...");

    // turn off relays
    KMPProDinoESP32.setAllRelaysOff();
    logToSerial("shutDown", "All relays off");
    delay(500);

    // save status
    current_az = domePosition();
    if (current_az >= 0 && current_az < 360) {
        EEPROM.writeInt(EEPROM_DOME_POSITION_ADDRESS, current_az);
        EEPROM.commit();
    }
    logToSerial("shutDown", "Dome position saved");
}


//////////


void startSlewing(const int new_target_az) {
    if (!AUTO || !SWITCHBOARD_STATUS || status_finding_zero || new_target_az == target_az)
        return;

    target_az = new_target_az % 360;

    logToSerial("startSlewing", String {"Start slewing to "} + target_az);
    if (status_park) {
        status_park = false;
        EEPROM.writeBool(EEPROM_PARK_STATE_ADDRESS, status_park);
        EEPROM.commit();
    }

    // compute direction
    int delta_position {abs(current_az - target_az)};
    const DomeDirection direction {(target_az < current_az) == (delta_position > 180) ? CW_DIRECTION : CCW_DIRECTION};
    delta_position = delta_position < 180 ? delta_position : abs(360 - delta_position);
    logToSerial("startSlewing", String {"Delta position: "} + delta_position);
    logToSerial("startSlewing", String {"Direction: "} + (direction == CW_DIRECTION ? "CW" : "CCW"));

    // move only if the slew direction is different
    /*if (delta_position < 3) {
        logToSerial("startSlewing", "Start slew not needed, delta under 3 degrees");
    } else*/ if (!MOVEMENT_STATUS || (IS_MOVING_CW && direction == CCW_DIRECTION) || (IS_MOVING_CCW && direction == CW_DIRECTION)) {
        logToSerial("startSlewing", "Start slew needed");
        startMotion(direction);
    } else {
        logToSerial("startSlewing", "Start slew not needed");
    }
}


//////////


void stopSlewing() {
    logToSerial("stopSlewing", "Stop slewing and saving state...");

    // stop and wait for dome drift stabilization
    stopMotion();
    delay(500);

    if (AUTO) {
        status_finding_zero = false;
        if (status_finding_park) {
            status_park = true;
            EEPROM.writeBool(EEPROM_PARK_STATE_ADDRESS, status_park);
            EEPROM.commit();
        }
        status_finding_park = false;
    }

    // save state
    target_az = current_az = domePosition();
    if (current_az >= 0 && current_az < 360) {
        EEPROM.writeInt(EEPROM_DOME_POSITION_ADDRESS, current_az);
        EEPROM.commit();
    }

    logToSerial("stopSlewing", "Done");
}


//////////


void park() {
    status_finding_park = true;
    startSlewing(PARK_POSITION);
}
