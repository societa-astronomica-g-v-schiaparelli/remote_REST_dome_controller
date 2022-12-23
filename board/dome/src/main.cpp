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

////////// local vars

#define TIME_SIREN 3000
#define TIME_BUTTON 2000

bool manual_reset_needed{};
bool error_AC_flag{false};
unsigned int error_AC_counter{0};

////////// global vars (init vars in "global_definitions.hpp")

bool blink_led_loop{true};

SemaphoreHandle_t xSemaphore{xSemaphoreCreateMutex()};
SemaphoreHandle_t xSemaphore_rs485{xSemaphoreCreateMutex()};

int current_az{-1};
int target_az{-1};

bool status_park{false};
bool status_finding_park{false};
bool status_finding_zero{false};
bool status_switchboard_ignited{false};

AsyncWebServer WebServer{80};
AsyncEventSource SSELogger{"/log_sse"};

//////////

void setup() {
    // serial log
    Serial.begin(115200);

    // board setup
    /* since ethernet is not needed and modem (GSM or LoRa) is
     * not present on this board, their initialization is disabled */
    logMessage("setup", "Setup board");
    KMPProDinoESP32.begin(ProDino_ESP32_Ethernet, false, false);
    KMPProDinoESP32.setStatusLed(yellow);
    KMPProDinoESP32.rs485Begin(19200);
    customOptoIn.setup(INPUT_PULLUP);

    // EEPROM
    logMessage("setup", "Reading EEPROM");
    EEPROM.begin(512);
    // reset EEPROM if not initialized or if required by the user with the ResetEEPROM request
    if (!EEPROM.readBool(EEPROM_INITIALIZED_ADDRESS)) {
        EEPROM.writeBool(EEPROM_INITIALIZED_ADDRESS, true);
        EEPROM.writeInt(EEPROM_DOME_POSITION_ADDRESS, PARK_POSITION);
        EEPROM.writeBool(EEPROM_PARK_STATE_ADDRESS, false);
        EEPROM.commit();
    }
    status_park = EEPROM.readBool(EEPROM_PARK_STATE_ADDRESS);
    target_az = current_az = EEPROM.readInt(EEPROM_DOME_POSITION_ADDRESS);

    // SPIFFS
    SPIFFS.begin();

    // network
    logMessage("setup", "Setup network");
    // init network services
    connectToWiFi();
    startWebServer();
    startOTA();
    // start network task
    xTaskCreateUniversal(net_task, "net_task", 4096, NULL, 2, NULL, -1);

    // write current position to encoder
    while (!writePositionToEncoder(current_az)) delay(500);
    // fix status_switchboard_ignited if reboot with switchboard on
    status_switchboard_ignited = SWITCHBOARD_STATUS;
    // set the manual reset flag to the current status
    manual_reset_needed = !AUTO;

    // end
    logMessage("setup", "End SETUP, starting LOOP");
}

//////////

void loop() {
    delay(100);
    // blink led on/off every two seconds
    if (blink_led_loop) KMPProDinoESP32.processStatusLed(blue, 1000);

    //////////////////
    // motion handle

    if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
        // AUTO
        if (AUTO) {
            // enable automatic services
            if (!manual_reset_needed) manual_reset_needed = manualToAuto();

            // AC handle, if dome has no electricity from the main grid, after ~1sec (error_AC_counter >= 10),
            // close the shutter and shutdown. If not, reset the counter and stop the procedure, if needed
            if (!AC_PRESENCE) {
                ++error_AC_counter;
                if (!error_AC_flag && error_AC_counter >= 10) {
                    // TODO put your AC emergency start procedure, example:
                    /* error_AC_flag = httpRequest(BABELE_IP_ADDRESS, R"(/api?json={"cmd":"shutdown"})", 8002).code == 200; */
                    error_AC_flag = true;
                }
            } else {
                if (error_AC_flag) {
                    // TODO put your AC emergency end procedure, example:
                    /* error_AC_flag = !httpRequest(BABELE_IP_ADDRESS, R"(/api?json={"cmd":"abort"})", 8002).code == 200; */
                    error_AC_flag = false;
                }
                if (error_AC_counter > 0) error_AC_counter = 0;
            }

            // handle motion
            if (MOVEMENT_STATUS) {
                logMessage("loop", "Update dome position");
                current_az = domePosition();
                int delta_position{abs(current_az - target_az)};
                delta_position = delta_position < 180 ? delta_position : abs(360 - delta_position);
                if (delta_position < 2) stopSlewing();
            }
            // handle status_finding_zero
            else if (status_finding_zero) {
                logMessage("loop", "Finding zero");
                status_finding_park = false;
                target_az = -1;
                if (status_park) {
                    status_park = false;
                    EEPROM.writeBool(EEPROM_PARK_STATE_ADDRESS, status_park);
                    EEPROM.commit();
                }
                xSemaphoreGive(xSemaphore);
                findZero();
                xSemaphoreTake(xSemaphore, portMAX_DELAY);
            }
        }

        // MANUAL
        else {
            // disable automatic services
            if (manual_reset_needed) manual_reset_needed = !autoToManual();

            // reset motion
            /* since motion is blocking, here it's ok */
            if (MOVEMENT_STATUS) {
                logMessage("loop", "Dome moving in manual mode, turning off relays");
                stopSlewing();
            }

            // reset statuses (high tollerance for park in manual mode)
            if (status_park && abs(current_az - PARK_POSITION) > 5) {
                logMessage("loop", "Dome moved from park position in manual mode, turning off parking flag");
                status_park = false;
                EEPROM.writeBool(EEPROM_PARK_STATE_ADDRESS, status_park);
                EEPROM.commit();
            } else if (!status_park && abs(current_az - PARK_POSITION) <= 5) {
                logMessage("loop", "Dome moved in park position in manual mode, turning on parking flag");
                status_park = true;
                EEPROM.writeBool(EEPROM_PARK_STATE_ADDRESS, status_park);
                EEPROM.commit();
            }
            if (status_finding_park) {
                logMessage("loop", "Dome in park in manual mode, turning off park flag");
                status_finding_park = false;
            }
            if (status_finding_zero) {
                logMessage("loop", "Dome in park in manual mode, turning off finding zero flag");
                status_finding_zero = false;
            }

            // move clockwise
            if (SWITCHBOARD_STATUS && buttonPressed(MAN_CW_O, TIME_BUTTON)) {
                logMessage("loop", "Start clockwise motion");
                startMotion(DomeDirection::CW);  // startSlewing requires target azimuth, so use startMotion
                do {
                    delay(100);
                    current_az = domePosition();
                } while (MAN_CW && SWITCHBOARD_STATUS);
                stopSlewing();  // stopMotion only stops relays, so use stopSlewing to also save states
                logMessage("loop", "End clockwise motion");
            }

            // move anticlockwise
            else if (SWITCHBOARD_STATUS && buttonPressed(MAN_CCW_O, TIME_BUTTON)) {
                logMessage("loop", "Start counterclockwise motion");
                startMotion(DomeDirection::CCW);  // startSlewing requires target azimuth, so use startMotion
                do {
                    delay(100);
                    current_az = domePosition();
                } while (MAN_CCW && SWITCHBOARD_STATUS);
                stopSlewing();  // stopMotion only stops relays, so use stopSlewing to also save states
                logMessage("loop", "End counterclockwise motion");
            }

            // ignite switchboard
            else if (!SWITCHBOARD_STATUS && buttonPressed(MAN_IGNITION_O, TIME_SIREN)) {
                logMessage("loop", "Ignite switchboard");
                switchboardIgnition();
                status_switchboard_ignited = true;
            }
        }

        //////////
        // other

        // switchboard off: shutdown
        if (status_switchboard_ignited && !SWITCHBOARD_STATUS) {
            logMessage("loop", "Switchboard off: shutdown");
            shutDown();
            status_switchboard_ignited = false;
        }

        xSemaphoreGive(xSemaphore);
    }
}

//////////

void net_task(void* _parameter) {
    for (;;) {
        delay(100);

        // update board uptime
        uptime::calculateUptime();

        // handle no wifi connection
        if (!WIFI_CONNECTED) {
            logMessage("net_task", "No wifi, trying reconnecting");
            WiFi.disconnect(true, true);
            connectToWiFi();
        }

        // handle operations
        if (WIFI_CONNECTED) {
            // handle OTA
            ArduinoOTA.handle();
        }
    }
}
