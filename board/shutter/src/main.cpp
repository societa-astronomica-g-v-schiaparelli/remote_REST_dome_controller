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

////////// global vars (init vars in "global_definitions.hpp")

bool blink_led_loop{true};

SemaphoreHandle_t xSemaphore{xSemaphoreCreateMutex()};

bool hardware_alert_status{};
char hardware_alert_status_description[ALERT_STATUS_DESCRIPTION_SIZE]{};
unsigned long start_movement_time{millis()};

bool network_connection_status{false};
bool network_alert_status{false};

EmergencyProcedure EP_status{EmergencyProcedure::NotNeeded};

AsyncWebServer WebServer{80};
AsyncEventSource SSELogger{"/log_sse"};

//////////

void setup() {
    // serial log
    Serial.begin(115200);

    // board setup
    /* Since ethernet is not needed and modem (GSM or LoRa) is
     * not present on this board, their initialization is disabled. */
    logMessage("setup", "Setup board");
    KMPProDinoESP32.begin(ProDino_ESP32_Ethernet, false, false);
    KMPProDinoESP32.setStatusLed(yellow);

    // EEPROM
    logMessage("setup", "Reading EEPROM");
    EEPROM.begin(512);
    // reset EEPROM if not initialized or if required by the user with the ResetEEPROM request
    if (!EEPROM.readBool(EEPROM_INITIALIZED_ADDRESS)) {
        EEPROM.writeBool(EEPROM_INITIALIZED_ADDRESS, true);
        EEPROM.writeBool(EEPROM_ALERT_STATUS_ADDRESS, false);
        EEPROM.writeString(EEPROM_ALERT_STATUS_DESCRIPTION_ADDRESS, "");
        EEPROM.commit();
    }
    hardware_alert_status = EEPROM.readBool(EEPROM_ALERT_STATUS_ADDRESS);
    EEPROM.readString(EEPROM_ALERT_STATUS_DESCRIPTION_ADDRESS, hardware_alert_status_description, sizeof(hardware_alert_status_description));

    // SPIFFS
    SPIFFS.begin();

    // network
    logMessage("setup", "Setup network");
    // init network services
    connectToWiFi();
    startWebServer();
    startOTA();
    // start network task
    xTaskCreateUniversal(net_task, "net_task", 4096, NULL, 2, NULL, NET_TASK_CORE);

    // end
    logMessage("setup", "End SETUP, starting LOOP");
}

//////////

void loop() {
    delay(100);
    // blink led on/off every two seconds
    if (blink_led_loop) KMPProDinoESP32.processStatusLed(blue, 1000);

    if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
        // handle auto
        if (AUTO) {
            // security control
            if (MOVEMENT_STATUS && (millis() - start_movement_time) > ALERT_STATUS_WAIT) {
                KMPProDinoESP32.setAllRelaysOff();
                EEPROM.writeBool(EEPROM_ALERT_STATUS_ADDRESS, (hardware_alert_status = true));
                if (strcmp(hardware_alert_status_description, "") == 0) {
                    snprintf(hardware_alert_status_description, sizeof(hardware_alert_status_description), "the shutter did not stop within the maximum time");
                    logMessage("loop", String{"ERROR: "} + hardware_alert_status_description);
                    EEPROM.writeString(EEPROM_ALERT_STATUS_DESCRIPTION_ADDRESS, hardware_alert_status_description);
                }
                EEPROM.commit();
                logMessage("loop", "ERROR: alert status");
            }
            // standard handle
            if (MOVEMENT_STATUS && (CLOSED_SENSOR || OPENED_SENSOR)) {
                logMessage("loop", "Stopping shutter...");
                // empirical time to let both motors close or open
                /* DO NOT EXTRACT THIS DELAY FROM THE INSIDE OF THE MUTEX!
                 * The mutex must be acquired to allow the closure (or
                 * aperture) to be fully completed (the sensors become
                 * true BEFORE the complete the full closure or aperture). */
                const unsigned long t{millis()};
                while (AUTO && (millis() - t) < SENSOR_TOGGLING_TIME) delay(50);
                KMPProDinoESP32.setAllRelaysOff();
                logMessage("loop", "Shutter stopped");
            }
        }
        // handle manual
        else if (MOVEMENT_STATUS) {
            KMPProDinoESP32.setAllRelaysOff();
            logMessage("loop", "Shutter moving in manual mode, turning off relays");
        }
        xSemaphoreGive(xSemaphore);
    }
}

//////////

void net_task(void* _parameter) {
    unsigned long time_with_no_network{};
    unsigned long time_last_1{millis()};
    unsigned long time_last_2{time_last_1 - INTERNET_PING_TIME};
    unsigned long time_now{millis()};

    network_connection_status = WIFI_CONNECTED;
    EP_status = AUTO ? (WIFI_CONNECTED ? EmergencyProcedure::NotNeeded : EmergencyProcedure::Waiting) : EmergencyProcedure::Disabled;

    for (;;) {
        delay(100);

        // update board uptime
        uptime::calculateUptime();

        // check wifi status
        if (!WIFI_CONNECTED) {
            logMessage("net_task", "No wifi, trying reconnecting");
            WiFi.disconnect(true, true);
            connectToWiFi();
        }

        // update loop-time
        time_last_1 = time_now;
        time_now = millis();
        const unsigned long dt_1{time_now - time_last_1};

        // handle operations
        if (WIFI_CONNECTED) {
            // handle OTA
            ArduinoOTA.handle();
            // check internet and/or lan
            const unsigned long dt_2{time_now - time_last_2};
            if (dt_2 > INTERNET_PING_TIME) {
                if (Ping.ping(INTERNET_PING_WEBSITE)) {
                    if (AUTO) EP_status = EmergencyProcedure::NotNeeded;
                    // reset vars only if needed
                    if (!network_connection_status) {
                        logMessage("net_task", "Network found, reset timers");
                        network_alert_status = false;
                        network_connection_status = true;
                        time_with_no_network = 0;
                    }
                } else {
                    // ping failed
                    network_connection_status = false;
                    if (AUTO) {
                        time_with_no_network += dt_2;
                        logMessage("net_task", String{"No network (lan/internet) for "} + time_with_no_network / 1000 + " seconds");
                        if (EP_status == EmergencyProcedure::NotNeeded) EP_status = EmergencyProcedure::Waiting;
                    } else {
                        logMessage("net_task", "No network (lan/internet), emergency handling off since shutter in manual mode");
                    }
                }
                time_last_2 = time_now;
            }
        } else {
            // no wifi
            network_connection_status = false;
            if (AUTO) {
                time_with_no_network += dt_1;
                logMessage("net_task", String{"No network (wifi) for "} + time_with_no_network / 1000 + " seconds");
                if (EP_status == EmergencyProcedure::NotNeeded) EP_status = EmergencyProcedure::Waiting;
            } else {
                logMessage("net_task", "No network (wifi), emergency handling off since shutter in manual mode");
            }
        }

        // handle no network
        /* DO NOT INSERT network_alert_status IN THE IF CONDITION
         * In this way, the no-network safety procedure is constantly active:
         * if for any reason the shutter would open (e.g. by putting it in
         * manual mode) the safety procedure will start and close it (obviously
         * only if in automatic mode). */

        // handle manual
        if (!AUTO) {
            EP_status = EmergencyProcedure::Disabled;
            time_with_no_network = 0;
        }

        // handle auto
        else if (!network_connection_status && time_with_no_network > MAX_TIME_NO_NETWORK) {
            if (EP_status == EmergencyProcedure::Waiting) {
                logMessage("net_task", "EP", "NETWORK ERROR: too much time without network -> STARTING EMERGENCY PROCEDURES");
                EP_status = EmergencyProcedure::Running;
            }
            network_alert_status = true;
            if (!CLOSED_SENSOR && !IS_CLOSING && !hardware_alert_status) {
                logMessage("net_task", "EP | shutter", "Closing shutter...");
                EP_status = EmergencyProcedure::Running;
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
                    const unsigned long t{millis()};
                    while (OPENED_SENSOR && AUTO) {
                        delay(50);
                        // safety stop
                        if ((millis() - t) > SENSOR_TOGGLING_TIME) {
                            snprintf(hardware_alert_status_description, sizeof(hardware_alert_status_description), "the opening limit switch sensor did not toggle in time during the closing procedure");
                            logMessage("net_task", "EP | shutter", String{"ERROR: "} + hardware_alert_status_description);
                            EEPROM.writeString(EEPROM_ALERT_STATUS_DESCRIPTION_ADDRESS, hardware_alert_status_description);
                            EEPROM.commit();
                            start_movement_time -= ALERT_STATUS_WAIT;
                            break;
                        }
                    }
                    xSemaphoreGive(xSemaphore);
                }
            }
            if (CLOSED_SENSOR)
                EP_status = EmergencyProcedure::Completed;
            else if (hardware_alert_status)
                EP_status = EmergencyProcedure::Error;
        }
    }
}
