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


////////// local vars


#define TIME_SIREN 3000
#define TIME_BUTTON 2000

#define ENABLE_ERROR_AC false
#if ENABLE_ERROR_AC
    bool error_AC_flag {false};
    bool error_AC_dome {true};
    bool error_AC_domeok {false};
    bool error_AC_shutter {true};
    bool error_AC_shutterok {false};
    bool error_AC_completed[] {false, false, false, false, false};
    StaticJsonDocument<32> error_AC_jsonfilter {};
#endif



////////// global vars (init vars in "global_definitions.h")


bool blink_led_loop {true};

SemaphoreHandle_t xSemaphore {xSemaphoreCreateMutex()};
SemaphoreHandle_t xSemaphore_rs485 {xSemaphoreCreateMutex()};

int current_az {-1};
int target_az {-1};

bool status_park {false};
bool status_finding_park {false};
bool status_finding_zero {false};
bool status_switchboard_ignited {false};

bool error_state {false};

AsyncWebServer WebServer {80};


//////////


void setup() {
    // serial log
    Serial.begin(115200);

    // board setup
    /* since ethernet is not needed and modem (GSM or LoRa) is
     * not present on this board, their initialization is disabled */
    logToSerial("setup", "Setup board");
    KMPProDinoESP32.begin(ProDino_ESP32_Ethernet, false, false);
    KMPProDinoESP32.setStatusLed(yellow);
    KMPProDinoESP32.setAllRelaysOff();
    KMPProDinoESP32.rs485Begin(19200);
    customOptoIn.setup();

    // EEPROM
    logToSerial("setup", "Reading EEPROM");
    EEPROM.begin(512);
    // reset EEPROM if not initialized or if required by the user with the ResetEEPROM request
    if (!EEPROM.readBool(EEPROM_INITIALIZED_ADDRESS)) {
        EEPROM.writeBool(EEPROM_INITIALIZED_ADDRESS, true);
        EEPROM.writeInt(EEPROM_DOME_POSITION_ADDRESS, PARK_POSITION);
        EEPROM.writeBool(EEPROM_ERROR_STATE_ADDRESS, false);
        EEPROM.writeBool(EEPROM_PARK_STATE_ADDRESS, false);
        EEPROM.commit();
    }
    error_state = EEPROM.readBool(EEPROM_ERROR_STATE_ADDRESS);
    status_park = EEPROM.readBool(EEPROM_PARK_STATE_ADDRESS);
    // write current position to encoder
    target_az = current_az = EEPROM.readInt(EEPROM_DOME_POSITION_ADDRESS);
    writePositionToEncoder(current_az);

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

    // fix status_switchboard_ignited if reboot with switchboard on
    status_switchboard_ignited = SWITCHBOARD_STATUS;
#if ENABLE_ERROR_AC
    // setup no AC variables
    error_AC_jsonfilter["rsp"]["shutter-status"] = true;
#endif

    // end
    logToSerial("setup", "End SETUP, entering into LOOP");
}


//////////


void loop() {
    delay(100);
    // blink led on/off every two seconds
    if (blink_led_loop) KMPProDinoESP32.processStatusLed(blue, 1000);

#if ENABLE_ERROR_AC
    // AC handle, if dome has no electricity from the main grid, close the shutter and shutdown
    if (!AC_PRESENCE) {
        error_AC_flag = true;
        // park dome
        if (error_AC_dome) {
            logToSerial("loop", "NO AC", "Parking dome...");
            if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
                park();
                xSemaphoreGive(xSemaphore);
                error_AC_dome = false
            }
        } else if (!error_AC_domeok && !MOVEMENT_STATUS && status_park) {
            logToSerial("loop", "NO AC", "Dome parked");
            error_AC_domeok = true;
            shutDown();
        }
        // close shutter
        if (error_AC_shutter) {
            logToSerial("loop", "NO AC", "Retry closing shutter...");
            StaticJsonDocument<64> json {};
            if (!httpRequest(json, "192.168.41.85", R"(/api?json={"cmd":"close"})")) {
                logToSerial("loop", "NO AC", "Request error, communication failed");
            } else if (json["rsp"].as<String>() != "done") {
                logToSerial("loop", "NO AC", "Shutter error: " + json["rsp"].as<String>());
            } else {
                error_AC_shutter = false;
            }
        } else if (!error_AC_shutterok) {
            StaticJsonDocument<68> json {};
            if (!httpRequest(json, error_AC_jsonfilter, "192.168.41.85", R"(/api?json={"cmd":"status"})")) {
                logToSerial("loop", "NO AC", "Shutter request error, communication failed");
            } else if (json["rsp"]["shutter-status"].as<ShutterStatus>() == ShutterClosed) {
                logToSerial("loop", "NO AC", "Shutter closed");
                error_AC_shutterok = true;
            }
        }
        // shutdown telescope
        if (!error_AC_completed[1]) {
            error_AC_completed[1] = httpRequest("192.168.40.87", "/io.cgi?DOI6=0").code == 200;
            delay(500);
        }
        // handle secondary
        if ()
        // emergency procedure completed, turning off all systems
        if (error_AC_domeok && error_AC_shutterok) {
            logToSerial("loop", "NO AC", "Emergency procedure completed, turning off all systems...");
            // shutter
            if (!error_AC_completed[0]) {
                error_AC_completed[0] = httpRequest("192.168.40.87", "/io.cgi?DOI8=0").code == 200;
                delay(500);
            }
            // telescope
            if (!error_AC_completed[1]) {
                error_AC_completed[1] = httpRequest("192.168.40.87", "/io.cgi?DOI6=0").code == 200;
                delay(500);
            }
            // switchboard
            if (!error_AC_completed[2]) {
                error_AC_completed[2] = httpRequest("192.168.40.87", "/io.cgi?DOI1=0").code == 200;
                delay(500);
            }
            // sodoma
            if (!error_AC_completed[3]) {
                if (httpRequest("192.168.40.82", R"(/api?json={"cmd":"poweroff_server"})", 8000).code == 200) delay(20000);
                error_AC_completed[3] = httpRequest("192.168.40.87", "/io.cgi?DOI5=0"). code == 200;
                delay(500);
            }
            // gomorra
            if (!error_AC_completed[4]) {
                if (httpRequest("192.168.40.83", R"(/api?json={"cmd":"poweroff_server"})", 8000).code == 200) delay(20000);
                error_AC_completed[4] = httpRequest("192.168.40.87", "/io.cgi?DOI4=0"). code == 200;
                delay(500);
            }
        }

    }
    // AC ok, reset errors
    else if (error_AC_flag) {
        logToSerial("loop", "NO AC", "AC ok, resetting errors");
        error_AC_dome = error_AC_shutter = true;
        error_AC_flag = error_AC_domeok = error_AC_shutterok = false;
        for (auto& device: error_AC_completed) device = false;
    }
#endif

    //////////////////
    // motion handle

    if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(50)) == pdTRUE) {

        // AUTO
        if (AUTO) {

            // handle motion
            if (MOVEMENT_STATUS) {
                logToSerial("loop", "Update dome position");
                current_az = domePosition();
                int delta_position {abs(current_az - target_az)};
                delta_position = delta_position < 180 ? delta_position : abs(360 - delta_position);
                if (delta_position < 2) stopSlewing();
            }

            // handle status_finding_zero
            else if (status_finding_zero) {
                logToSerial("loop", "Finding zero");
                xSemaphoreGive(xSemaphore);
                findZero();
                xSemaphoreTake(xSemaphore, portMAX_DELAY);
            }
        }

        // MANUAL
        else {
            // reset motion
            /* since motion is blocking, here it's ok */
            if (MOVEMENT_STATUS) {
                logToSerial("loop", "Dome moving in manual mode, turning off relays");
                stopSlewing();
            }

            // reset statuses (high tollerance for park in manual mode)
            if (status_park && abs(current_az-PARK_POSITION) > 5) {
                logToSerial("loop", "Dome moved from park position in manual mode, turning off parking flag");
                status_park = false;
                EEPROM.writeBool(EEPROM_PARK_STATE_ADDRESS, status_park);
                EEPROM.commit();
            } else if (!status_park && abs(current_az-PARK_POSITION) <= 5) {
                logToSerial("loop", "Dome moved in park position in manual mode, turning on parking flag");
                status_park = true;
                EEPROM.writeBool(EEPROM_PARK_STATE_ADDRESS, status_park);
                EEPROM.commit();
            }
            if (status_finding_park) {
                logToSerial("loop", "Dome in park in manual mode, turning off park flag");
                status_finding_park = false;
            }
            if (status_finding_zero) {
                logToSerial("loop", "Dome in park in manual mode, turning off finding zero flag");
                status_finding_zero = false;
            }

            // move clockwise
            if (SWITCHBOARD_STATUS && buttonPressed(MAN_CW_O, TIME_BUTTON)) {
                logToSerial("loop", "Start clockwise motion");
                startMotion(CW_DIRECTION); // startSlewing requires target azimuth, so use startMotion
                do {
                    delay(100);
                    current_az = domePosition();
                } while (MAN_CW && SWITCHBOARD_STATUS);
                stopSlewing(); // stopMotion only stops relays, so use stopSlewing to also save states
                logToSerial("loop", "End clockwise motion");
            }

            // move anticlockwise
            else if (SWITCHBOARD_STATUS && buttonPressed(MAN_CCW_O, TIME_BUTTON)) {
                logToSerial("loop", "Start counterclockwise motion");
                startMotion(CCW_DIRECTION); // startSlewing requires target azimuth, so use startMotion
                do{
                    delay(100);
                    current_az = domePosition();
                } while (MAN_CCW && SWITCHBOARD_STATUS);
                stopSlewing(); // stopMotion only stops relays, so use stopSlewing to also save states
                logToSerial("loop", "End counterclockwise motion");
            }

            // ignite switchboard
            else if (!SWITCHBOARD_STATUS && buttonPressed(MAN_IGNITION_O, TIME_SIREN)) {
                logToSerial("loop", "Ignite switchboard");
                switchboardIgnition();
                status_switchboard_ignited = true;
            }
        }

        //////////
        // other

        // switchboard off: shutdown
        if (status_switchboard_ignited && !SWITCHBOARD_STATUS) {
            logToSerial("loop", "Switchboard off: shutdown");
            shutDown();
            status_switchboard_ignited = false;
        }

        xSemaphoreGive(xSemaphore);
    }
}


//////////


void net_task(void* _parameter) {
    for(;;) {
        delay(100);

        // update board uptime
        uptime::calculateUptime();

        // handle no wifi connection
        if (!WIFI_CONNECTED) {
            logToSerial("net_task", "No wifi, trying reconnecting");
            WiFi.disconnect(true, true);
            connectToWiFi();
        }

        // handle operations
        if (WIFI_CONNECTED) {
            // handle OTA
            ArduinoOTA.handle();
        }
    }

    // delete task on exit
    vTaskDelete(NULL);
}
