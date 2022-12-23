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

#ifndef _GLOBAL_DEFINITIONS_HPP_
#define _GLOBAL_DEFINITIONS_HPP_

//////////

/* CHANGELOG (main differences)
 *
 * beta
 * - v0.1.0: first "final" working version for prodino
 * - v0.1.1: add internet check
 * - v0.1.2: park telescope if no internet and better emergency handling
 * - v0.1.3: add software locking
 * - v0.1.4: code refactor
 * - v0.1.5: disable emergency procedure if manual mode
 * - v0.1.6: remove park telescope if no internet feature since advanced
 *           internet handling is now on the main server
 * - v0.1.7: sweetalert2 implemented
 * - v0.1.8: new ProDinoESP32 library with new dependencies
 *
 * stable
 * - v1.0.0: first stable version
 * - v1.0.1: fix version number
 * - v1.0.2: better webpage css
 * - v1.0.3: send telegram notification on hw alert and library update */
#define FIRMWARE_VERSION "v1.0.3"

//////////

/* Since ethernet is not needed, force the definition of UTIL_H to disable the
 * definition of not needed functions from the library "Ethernet.h" included
 * by "KMPProDinoESP32.h", in particular by:
 *   -> lib\ProDinoESP32\src\Ethernet\utility\w5100.h:456
 * Indeed, these functions have conflicts with the same functions defined in
 * the native framework-arduinoespressif32 (used for example by <WiFi.h> or
 * <ArduinoOTA.h>).
 * WARNING: IF YOU WANT TO ENABLE ETHERNET, CHECK CAREFULLY THIS THING! */
#define UTIL_H
#include "KMPProDinoESP32.h"

/* Keep ESPAsyncWebServer on a random core since the net_task is more important. */
#define CONFIG_ASYNC_TCP_RUNNING_CORE -1

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP32Ping.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <uptime.h>
#include <uptime_formatter.h>

////////////////////////////////////////////////////////////////////////////////
// VARIABLES

// handle led blink during normal operations or OTA
extern bool blink_led_loop;

// allowed types for logMessage template (types list from String constructor)
#define LOGMESSAGE_ENABLEIF(T) typename std::enable_if<                         \
                                   std::is_same<T, char>::value ||              \
                                       std::is_same<T, unsigned char>::value || \
                                       std::is_same<T, int>::value ||           \
                                       std::is_same<T, unsigned int>::value ||  \
                                       std::is_same<T, long>::value ||          \
                                       std::is_same<T, unsigned long>::value || \
                                       std::is_same<T, float>::value ||         \
                                       std::is_same<T, double>::value ||        \
                                       std::is_same<T, uint8_t>::value,         \
                                   int>::type = 0

/* Global semaphore, needed to safely terminating critical operations and ensure
 * multithread code syncing. Ensure no concurrent writing on variables and no
 * concurrent operations on relays and EEPROM; is also used to lock parts of code
 * during critical operations such as the final stages of the motion. */
extern SemaphoreHandle_t xSemaphore;

//////////

// shutter opening motor relay
#define OPENING_MOTOR Relay::Relay1
#define IS_OPENING KMPProDinoESP32.getRelayState(OPENING_MOTOR)
// shutter closing motor relay
#define CLOSING_MOTOR Relay::Relay2
#define IS_CLOSING KMPProDinoESP32.getRelayState(CLOSING_MOTOR)

// indicate if the shutter is moving due to an automatic command, aka if any relays is on
#define MOVEMENT_STATUS (IS_OPENING || IS_CLOSING)

// automatic-manual switch status
#define AUTO KMPProDinoESP32.getOptoInState(OptoIn::OptoIn1)
// closing limit switch sensor
#define CLOSED_SENSOR KMPProDinoESP32.getOptoInState(OptoIn::OptoIn2)
// opening limit switch sensor
#define OPENED_SENSOR KMPProDinoESP32.getOptoInState(OptoIn::OptoIn3)

// empirical time to let both motors close or open, aka the time between toggle and real open/close
#define SENSOR_TOGGLING_TIME 4500
// max time to wait before trigger the alert status
#define ALERT_STATUS_WAIT 22000
// aux variable to check that the shutter moving time is below ALERT_STATUS_WAIT
extern unsigned long start_movement_time;
// alert status triggered if the shutter do not complete motion in the predicted time, e.g. if a mechanical problem occurs
extern bool hardware_alert_status;
#define ALERT_STATUS_DESCRIPTION_SIZE 128
// description of what caused the alert status (array of char to better handle EEPROM writing)
extern char hardware_alert_status_description[ALERT_STATUS_DESCRIPTION_SIZE];

#define INTERNET_PING_WEBSITE "www.google.com"
// time between each ping
#define INTERNET_PING_TIME 60000
// max time to wait without network before the emergency close of the shutter
#define MAX_TIME_NO_NETWORK 600000
// true if connected to wifi and internet is ok, otherwise false
extern bool network_connection_status;
// alert status triggered if no network for too much time
extern bool network_alert_status;

enum class EmergencyProcedure {
    NotNeeded,  // system is ok, EP not needed
    Waiting,    // system warning, waiting to hit the time limit to start EP
    Running,    // EP started, but not terminated
    Completed,  // EP completed, system is secured
    Error,      // critical error, it's impossible to complete EP
    Disabled,   // dome in manual mode, EP disabled
};
// hold the status of the emergency procedure if no network
extern EmergencyProcedure EP_status;

enum class ShutterStatus {
    // static statuses
    PartiallyOpened = -1,
    Opened,
    Closed,
    // dynamic statuses (available only if AUTO)
    Opening,
    Closing,
};

//////////

#define HOSTNAME "shutter-controller"
#define WIFI_SSID "wifi_ssid"         /* TODO put your wifi ssid */
#define WIFI_PASSWORD "wifi_password" /* TODO put your wifi password */
#define WIFI_CONNECTED (WiFi.status() == WL_CONNECTED)

#define OTA_PASSWORD "ota_password" /* TODO put your OTA password */

extern AsyncWebServer WebServer;
extern AsyncEventSource SSELogger;
#define WEBPAGE_LOGIN_USER "admin"     /* TODO put your webpage user */
#define WEBPAGE_LOGIN_PASSWORD "admin" /* TODO put your webpage password */

#define HTTP_REQUEST_TIMEOUT 5000
struct httpResponseSummary {
    int code;
    String body;
};

// since ESP32 is dual core, set the net_task core to be the one left free from the loop
#define NET_TASK_CORE !CONFIG_ARDUINO_RUNNING_CORE

//////////

// Every element needs to be the sum of the aboves

// EEPROM initialization
#define EEPROM_INITIALIZED_ADDRESS 0
#define EEPROM_INITIALIZED_SIZE sizeof(bool)
// alert status
#define EEPROM_ALERT_STATUS_ADDRESS (EEPROM_INITIALIZED_ADDRESS + EEPROM_INITIALIZED_SIZE)
#define EEPROM_ALERT_STATUS_SIZE sizeof(bool)
// alert status description
#define EEPROM_ALERT_STATUS_DESCRIPTION_ADDRESS (EEPROM_ALERT_STATUS_ADDRESS + EEPROM_ALERT_STATUS_SIZE)
#define EEPROM_ALERT_STATUS_DESCRIPTION_SIZE sizeof(char[ALERT_STATUS_DESCRIPTION_SIZE])

////////////////////////////////////////////////////////////////////////////////
// FUNCTIONS

/**
 * @brief Network management task.
 */
void net_task(void *_parameter);

//////////

/**
 * @brief Setup and connect to Wi-Fi.
 */
void connectToWiFi();

/**
 * @brief Flash onboard led.
 * @param _color led color
 * @param _delayInterval time between led on and off
 */
void flashLed(const uint32_t &_color, const int &_delayInterval);

/**
 * @brief Get the shutter status based on sensors connected to the board.
 * @return A ShutterStatus enum value with the shutter position.
 */
ShutterStatus getShutterStatus();

/**
 * @brief Log on serial and using SSELogger, formatted as "[_identifier] _msg".
 * @param _identifier calling task identifier, e.g. "setup", "loop", ...
 * @param _msg logging message
 */
void logMessage(const char *_identifier, const char *_msg);
/**
 * @brief Log on serial and using SSELogger, formatted as "[_identifier] _msg".
 * @param _identifier calling task identifier, e.g. "setup", "loop", ...
 * @param _msg logging message
 */
void logMessage(const char *_identifier, const String &_msg);
/**
 * @brief Log on serial and using SSELogger, formatted as "[_identifier] _msg".
 * @param _identifier calling task identifier, e.g. "setup", "loop", ...
 * @param _msg logging message
 */
template <typename T, LOGMESSAGE_ENABLEIF(T)>
void logMessage(const char *_identifier, const T &_msg);
/**
 * @brief Log on serial and using SSELogger, formatted as "[_identifier1] (_identifier2) _msg".
 * @param _identifier1 calling task identifier, e.g. "setup", "loop", ...
 * @param _identifier2 nested calling task identifier
 * @param _msg logging message
 */
void logMessage(const char *_identifier1, const char *_identifier2, const char *_msg);
/**
 * @brief Log on serial and using SSELogger, formatted as "[_identifier1] (_identifier2) _msg".
 * @param _identifier1 calling task identifier, e.g. "setup", "loop", ...
 * @param _identifier2 nested calling task identifier
 * @param _msg logging message
 */
void logMessage(const char *_identifier1, const char *_identifier2, const String &_msg);
/**
 * @brief Log on serial and using SSELogger, formatted as "[_identifier1] (_identifier2) _msg".
 * @param _identifier1 calling task identifier, e.g. "setup", "loop", ...
 * @param _identifier2 nested calling task identifier
 * @param _msg logging message
 */
template <typename T, LOGMESSAGE_ENABLEIF(T)>
void logMessage(const char *_identifier1, const char *_identifier2, const T &_msg);
/**
 * @brief Log on serial and using SSELogger, formatted as "[_identifier1] (_identifier2) _msg".
 * @param _identifier1 calling task identifier, e.g. "setup", "loop", ...
 * @param _identifier2 nested calling task identifier
 * @param _msg logging message
 */
void logMessage(const char *_identifier1, const String &_identifier2, const char *_msg);
/**
 * @brief Log on serial and using SSELogger, formatted as "[_identifier1] (_identifier2) _msg".
 * @param _identifier1 calling task identifier, e.g. "setup", "loop", ...
 * @param _identifier2 nested calling task identifier
 * @param _msg logging message
 */
void logMessage(const char *_identifier1, const String &_identifier2, const String &_msg);
/**
 * @brief Log on serial and using SSELogger, formatted as "[_identifier1] (_identifier2) _msg".
 * @param _identifier1 calling task identifier, e.g. "setup", "loop", ...
 * @param _identifier2 nested calling task identifier
 * @param _msg logging message
 */
template <typename T, LOGMESSAGE_ENABLEIF(T)>
void logMessage(const char *_identifier1, const String &_identifier2, const T &_msg);
/**
 * @brief Log on serial and using SSELogger, formatted as "[_identifier1] (_identifier2) _msg".
 * @param _identifier1 calling task identifier, e.g. "setup", "loop", ...
 * @param _identifier2 nested calling task identifier
 * @param _msg logging message
 */
template <typename T, LOGMESSAGE_ENABLEIF(T)>
void logMessage(const char *_identifier1, const T &_identifier2, const char *_msg);
/**
 * @brief Log on serial and using SSELogger, formatted as "[_identifier1] (_identifier2) _msg".
 * @param _identifier1 calling task identifier, e.g. "setup", "loop", ...
 * @param _identifier2 nested calling task identifier
 * @param _msg logging message
 */
template <typename T, LOGMESSAGE_ENABLEIF(T)>
void logMessage(const char *_identifier1, const T &_identifier2, const String &_msg);
/**
 * @brief Log on serial and using SSELogger, formatted as "[_identifier1] (_identifier2) _msg".
 * @param _identifier1 calling task identifier, e.g. "setup", "loop", ...
 * @param _identifier2 nested calling task identifier
 * @param _msg logging message
 */
template <typename T, typename V, LOGMESSAGE_ENABLEIF(T), LOGMESSAGE_ENABLEIF(V)>
void logMessage(const char *_identifier1, const T &_identifier2, const V &_msg);

/**
 * @brief Setup and start OTA.
 */
void startOTA();

/**
 * @brief Setup and start web server.
 */
void startWebServer();

//////////

#endif  // _GLOBAL_DEFINITIONS_HPP_
