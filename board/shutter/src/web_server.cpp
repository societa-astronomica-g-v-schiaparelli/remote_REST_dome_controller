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

    ////////
    // API

    WebServer.on("/api", HTTP_GET, [] (AsyncWebServerRequest *request) {
        if (request->hasParam("json")) {
            StaticJsonDocument<64> json {};
            String response {};

            // try deserialization
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
            if (!c1) {
                json.clear();
                json["rsp"] = "Error: wrong syntax";
                serializeJson(json, response);
                request->send(400, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), response);
                return;
            }

            // save command, clear json and handle request
            const String command {json["cmd"].as<String>()};
            json.clear();

            /* shutter-related functions */

            if (command == "abort") {
                if (!AUTO) {
                    json["rsp"] = "Error: shutter in manual mode";
                } else if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(50)) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                } else {
                    KMPProDinoESP32.setAllRelaysOff();
                    xSemaphoreGive(xSemaphore);
                    json["rsp"] = "done";
                }
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            else if (command == "close") {
                if (alert_status) {
                    json["rsp"] = "Error: shutter in alert status";
                } else if (network_alert_status) {
                    json["rsp"] = "Error: no network";
                } else if (!AUTO) {
                    json["rsp"] = "Error: shutter in manual mode";
                } else if (getShutterStatus() == ShutterClosed || getShutterStatus() == ShutterClosing) {
                    json["rsp"] = "done";
                } else {
                    json["rsp"] = "done";
                    serializeJson(json, response);
                    request->send(200, "application/json", response);
                    logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
                    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
                        if (MOVEMENT_STATUS) {
                            KMPProDinoESP32.setAllRelaysOff();
                            delay(150);
                        }
                        start_movement_time = millis();
                        KMPProDinoESP32.setRelayState(CLOSING_MOTOR, true);
                        // let the opening sensor change
                        /* DO NOT EXTRACT THIS DELAY FROM THE INSIDE OF THE MUTEX!
                         * The mutex must be acquired in order to block the loop
                         * shutter handle until the sensor is changed. */
                        const unsigned long t {millis()};
                        while (OPENED_SENSOR && AUTO) {
                            delay(50);
                            // safety stop
                            if ((millis() - t) > SENSOR_TOGGLING_TIME) {
                                snprintf(alert_status_description, sizeof(alert_status_description), "the opening limit switch sensor did not toggle in time during the closing procedure");
                                logToSerial("ESPAsyncWebServer", request->url(), String{"ERROR: "} + alert_status_description);
                                EEPROM.writeString(EEPROM_ALERT_STATUS_DESCRIPTION_ADDRESS, alert_status_description);
                                EEPROM.commit();
                                start_movement_time -= ALERT_STATUS_WAIT;
                                break;
                            }
                        }
                        xSemaphoreGive(xSemaphore);
                    }
                    return;
                }
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            else if (command == "open") {
                if (alert_status) {
                    json["rsp"] = "Error: shutter in alert status";
                } else if (network_alert_status) {
                    json["rsp"] = "Error: no network";
                } else if (!AUTO) {
                    json["rsp"] = "Error: shutter in manual mode";
                } else if (getShutterStatus() == ShutterOpened || getShutterStatus() == ShutterOpening) {
                    json["rsp"] = "done";
                } else {
                    json["rsp"] = "done";
                    serializeJson(json, response);
                    request->send(200, "application/json", response);
                    logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
                    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
                        if (MOVEMENT_STATUS) {
                            KMPProDinoESP32.setAllRelaysOff();
                            delay(150);
                        }
                        start_movement_time = millis();
                        KMPProDinoESP32.setRelayState(OPENING_MOTOR, true);
                        // let the closing sensor change
                        /* DO NOT EXTRACT THIS DELAY FROM THE INSIDE OF THE MUTEX!
                         * The mutex must be acquired in order to block the loop
                         * shutter handle until the sensor is changed. */
                        const unsigned long t {millis()};
                        while (CLOSED_SENSOR && AUTO) {
                            delay(50);
                            // safety stop
                            if ((millis() - t) > SENSOR_TOGGLING_TIME) {
                                snprintf(alert_status_description, sizeof(alert_status_description), "the closing limit switch sensor did not toggle in time during the opening procedure");
                                logToSerial("ESPAsyncWebServer", request->url(), String{"ERROR: "} + alert_status_description);
                                EEPROM.writeString(EEPROM_ALERT_STATUS_DESCRIPTION_ADDRESS, alert_status_description);
                                EEPROM.commit();
                                start_movement_time -= ALERT_STATUS_WAIT;
                                break;
                            }
                        }
                        xSemaphoreGive(xSemaphore);
                    }
                    return;
                }
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            /* system management */

            else if (command == "reset-alert-status") {
                if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(50)) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                } else {
                    EEPROM.writeBool(EEPROM_ALERT_STATUS_ADDRESS, (alert_status = false));
                    alert_status_description[0] = 0;
                    EEPROM.writeString(EEPROM_ALERT_STATUS_DESCRIPTION_ADDRESS, alert_status_description);
                    EEPROM.commit();
                    xSemaphoreGive(xSemaphore);
                    json["rsp"] = "done";
                }
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            else if (command == "reset-EEPROM") {
                if (alert_status) {
                    json["rsp"] = "Error: shutter in alert status";
                } else if (MOVEMENT_STATUS) {
                    json["rsp"] = "Error: shutter is moving";
                } else if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(50)) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                } else {
                    // do not give back the semaphore to ensure no critical operation is in progress during the reboot
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
                if (MOVEMENT_STATUS) {
                    json["rsp"] = "Error: shutter is moving";
                } else if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(50)) != pdTRUE) {
                    json["rsp"] = "Error: mutex acquired";
                } else {
                    // take the semaphore to ensure no critical operation is in progress during the reboot
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

            else if (command == "force-restart") {
                json["rsp"] = "done";
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command);
                ESP.restart();
            }

            else if (command == "server-logging-toggle") {
                webserver_logging = !webserver_logging;
                json["rsp"] = "done";
                serializeJson(json, response);
                request->send(200, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response);
            }

            else if (command == "server-logging-status") {
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
                    json_status["rsp"]["shutter-status"] = getShutterStatus();
                    json_status["rsp"]["movement-status"] = MOVEMENT_STATUS;
                    json_status["rsp"]["network-status"] = network_connection_status;
                    json_status["rsp"]["alert"]["alert-status"] = alert_status;
                    json_status["rsp"]["alert"]["alert-status-description"] = alert_status_description;
                    json_status["rsp"]["alert"]["network-alert-status"] = network_alert_status;
                    json_status["rsp"]["relay"]["opening-motor"] = IS_OPENING;
                    json_status["rsp"]["relay"]["closing-motor"] = IS_CLOSING;
                    json_status["rsp"]["optoin"]["auto"] = AUTO;
                    json_status["rsp"]["optoin"]["closed-sensor"] = CLOSED_SENSOR;
                    json_status["rsp"]["optoin"]["opened-sensor"] = OPENED_SENSOR;
                    json_status["rsp"]["wifi"]["hostname"] = HOSTNAME;
                    json_status["rsp"]["wifi"]["mac-address"] = WiFi.macAddress();
                    // serialize and send
                    response_status.clear();
                    serializeJson(json_status, response_status);
                    request->send(200, "application/json", response_status);
                    if (webserver_logging) logToSerial("ESPAsyncWebServer", request->url(), command + ": " + response_status);
                    xSemaphoreGive(xSemaphore_status);
                }
            }

            /* error */

            else {
                json["rsp"] = "Error: unknown command";
                serializeJson(json, response);
                request->send(400, "application/json", response);
                logToSerial("ESPAsyncWebServer", request->url(), response);
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

    // add default CORS header
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

    // begin
    WebServer.begin();
}
