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
 * - v0.2.0: implementation of the dome encoder and RS485 communication
 * - v0.2.1: code refactor
 *
 * stable
 * - v1.0.0: first stable version (better aut/man and no-ac handling)
 * - v1.0.1: fix version number
 * - v1.0.2: better webpage css
 * - v1.0.3: fix bugs (find zero procedure and json status error) and library update */
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

/* Since ESP32 is dual core, set the async core to be the one left free from
 * the loop. By default the async tcp task is not pinned on any core, but here
 * we want that task pinned on the free core. */
#define CONFIG_ASYNC_TCP_RUNNING_CORE !CONFIG_ARDUINO_RUNNING_CORE

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <StreamUtils.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <uptime.h>
#include <uptime_formatter.h>

#include <vector>

#include "CustomOptoIn.hpp"
#include "KMPCommon.h"

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

/* RS485 semaphore: one reading per time */
extern SemaphoreHandle_t xSemaphore_rs485;
#define SEMAPHORE_RS485_TIMEOUT pdMS_TO_TICKS(250)

//////////

// clockwise motor
#define CW_MOTOR Relay::Relay1
#define IS_MOVING_CW KMPProDinoESP32.getRelayState(CW_MOTOR)
// counterclockwise motor
#define CCW_MOTOR Relay::Relay2
#define IS_MOVING_CCW KMPProDinoESP32.getRelayState(CCW_MOTOR)
// switchboard ignition relè
#define SWITCHBOARD Relay::Relay3

// indicate if the dome is moving, aka if any relays is on
#define MOVEMENT_STATUS (IS_MOVING_CW || IS_MOVING_CCW)

// CW manual rotation button
#define MAN_CW_O OptoInC::OptoInC1
#define MAN_CW customOptoIn.getState(MAN_CW_O)
// CCW manual rotation button
#define MAN_CCW_O OptoInC::OptoInC4
#define MAN_CCW customOptoIn.getState(MAN_CCW_O)
// automatic-manual switch status
#define AUTO_O OptoInC::OptoInC2
#define AUTO customOptoIn.getState(AUTO_O)
// switchboard ignition status
#define SWITCHBOARD_STATUS_O OptoInC::OptoInC3
#define SWITCHBOARD_STATUS customOptoIn.getState(SWITCHBOARD_STATUS_O)
// manual switchboard ignition button
#define MAN_IGNITION_O OptoInC::OptoInC5
#define MAN_IGNITION customOptoIn.getState(MAN_IGNITION_O)
// AC power status
#define AC_PRESENCE_O OptoInC::OptoInC7
#define AC_PRESENCE customOptoIn.getState(AC_PRESENCE_O)

enum class DomeDirection {
    CW,
    CCW
};

#define PARK_POSITION 90
#define ZERO_POSITION 248

// current dome azimuth
extern int current_az;
// current dome target azimuth
extern int target_az;

extern bool status_park;
extern bool status_finding_park;
extern bool status_finding_zero;
extern bool status_switchboard_ignited;

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

#define HOSTNAME "dome-controller"
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

//////////

// Every element needs to be the sum of the aboves

// EEPROM initialization
#define EEPROM_INITIALIZED_ADDRESS 0
#define EEPROM_INITIALIZED_SIZE sizeof(bool)
// dome position
#define EEPROM_DOME_POSITION_ADDRESS (EEPROM_INITIALIZED_ADDRESS + EEPROM_INITIALIZED_SIZE)
#define EEPROM_DOME_POSITION_SIZE sizeof(int)
// park state
#define EEPROM_PARK_STATE_ADDRESS (EEPROM_DOME_POSITION_ADDRESS + EEPROM_DOME_POSITION_SIZE)
#define EEPROM_PARK_STATE_SIZE sizeof(bool)

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
 * @brief Make HTTP request.
 * @return A httpResponseSummary instance with response code and body.
 */
httpResponseSummary httpRequest(const String &_host, const String &_uri, const uint16_t &_port = 80, const WebRequestMethod &_method = HTTP_GET, const String &_payload = "", const bool &_log = true);
/**
 * @brief Make HTTP request.
 * @return A httpResponseSummary instance with response code and body
 */
httpResponseSummary httpRequest(const char *_host, const char *_uri, const uint16_t &_port = 80, const WebRequestMethod &_method = HTTP_GET, const char *_payload = "", const bool &_log = true);
/**
 * @brief Make HTTP request.
 * @param _json reference to a JsonDocument in which deserialize the response
 * @return true on success, otherwise false.
 */
bool httpRequest(JsonDocument &_json, const String &_host, const String &_uri, const uint16_t &_port = 80, const WebRequestMethod &_method = HTTP_GET, const String &_payload = "", const bool &_log = true);
/**
 * @brief Make HTTP request.
 * @param _json reference to a JsonDocument in which deserialize the response
 * @return true on success, otherwise false.
 */
bool httpRequest(JsonDocument &_json, const char *_host, const char *_uri, const uint16_t &_port = 80, const WebRequestMethod &_method = HTTP_GET, const char *_payload = "", const bool &_log = true);
/**
 * @brief Make HTTP request.
 * @param _json reference to a JsonDocument in which deserialize the response
 * @param _filter JsonDocument for deserialization filtering using the DeserializationOption::Filter
 * @return true on success, otherwise false.
 */
bool httpRequest(JsonDocument &_json, const JsonDocument &_filter, const String &_host, const String &_uri, const uint16_t &_port = 80, const WebRequestMethod &_method = HTTP_GET, const String &_payload = "", const bool &_log = true);
/**
 * @brief Make HTTP request.
 * @param _json reference to a JsonDocument in which deserialize the response
 * @param _filter JsonDocument for deserialization filtering using the DeserializationOption::Filter
 * @return true on success, otherwise false.
 */
bool httpRequest(JsonDocument &_json, const JsonDocument &_filter, const char *_host, const char *_uri, const uint16_t &_port = 80, const WebRequestMethod &_method = HTTP_GET, const char *_payload = "", const bool &_log = true);

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
 * @brief Read from RS485 port until the end of the message.
 * @return A std::vector<byte> containing the readed values.
 */
std::vector<byte> readFromSerial485();

/**
 * @brief Setup and start OTA.
 */
void startOTA();

/**
 * @brief Setup and start web server.
 */
void startWebServer();

//////////

/**
 * @brief Disable automatic services for manual mode.
 * @return true if every request is ok, else false.
 */
bool autoToManual();

/**
 * @brief Block the execution and check if a button pression duration is above a certain time threshold.
 * @param button the button to be checked
 * @param ms time threshold (in milliseconds)
 * @return true when the button pression duration is above the threshold, false if released before.
 */
bool buttonPressed(const OptoInC &button, const unsigned long &ms);

/**
 * @brief Encoder command to retrive the dome position.
 * @return If communication is successful, return the dome azimuth. Otherwise:
 * -1 (general error), -2 (mutex error), -3 (empty data).
 */
int domePosition();

/**
 * @brief Send to the encoder the command to find the dome zero, and then move the dome until the encoder response.
 */
void findZero();

/**
 * @brief Enable automatic services for manual mode.
 * @return true if every request is ok, else false.
 */
bool manualToAuto();

/**
 * @brief Park the dome.
 */
void park();

/**
 * @brief Save env vars, relays off, and other stuffs.
 */
void shutDown();

/**
 * @brief LOW LEVEL FUNCTION. Move the dome in the specified direction. If it's already moving, stop it and then
 * start the new movement.
 * @param direction movement direction (when the dome is seen from above)
 */
void startMotion(const DomeDirection &direction);

/**
 * @brief Encoder command to start the siren.
 */
void startSiren();

/**
 * @brief Move the dome to the specified target. This function contains all the high level logic to compute and
 * complete the motion.
 */
void startSlewing(const int az_target);

/**
 * @brief LOW LEVEL FUNCTION. Stop the dome movement.
 */
void stopMotion();

/**
 * @brief Encoder command to stop the siren. Note that the siren stops at the first new command given,
 * so this is only a convenience function.
 */
void stopSiren();

/**
 * @brief Stop movement, save dome position and other motion-ending stuffs.
 */
void stopSlewing();

/**
 * @brief Impulsive command to power on the switchboard.
 */
void switchboardIgnition();

/**
 * @brief Encoder command to set the dome position into the encoder.
 * @param position range from 0 to 359
 * @return true if writing is ok, otherwise false.
 */
bool writePositionToEncoder(const int position);

//////////

#endif  // _GLOBAL_DEFINITIONS_HPP_
