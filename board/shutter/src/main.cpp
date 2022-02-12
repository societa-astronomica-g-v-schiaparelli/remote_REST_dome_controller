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


////////// global vars (init vars in "global_definitions.h")


bool blink_led_loop {true};

SemaphoreHandle_t xSemaphore {xSemaphoreCreateMutex()};

bool alert_status {};
char alert_status_description[ALERT_STATUS_DESCRIPTION_SIZE] {};
unsigned long start_movement_time {millis()};

bool network_connection_status {false};
bool network_alert_status {false};

AsyncWebServer WebServer {80};


//////////


void setup() {
    // serial log
    Serial.begin(115200);

    // board setup
    /* Since ethernet is not needed and modem (GSM or LoRa) is
     * not present on this board, their initialization is disabled. */
    logToSerial("setup", "Setup board");
    KMPProDinoESP32.begin(ProDino_ESP32_Ethernet, false, false);
    KMPProDinoESP32.setStatusLed(yellow);
    KMPProDinoESP32.setAllRelaysOff();

    // EEPROM
    logToSerial("setup", "Reading EEPROM");
    EEPROM.begin(512);
    // reset EEPROM if not initialized or if required by the user with the ResetEEPROM request
    if (!EEPROM.readBool(EEPROM_INITIALIZED_ADDRESS)) {
        EEPROM.writeBool(EEPROM_INITIALIZED_ADDRESS, true);
        EEPROM.writeBool(EEPROM_ALERT_STATUS_ADDRESS, false);
        EEPROM.writeString(EEPROM_ALERT_STATUS_DESCRIPTION_ADDRESS, "");
        EEPROM.commit();
    }
    alert_status = EEPROM.readBool(EEPROM_ALERT_STATUS_ADDRESS);
    EEPROM.readString(EEPROM_ALERT_STATUS_DESCRIPTION_ADDRESS, alert_status_description, sizeof(alert_status_description));

    // SPIFFS
    SPIFFS.begin();

    // network
    logToSerial("setup", "Setup network");
    // init network services
    connectToWiFi();
    startWebServer();
    startOTA();
    // start network task
    xTaskCreateUniversal(net_task, "net_task", 4096, NULL, 2, NULL, -1);

    // end
    logToSerial("setup", "End SETUP, starting LOOP");
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
                EEPROM.writeBool(EEPROM_ALERT_STATUS_ADDRESS, (alert_status = true));
                if (strcmp(alert_status_description, "") == 0) {
                    snprintf(alert_status_description, sizeof(alert_status_description), "the shutter did not stop within the maximum time");
                    logToSerial("loop", String{"ERROR: "} + alert_status_description);
                    EEPROM.writeString(EEPROM_ALERT_STATUS_DESCRIPTION_ADDRESS, alert_status_description);
                }
                EEPROM.commit();
                logToSerial("loop", "ERROR: alert status");
            }
            // standard handle
            if (MOVEMENT_STATUS && (CLOSED_SENSOR || OPENED_SENSOR)) {
                logToSerial("loop", "Stopping shutter...");
                // empirical time to let both motors close or open
                /* DO NOT EXTRACT THIS DELAY FROM THE INSIDE OF THE MUTEX!
                 * The mutex must be acquired to allow the closure (or
                 * aperture) to be fully completed (the sensors become
                 * true BEFORE the complete the full closure or aperture). */
                const unsigned long t {millis()};
                while (AUTO && (millis() - t) < SENSOR_TOGGLING_TIME) delay(50);
                KMPProDinoESP32.setAllRelaysOff();
                logToSerial("loop", "Shutter stopped");
            }
        }
        // handle manual
        else if (MOVEMENT_STATUS) {
            logToSerial("loop", "Shutter moving in manual mode, turning off relays");
            KMPProDinoESP32.setAllRelaysOff();
        }
        xSemaphoreGive(xSemaphore);
    }
}


//////////


void net_task(void* _parameter) {
    unsigned long time_with_no_network {};
    unsigned long time_now {millis()};
    unsigned long time_last_1 {millis()};
    unsigned long time_last_2 {millis()};

    network_connection_status = WIFI_CONNECTED;

    for(;;) {
        delay(100);

        // update board uptime
        uptime::calculateUptime();

        // check wifi status
        if (!WIFI_CONNECTED) {
            logToSerial("net_task", "No wifi, trying reconnecting");
            WiFi.disconnect(true, true);
            connectToWiFi();
        }

        // update loop-time
        time_last_1 = time_now;
        time_now = millis();
        const unsigned long dt_1 {time_now - time_last_1};

        // handle operations
        if (WIFI_CONNECTED) {
            // handle OTA
            ArduinoOTA.handle();
            // if no lan or internet, close
            const unsigned long dt_2 {time_now - time_last_2};
            if (dt_2 > INTERNET_PING_TIME) {
                if (Ping.ping(INTERNET_PING_WEBSITE)) {
                    if (!network_connection_status) logToSerial("net_task", "Network found, reset timers");
                    time_with_no_network = 0;
                    network_connection_status = true;
                    network_alert_status = false;
                } else {
                    logToSerial("net_task", String {"No network (lan/internet) for "} + time_with_no_network/1000 + " seconds");
                    time_with_no_network += dt_2;
                    network_connection_status = false;
                }
                time_last_2 = time_now;
            }
        } else {
            logToSerial("net_task", String {"No network (wifi) for "} + time_with_no_network/1000 + " seconds");
            time_with_no_network += dt_1;
            network_connection_status = false;
        }

        // handle no network
        /* DO NOT INSERT network_alert_status IN THE IF CONDITION
         * In this way, the no-network safety procedure is constantly active:
         * if for any reason the shutter should open (e.g. by putting it in
         * manual mode) the safety procedure will start and close it (obviously
         * only if in automatic mode). */
        if (!network_connection_status && time_with_no_network > MAX_TIME_NO_NETWORK && AUTO && !CLOSED_SENSOR && !IS_CLOSING && !alert_status) {
            logToSerial("net_task", "NETWORK ERROR, STARTING EMERGENCY PROCEDURES");
            logToSerial("net_task", "Too much time without network, closing shutter");
            network_alert_status = true;
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
                        logToSerial("net_task", String{"ERROR: "} + alert_status_description);
                        EEPROM.writeString(EEPROM_ALERT_STATUS_DESCRIPTION_ADDRESS, alert_status_description);
                        EEPROM.commit();
                        start_movement_time -= ALERT_STATUS_WAIT;
                        break;
                    }
                }
                xSemaphoreGive(xSemaphore);
            }
        }
    }

    // delete task on exit
    vTaskDelete(NULL);
}
