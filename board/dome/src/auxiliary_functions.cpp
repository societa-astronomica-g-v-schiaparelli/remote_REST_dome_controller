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

////////////////////////////////////////////////////////////////////////////////
// AUXILIARY FUNCTIONS

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

httpResponseSummary httpRequest(const String &_host, const String &_uri, const uint16_t &_port, const WebRequestMethod &_method, const String &_payload, const bool &_log) {
    if (_log) logMessage("httpRequest", "Begin request to: " + _host + _uri + " port " + _port + " method " + _method);
    HTTPClient http{};
    httpResponseSummary httpResponse{};
    // set timeouts
    http.setConnectTimeout(HTTP_REQUEST_TIMEOUT);
    http.setTimeout(HTTP_REQUEST_TIMEOUT);
    // perform request
    http.begin(_host, _port, _uri);
    switch (_method) {
        case HTTP_PUT:
            httpResponse.body = ((httpResponse.code = http.PUT(_payload)) == 200) ? http.getString() : "";
            break;
        case HTTP_GET:
        default:
            httpResponse.body = ((httpResponse.code = http.GET()) == 200) ? http.getString() : "";
            break;
    }
    http.end();
    if (_log) logMessage("httpRequest", httpResponse.code, (httpResponse.code == 200) ? "Success" : "Error");
    return httpResponse;
}

httpResponseSummary httpRequest(const char *_host, const char *_uri, const uint16_t &_port, const WebRequestMethod &_method, const char *_payload, const bool &_log) {
    return httpRequest(String{_host}, String{_uri}, _port, _method, String{_payload}, _log);
}

bool httpRequest(JsonDocument &_json, const String &_host, const String &_uri, const uint16_t &_port, const WebRequestMethod &_method, const String &_payload, const bool &_log) {
    if (_log) logMessage("httpRequest", "Begin request to: " + _host + _uri + " port " + _port + " method " + _method);
    WiFiClient client{};
    HTTPClient http{};
    // set timeouts
    http.setConnectTimeout(HTTP_REQUEST_TIMEOUT);
    http.setTimeout(HTTP_REQUEST_TIMEOUT);
    // use HTTP 1.0, needed for http.getStream() with ArduinoJson
    http.useHTTP10(true);
    // perform request
    http.begin(client, _host, _port, _uri);
    int code{};
    switch (_method) {
        case HTTP_PUT:
            code = http.PUT(_payload);
            break;
        case HTTP_GET:
        default:
            code = http.GET();
            break;
    }
    ReadBufferingStream bufferedStream{http.getStream(), 64};
    _json.clear();
    const DeserializationError d_error{deserializeJson(_json, bufferedStream)};
    if (_log) logMessage("httpRequest", String{"("} + code + ", " + d_error.code(), (code == 200 && !d_error) ? "Success" : "Error");
    return code == 200 && !d_error;
}

bool httpRequest(JsonDocument &_json, const char *_host, const char *_uri, const uint16_t &_port, const WebRequestMethod &_method, const char *_payload, const bool &_log) {
    return httpRequest(_json, String{_host}, String{_uri}, _port, _method, String{_payload}, _log);
}

bool httpRequest(JsonDocument &_json, const JsonDocument &_filter, const String &_host, const String &_uri, const uint16_t &_port, const WebRequestMethod &_method, const String &_payload, const bool &_log) {
    if (_log) logMessage("httpRequest", "Begin request to: " + _host + _uri + " port " + _port + " method " + _method);
    WiFiClient client{};
    HTTPClient http{};
    // set timeouts
    http.setConnectTimeout(HTTP_REQUEST_TIMEOUT);
    http.setTimeout(HTTP_REQUEST_TIMEOUT);
    // use HTTP 1.0, needed for http.getStream() with ArduinoJson
    http.useHTTP10(true);
    // perform request
    http.begin(client, _host, _port, _uri);
    int code{};
    switch (_method) {
        case HTTP_PUT:
            code = http.PUT(_payload);
            break;
        case HTTP_GET:
        default:
            code = http.GET();
            break;
    }
    ReadBufferingStream bufferedStream{http.getStream(), 64};
    _json.clear();
    const DeserializationError d_error{deserializeJson(_json, bufferedStream, DeserializationOption::Filter(_filter))};
    if (_log) logMessage("httpRequest", String{"("} + code + ", " + d_error.code(), (code == 200 && !d_error) ? "Success" : "Error");
    return code == 200 && !d_error;
}

bool httpRequest(JsonDocument &_json, const JsonDocument &_filter, const char *_host, const char *_uri, const uint16_t &_port, const WebRequestMethod &_method, const char *_payload, const bool &_log) {
    return httpRequest(_json, _filter, String{_host}, String{_uri}, _port, _method, String{_payload}, _log);
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

std::vector<byte> readFromSerial485() {
    logMessage("readFromSerial485", "Start reading data");
    int value{KMPProDinoESP32.rs485Read()};
    // since the encoder board is slow, try multiple readings before give up
    for (int i{}; value == -1 && i < 10; ++i) {
        delay(30);
        value = KMPProDinoESP32.rs485Read();
    }
    logMessage("readFromSerial485", value == -1 ? "No data received" : "Receiving data...");
    std::vector<byte> buffer{};
    if (value != -1) {
        // read data from RS485
        while (value != -1) {
            delay(30);
            buffer.push_back(static_cast<byte>(value));
            value = KMPProDinoESP32.rs485Read();
        }
        // response codification for logging
        String strbuff{};
        for (auto i : buffer) strbuff += String{i} + " ";
        logMessage("readFromSerial485", strbuff);
    }
    return buffer;
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

////////////////////////////////////////////////////////////////////////////////
// DOME FUNCTIONS

//////////////
// LOW LEVEL

bool buttonPressed(const OptoInC &button, const unsigned long &ms) {
    if (customOptoIn.getState(button)) {
        unsigned long time{};
        // check short pression
        time = millis() + 250;
        if (customOptoIn.getState(button)) {
            logMessage("buttonPressed", static_cast<int>(button), "Button pressed");
            while (customOptoIn.getState(button) && millis() < time) delay(50);
        }
        // long pression
        if (customOptoIn.getState(button)) {
            logMessage("buttonPressed", static_cast<int>(button), "Long pression detected");
            if (xSemaphoreTake(xSemaphore_rs485, SEMAPHORE_RS485_TIMEOUT) != pdTRUE) {
                logMessage("buttonPressed", static_cast<int>(button), "Error while acquiring semaphore");
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
                        logMessage("buttonPressed", static_cast<int>(button), "Button pression completed");
                        return true;
                    }
                }
                xSemaphoreGive(xSemaphore_rs485);
                stopSiren();
            }
        }
        // early release
        logMessage("buttonPressed", static_cast<int>(button), "Button released before completion");
    }
    return false;
}

//////////

void switchboardIgnition() {
    logMessage("switchboardIgnition", "Igniting switchboard");
    KMPProDinoESP32.setRelayState(SWITCHBOARD, true);
    delay(500);
    KMPProDinoESP32.setRelayState(SWITCHBOARD, false);
}

//////////

void startMotion(const DomeDirection &direction) {
    // stop movement if dome is moving
    if (AUTO && MOVEMENT_STATUS) {
        logMessage("move", "Dome moving, stopping...");
        stopMotion();
        delay(750);
    }
    // start moving
    switch (direction) {
        case DomeDirection::CW:
            logMessage("move", "Start CW motion");
            KMPProDinoESP32.setRelayState(CW_MOTOR, true);
            break;
        case DomeDirection::CCW:
            logMessage("move", "Start CCW motion");
            KMPProDinoESP32.setRelayState(CCW_MOTOR, true);
            break;
    }
}

void stopMotion() {
    KMPProDinoESP32.setRelayState(CCW_MOTOR, false);
    KMPProDinoESP32.setRelayState(CW_MOTOR, false);
    logMessage("stopMotion", "Motion stopped");
}

//////////

void startSiren() {
    KMPProDinoESP32.rs485Write(static_cast<byte>(0x42));
    logMessage("startSiren", "Siren playing...");
}

void stopSiren() {
    domePosition();  // siren stops at the first new command given, so the dome position request is ok
    logMessage("stopSiren", "Siren stopped");
}

//////////

bool writePositionToEncoder(const int position) {
    // convert to bytes
    char hex_string[5]{};  // 5 because of the terminator \0 (4 character + the terminator)
    snprintf(hex_string, sizeof(hex_string), "%04X", position);
    byte buf[2]{};
    buf[0] = HexToByte(hex_string[0], hex_string[1]);
    buf[1] = HexToByte(hex_string[2], hex_string[3]);
    // log
    char log_buf[32]{};
    snprintf(log_buf, sizeof(log_buf), "Writing: %d, 0x%s", position, hex_string);
    logMessage("writePositionToEncoder", log_buf);
    // send position
    if (xSemaphoreTake(xSemaphore_rs485, SEMAPHORE_RS485_TIMEOUT) != pdTRUE) {
        logMessage("writePositionToEncoder", "Error: mutex acquired");
        return false;
    }
    KMPProDinoESP32.rs485Write(static_cast<byte>(0x57));
    KMPProDinoESP32.rs485Write(buf, sizeof(buf));
    xSemaphoreGive(xSemaphore_rs485);
    logMessage("writePositionToEncoder", "Done");
    return true;
}

///////////////
// HIGH LEVEL

int domePosition() {
    logMessage("domePosition", "Request dome position...");
    // acquire semaphore
    if (xSemaphoreTake(xSemaphore_rs485, SEMAPHORE_RS485_TIMEOUT) != pdTRUE) {
        logMessage("domePosition", "Error: mutex acquired");
        return -2;
    }
    // request position
    KMPProDinoESP32.rs485Write(static_cast<byte>(0x52));
    const std::vector<byte> response{readFromSerial485()};
    // give semaphore
    xSemaphoreGive(xSemaphore_rs485);
    // check response
    if (response.empty()) {
        logMessage("domePosition", "Error: no data recived");
        return -3;
    }
    // convert position to integer
    const int position{response[1] | response[0] << 8};
    logMessage("domePosition", position);
    return position;
}

//////////

void findZero() {
    logMessage("findZero", "Start find-zero procedure");
    std::vector<byte> response{};

    if (xSemaphoreTake(xSemaphore_rs485, SEMAPHORE_RS485_TIMEOUT) != pdTRUE) {
        logMessage("findZero", "Error: mutex acquired");
        return;
    }

    // find zero
    startMotion(DomeDirection::CW);
    KMPProDinoESP32.rs485Write(static_cast<byte>(0x5A));
    logMessage("findZero", "Searching zero...");
    do {
        // no delay here since it is in the readFromSerial485 function
        response = readFromSerial485();
        logMessage("findZero", String{response.empty()} + status_finding_zero);
    } while (response.empty() && status_finding_zero && AUTO);
    stopMotion();
    xSemaphoreGive(xSemaphore_rs485);

    // handle errors
    if (!AUTO) {
        xSemaphoreTake(xSemaphore, portMAX_DELAY);
        status_finding_zero = false;
        logMessage("findZero", "Manual mode, aborting.");
        xSemaphoreGive(xSemaphore);
        return;
    } else if (!status_finding_zero) {
        logMessage("findZero", "Zero aborted");
        return;
    } else if (response[0] != static_cast<byte>(0x5A) || response.size() != 3) {
        logMessage("findZero", "Wrong encoder response");
        return;
    }

    // find zero ok
    xSemaphoreTake(xSemaphore, portMAX_DELAY);
    logMessage("findZero", "Zero found");
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
    logMessage("shutDown", "Shutting down...");

    // turn off relays
    KMPProDinoESP32.setAllRelaysOff();
    logMessage("shutDown", "All relays off");
    delay(500);

    // save status
    current_az = domePosition();
    if (current_az >= 0 && current_az < 360) {
        EEPROM.writeInt(EEPROM_DOME_POSITION_ADDRESS, current_az);
        EEPROM.commit();
    }
    logMessage("shutDown", "Dome position saved");
}

//////////

void startSlewing(const int new_target_az) {
    if (!AUTO || !SWITCHBOARD_STATUS || status_finding_zero || new_target_az == target_az)
        return;

    target_az = new_target_az % 360;

    logMessage("startSlewing", String{"Start slewing to "} + target_az);
    if (status_park) {
        status_park = false;
        EEPROM.writeBool(EEPROM_PARK_STATE_ADDRESS, status_park);
        EEPROM.commit();
    }

    // compute direction
    int delta_position{abs(current_az - target_az)};
    const DomeDirection direction{(target_az < current_az) == (delta_position > 180) ? DomeDirection::CW : DomeDirection::CCW};
    delta_position = delta_position < 180 ? delta_position : abs(360 - delta_position);
    logMessage("startSlewing", String{"Delta position: "} + delta_position);
    logMessage("startSlewing", String{"Direction: "} + (direction == DomeDirection::CW ? "CW" : "CCW"));

    // move only if the slew direction is different
    /*if (delta_position < 3) {
        logMessage("startSlewing", "Start slew not needed, delta under 3 degrees");
    } else*/
    if (!MOVEMENT_STATUS || (IS_MOVING_CW && direction == DomeDirection::CCW) || (IS_MOVING_CCW && direction == DomeDirection::CW)) {
        logMessage("startSlewing", "Start slew needed");
        startMotion(direction);
    } else {
        logMessage("startSlewing", "Start slew not needed");
    }
}

//////////

void stopSlewing() {
    logMessage("stopSlewing", "Stop slewing and saving state...");

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

    logMessage("stopSlewing", "Done");
}

//////////

void park() {
    status_finding_park = true;
    startSlewing(PARK_POSITION);
}

//////////////////////
// SECURITY FUNCTION

bool autoToManual() {
    logMessage("autoToManual", "Switching systems to manual mode");
    // TODO put your auto-to-manual procedure (e.g. disabling aux services), example:
    /* return httpRequest(BABELE_IP_ADDRESS, R"(/api?json={"cmd":"abort"})", 8002).code == 200 &&
              httpRequest(BABELE_IP_ADDRESS, R"(/api?json={"cmd":"auto","params":{"action":"off"}})", 8002).code == 200 &&
              httpRequest(BABELE_IP_ADDRESS, R"(/api?json={"cmd":"no_network","params":{"action":"off"}})", 8002).code == 200 &&
              httpRequest(BABELE_IP_ADDRESS, R"(/api?json={"cmd":"unfollow"})", 8003).code == 200 &&
              httpRequest(BABELE_IP_ADDRESS, R"(/power/godshand)", 3001).code == 200; */
    return true;
}

//////////

bool manualToAuto() {
    logMessage("manualToAuto", "Switching systems to automatic mode");
    // TODO put your manual-to-auto procedure (e.g. enabling aux services), example:
    /* return httpRequest(BABELE_IP_ADDRESS, R"(/api?json={"cmd":"auto","params":{"action":"on"}})", 8002).code == 200 &&
              httpRequest(BABELE_IP_ADDRESS, R"(/api?json={"cmd":"no_network","params":{"action":"on"}})", 8002).code == 200; */
    return true;
}
