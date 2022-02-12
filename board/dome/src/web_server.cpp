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


#define JSON_S_SIZE 512
// status json, allocated in global stack
StaticJsonDocument<JSON_S_SIZE> json_status {};
// status string for json_status serialization
String response_status {};
// semaphore for status json, to avoid too much allocations
SemaphoreHandle_t xSemaphore_status {xSemaphoreCreateMutex()};

// variable for disabling webserver logging
bool webserver_logging {false};


//////////


void startWebServer() {

    /////////////
    // WEBPAGES

    WebServer.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String clientIP {request->client()->remoteIP().toString()};
        if (!request->authenticate(WEBPAGE_LOGIN_USER, WEBPAGE_LOGIN_PASSWORD))
            return request->requestAuthentication();
        request->send(SPIFFS, "/dashboard.html", "text/html");
        logToSerial("ESPAsyncWebServer", request->url(), "");
    });

    WebServer.on("/dashboard.js", HTTP_GET, [] (AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/dashboard.js", "text/javascript");
        logToSerial("ESPAsyncWebServer", request->url(), "");
    });

    WebServer.on("/webserial", HTTP_GET, [] (AsyncWebServerRequest *request) {
        if (!request->authenticate(WEBPAGE_LOGIN_USER, WEBPAGE_LOGIN_PASSWORD))
            return request->requestAuthentication();
        request->send(SPIFFS, "/webserial.html", "text/html");
        logToSerial("ESPAsyncWebServer", request->url(), "");
    });

    WebServer.on("/webserial.js", HTTP_GET, [] (AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/webserial.js", "text/javascript");
        logToSerial("ESPAsyncWebServer", request->url(), "");
    });

    WebServer.on("/style.css", HTTP_GET, [] (AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/style.css", "text/css");
        logToSerial("ESPAsyncWebServer", request->url(), "");
    });

    ///////////
    // API

    WebServer.on("/api", HTTP_GET, [] (AsyncWebServerRequest *request) {
        if (request->hasParam("json")) {
            StaticJsonDocument<192> json {};
            String response{};

            // try serialization
            const DeserializationError d_error {deserializeJson(json, request->getParam("json")->value())};
            if (d_error) {
                json["rsp"] = "Error: wrong syntax";
                serializeJson(json, response);
                request->send(400, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), response);
                return;
            }

            // check json syntax
            const bool c1 {json.containsKey("cmd") && json["cmd"].is<String>()};
            const bool c2 {json["cmd"] != "slew-to-az" || (json["cmd"] == "slew-to-az" && json.containsKey("az-target"))};
            const bool c3 {json["cmd"] != "encoder-writeconf" || (json["cmd"] == "encoder-writeconf" && json.containsKey("config") && json["config"].is<JsonArrayConst>())};
            if (!(c1 && c2 && c3)) {
                json.clear();
                json["rsp"] = "Error: wrong syntax";
                serializeJson(json, response);
                request->send(400, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), response);
                return;
            }

            // save command, create response json and handle request
            const String command {json["cmd"].as<String>()};

            /* dome-related functions */

            if (command == "abort") {
                json.clear();
                if (!AUTO) {
                    json["rsp"] = "Error: dome in manual mode";
                } else if (!SWITCHBOARD_STATUS) {
                    json["rsp"] = "Error: switchboard off";
                } else if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(300)) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                } else if (!MOVEMENT_STATUS) {
                    json["rsp"] = "done";
                    status_finding_zero = status_finding_park = false;
                    xSemaphoreGive(xSemaphore);
                } else {
                    json["rsp"] = "done";
                    serializeJson(json, response);
                    request->send(200, "application/json", response);
                    logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
                    status_finding_zero = status_finding_park = false;
                    stopSlewing();
                    xSemaphoreGive(xSemaphore);
                    return;
                }
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            else if (command == "slew-to-az") {
                const int new_target_az {json["az-target"].as<int>()};
                json.clear();
                if (!AUTO) {
                    json["rsp"] = "Error: dome in manual mode";
                } else if (!AC_PRESENCE) {
                    json["rsp"] = "Error: no AC";
                } else if (!SWITCHBOARD_STATUS) {
                    json["rsp"] = "Error: switchboard off";
                } else if (status_finding_zero) {
                    json["rsp"] = "Error: finding zero";
                } else if (status_finding_park) {
                    json["rsp"] = "Error: parking";
                } else if (new_target_az < 0 || new_target_az > 360) {
                    json["rsp"] = "Error: target out of bound";
                } else if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(300)) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                } else {
                    json["rsp"] = "done";
                    serializeJson(json, response);
                    request->send(200, "application/json", response);
                    logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
                    startSlewing(new_target_az);
                    xSemaphoreGive(xSemaphore);
                    return;
                }
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            else if (command == "park") {
                json.clear();
                if (!AUTO) {
                    json["rsp"] = "Error: dome in manual mode";
                } else if (!AC_PRESENCE) {
                    json["rsp"] = "Error: no AC";
                } else if (!SWITCHBOARD_STATUS) {
                    json["rsp"] = "Error: switchboard off";
                } else if (status_finding_zero) {
                    json["rsp"] = "Error: finding zero";
                } else if (status_finding_park || status_park) {
                    json["rsp"] = "done";
                } else if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(300)) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                } else if (!MOVEMENT_STATUS && current_az > PARK_POSITION-2 && current_az < PARK_POSITION+2) {
                    json["rsp"] = "done";
                    status_park = true;
                    EEPROM.writeBool(EEPROM_PARK_STATE_ADDRESS, status_park);
                    EEPROM.commit();
                    xSemaphoreGive(xSemaphore);
                } else {
                    json["rsp"] = "done";
                    serializeJson(json, response);
                    request->send(200, "application/json", response);
                    logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
                    park();
                    xSemaphoreGive(xSemaphore);
                    return;
                }
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            else if (command == "find-zero") {
                json.clear();
                if (!AUTO) {
                    json["rsp"] = "Error: dome in manual mode";
                } else if (!AC_PRESENCE) {
                    json["rsp"] = "Error: no AC";
                } else if (!SWITCHBOARD_STATUS) {
                    json["rsp"] = "Error: switchboard off";
                } else if (MOVEMENT_STATUS) {
                    json["rsp"] = "Error: dome moving";
                } else if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(300)) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                } else {
                    json["rsp"] = "done";
                    serializeJson(json, response);
                    request->send(200, "application/json", response);
                    logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
                    status_finding_zero = true;
                    xSemaphoreGive(xSemaphore);
                    return;
                }
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            /* encoder-related functions */

            else if (command == "encoder-readconf") {
                json.clear();
                if (xSemaphoreTake(xSemaphore_rs485, SEMAPHORE_RS485_TIMEOUT) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                    serializeJson(json, response);
                } else {
                    KMPProDinoESP32.rs485Write(static_cast<byte>(0xC1));
                    std::vector<byte> config {readFromSerial485()};
                    xSemaphoreGive(xSemaphore_rs485);
                    StaticJsonDocument<384> json_conf {};
                    for (int i{}; i<config.size(); ++i) json_conf["bytes"][i] = config[i];
                    json_conf["decoded data"]["steps / degree"] = static_cast<int>(config[0]);
                    json_conf["decoded data"]["steps / dome revolution"] = static_cast<int>(config[2] | config[1] << 8);
                    json_conf["decoded data"]["zero azimuth"] = static_cast<int>(config[4] | config[3] << 8);
                    json_conf["decoded data"]["zero offset steps"] = static_cast<int>(config[6] | config[5] << 8);
                    json_conf["decoded data"]["checksum"] = static_cast<int>(config[7]);
                    config.pop_back();
                    byte checksum {};
                    for (auto i: config) checksum += i;
                    checksum = ~checksum + 1;
                    json_conf["validation"] = config[7] == checksum;
                    serializeJson(json_conf, response);
                }
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            else if (command == "encoder-writeconf") {
                const DynamicJsonDocument json_writeconf {json};
                const JsonArrayConst encoder_config {json_writeconf["config"].as<JsonArrayConst>()};
                json.clear();
                bool config_type_error {false};
                for (auto i: encoder_config) if (!i.is<byte>()) config_type_error = true;
                if (AUTO) {
                    json["rsp"] = "Error: dome in automatic mode";
                } else if (!AC_PRESENCE) {
                    json["rsp"] = "Error: no AC";
                } else if (MOVEMENT_STATUS) {
                    json["rsp"] = "Error: dome is moving";
                } else if (config_type_error) {
                    json["rsp"] = "Error: type must be byte (uint8_t)";
                } else if (encoder_config.size() != 7) {
                    json["rsp"] = "Error: config must be of 7 bytes";
                } else {
                    byte config[] {
                        static_cast<byte>(0xC0),
                        encoder_config[0].as<byte>(),
                        encoder_config[1].as<byte>(),
                        encoder_config[2].as<byte>(),
                        encoder_config[3].as<byte>(),
                        encoder_config[4].as<byte>(),
                        encoder_config[5].as<byte>(),
                        encoder_config[6].as<byte>(),
                        static_cast<byte>(0)
                    };
                    byte checksum {};
                    for (auto i: config) checksum += i;
                    checksum = ~checksum + 1;
                    config[8] = checksum;
                    if (xSemaphoreTake(xSemaphore_rs485, SEMAPHORE_RS485_TIMEOUT) != pdTRUE) {
                        json["rsp"] = "Error: mutex acquired";
                    } else {
                        KMPProDinoESP32.rs485Write(config, sizeof(config));
                        xSemaphoreGive(xSemaphore_rs485);
                        json["rsp"] = "done";
                    }
                }
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            else if (command == "encoder-resetconf") {
                json.clear();
                if (AUTO) {
                    json["rsp"] = "Error: dome in automatic mode";
                } else if (!AC_PRESENCE) {
                    json["rsp"] = "Error: no AC";
                } else if (MOVEMENT_STATUS) {
                    json["rsp"] = "Error: dome is moving";
                } else if (xSemaphoreTake(xSemaphore_rs485, SEMAPHORE_RS485_TIMEOUT) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                } else {
                    KMPProDinoESP32.rs485Write('D');
                    xSemaphoreGive(xSemaphore_rs485);
                    json["rsp"] = "done";
                }
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            else if (command == "encoder-disablezero") {
                json.clear();
                if (AUTO) {
                    json["rsp"] = "Error: dome in automatic mode";
                } else if (!AC_PRESENCE) {
                    json["rsp"] = "Error: no AC";
                } else if (MOVEMENT_STATUS) {
                    json["rsp"] = "Error: dome is moving";
                } else if (xSemaphoreTake(xSemaphore_rs485, SEMAPHORE_RS485_TIMEOUT) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                } else {
                    KMPProDinoESP32.rs485Write('#');
                    xSemaphoreGive(xSemaphore_rs485);
                    json["rsp"] = "done";
                }
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            /* system management */

            else if (command == "ignite-switchboard") {
                json.clear();
                if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(300)) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                } else {
                    json["rsp"] = "done";
                    serializeJson(json, response);
                    request->send(200, "application/json", response);
                    logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
                    if (!status_switchboard_ignited) {
                        switchboardIgnition();
                        status_switchboard_ignited = true;
                    }
                    xSemaphoreGive(xSemaphore);
                    return;
                }
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            else if (command == "reset-EEPROM") {
                json.clear();
                if (MOVEMENT_STATUS) {
                    json["rsp"] = "Error: dome is moving";
                } else if (!AC_PRESENCE) {
                    json["rsp"] = "Error: no AC";
                } else if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(300)) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                } else {
                    EEPROM.writeBool(EEPROM_INITIALIZED_ADDRESS, false);
                    EEPROM.commit();
                    json["rsp"] = "done";
                    serializeJson(json, response);
                    request->send(200, "application/json", response);
                    logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
                    ESP.restart();
                    xSemaphoreGive(xSemaphore);
                    return;
                }
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            else if (command == "restart") {
                json.clear();
                if (MOVEMENT_STATUS) {
                    json["rsp"] = "Error: dome is moving";
                } else if (!AC_PRESENCE) {
                    json["rsp"] = "Error: no AC";
                } else if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(300)) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                } else {
                    json["rsp"] = "done";
                    serializeJson(json, response);
                    request->send(200, "application/json", response);
                    logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
                    shutDown();
                    ESP.restart();
                    xSemaphoreGive(xSemaphore);
                    return;
                }
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            else if (command == "force-restart") {
                json.clear();
                json["rsp"] = "done";
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command);
                ESP.restart();
            }

            else if (command == "turn-off") {
                json.clear();
                if (!AUTO) {
                    json["rsp"] = "Error: dome in manual mode";
                } else if (!AC_PRESENCE) {
                    json["rsp"] = "Error: no AC";
                } else if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(300)) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                } else {
                    json["rsp"] = "done";
                    serializeJson(json, response);
                    request->send(200, "application/json", response);
                    logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
                    shutDown();
                    xSemaphoreGive(xSemaphore);
                    return;
                }
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            else if (command == "server-logging-toggle") {
                json.clear();
                webserver_logging = !webserver_logging;
                json["rsp"] = "done";
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            else if (command == "server-logging-status") {
                json.clear();
                json["rsp"] = webserver_logging;
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            /* status */

            else if (command == "status") {
                if (xSemaphoreTake(xSemaphore_status, pdMS_TO_TICKS(50)) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                    serializeJson(json, response);
                    request->send(200, "application/json", response);
                    logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
                } else {
                    // create json
                    json_status.clear();
                    json_status["rsp"]["firmware-version"] = FIRMWARE_VERSION;
                    json_status["rsp"]["uptime"] = uptime_formatter::getUptime();
                    json_status["rsp"]["dome-azimuth"] = status_finding_zero ? -1 : current_az;
                    json_status["rsp"]["target-azimuth"] = target_az;
                    json_status["rsp"]["movement-status"] = MOVEMENT_STATUS;
                    json_status["rsp"]["in-park"] = status_park;
                    json_status["rsp"]["finding-park"] = status_finding_park;
                    json_status["rsp"]["finding-zero"] = status_finding_zero;
                    // TODO implement alert statuses
                    // json_status["rsp"]["alert"][...] = ...;
                    json_status["rsp"]["relay"]["cw-motor"] = IS_MOVING_CW;
                    json_status["rsp"]["relay"]["ccw-motor"] = IS_MOVING_CCW;
                    json_status["rsp"]["relay"]["switchboard"] = KMPProDinoESP32.getRelayState(SWITCHBOARD);
                    json_status["rsp"]["optoin"]["auto"] = customOptoIn.getState(AUTO_O);
                    json_status["rsp"]["optoin"]["switchboard-status"] = SWITCHBOARD_STATUS;
                    json_status["rsp"]["optoin"]["auto-ignition"] = AUTO_IGNITION;
                    json_status["rsp"]["optoin"]["ac-presence"] = AC_PRESENCE;
                    json_status["rsp"]["optoin"]["manual-cw-button"] = MAN_CW;
                    json_status["rsp"]["optoin"]["manual-ccw-button"] = MAN_CCW;
                    json_status["rsp"]["optoin"]["manual-ignition"] = MAN_IGNITION;
                    json_status["rsp"]["wifi"]["hostname"] = HOSTNAME;
                    json_status["rsp"]["wifi"]["mac-address"] = WiFi.macAddress();
                    // clean, serialize and send
                    response_status.clear();
                    serializeJson(json_status, response_status);
                    request->send(200, "application/json", response_status);
                    if (webserver_logging) logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response_status);
                    xSemaphoreGive(xSemaphore_status);
                }
            }

            /* error */

            else {
                json.clear();
                json["rsp"] = "Error: unknown request";
                serializeJson(json, response);
                request->send(400, "application/json", response);
                logToSerial("ESPAsyncWebServer", response);
            }
        }

        else {
            const char response[] {R"({"rsp":"Error: unknown params"})"};
            request->send_P(400, "application/json", response);
            logToSerial("ESPAsyncWebServer", request->url(), response);
        }
    });

    //////////
    // OTHER

    WebSerial.begin(&WebServer);

    WebServer.onNotFound([] (AsyncWebServerRequest *request) {
        const char response[] {R"({"rsp":"Error: not found"})"};
        request->send_P(404, "application/json", response);
        logToSerial("ESPAsyncWebServer", request->url(), response);
    });

    //////////
    // BEGIN

    // prepare status string
    response_status.reserve(JSON_S_SIZE);

    // add header
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

    WebServer.begin();
}
