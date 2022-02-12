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


#pragma once


//////////


#define FIRMWARE_VERSION "v0.2.0"


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

/* Since ESP32 is dual core, set the async core to be the one left free from
 * the loop. By default the async tcp task is not pinned on any core, but here
 * we want that task pinned on the free core. */
#define CONFIG_ASYNC_TCP_RUNNING_CORE !CONFIG_ARDUINO_RUNNING_CORE

#include "KMPProDinoESP32.h"
#include "KMPCommon.h"

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include "CustomOptoIn.h"
#include <EEPROM.h>
#include <esp_wifi.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <StreamUtils.h>
#include <uptime_formatter.h>
#include <uptime.h>
#include <vector>
#include "WebSerial.h"
#include <WiFi.h>


////////////////////////////////////////////////////////////////////////////////
// VARIABLES


// handle led blink
extern bool blink_led_loop;

// types for logToSerial template (types list from String constructor)
#define LOGTOSERIAL_ENABLEIF(T) typename std::enable_if< \
                                    std::is_same<T,char>::value || \
                                    std::is_same<T,unsigned char>::value || \
                                    std::is_same<T,int>::value || \
                                    std::is_same<T,unsigned int>::value || \
                                    std::is_same<T,long>::value || \
                                    std::is_same<T,unsigned long>::value || \
                                    std::is_same<T,float>::value || \
                                    std::is_same<T,double>::value || \
                                    std::is_same<T,uint8_t>::value \
                                , int>::type = 0

// global semaphore
extern SemaphoreHandle_t xSemaphore;
// rs485 semaphore
extern SemaphoreHandle_t xSemaphore_rs485;
#define SEMAPHORE_RS485_TIMEOUT pdMS_TO_TICKS(250)


//////////


// clockwise motor
#define CW_MOTOR Relay1
#define IS_MOVING_CW KMPProDinoESP32.getRelayState(CW_MOTOR)
// counterclockwise motor
#define CCW_MOTOR Relay2
#define IS_MOVING_CCW KMPProDinoESP32.getRelayState(CCW_MOTOR)
// switchboard ignition relè
#define SWITCHBOARD Relay3

// indicate if the dome is moving, aka if any relays is on
#define MOVEMENT_STATUS (IS_MOVING_CW || IS_MOVING_CCW)

// CW manual rotation button
#define MAN_CW_O OptoInC1
#define MAN_CW customOptoIn.getState(MAN_CW_O)
// CCW manual rotation button
#define MAN_CCW_O OptoInC4
#define MAN_CCW customOptoIn.getState(MAN_CCW_O)
// automatic-manual switch status
#define AUTO_O OptoInC2
#define AUTO customOptoIn.getState(AUTO_O)
// switchboard ignition status
#define SWITCHBOARD_STATUS_O OptoInC3
#define SWITCHBOARD_STATUS customOptoIn.getState(SWITCHBOARD_STATUS_O)
// manual switchboard ignition button
#define MAN_IGNITION_O OptoInC5
#define MAN_IGNITION customOptoIn.getState(MAN_IGNITION_O)
// AC power status
#define AC_PRESENCE_O OptoInC7
#define AC_PRESENCE customOptoIn.getState(AC_PRESENCE_O)
// // automatic switchboard ignition relè (deprecated)
#define AUTO_IGNITION_O OptoInC6
#define AUTO_IGNITION customOptoIn.getState(AUTO_IGNITION_O)

enum DomeDirection {
    CW_DIRECTION,
    CCW_DIRECTION
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

extern bool error_state;

enum ShutterStatus {
    // static statuses
    ShutterPartiallyOpened = -1,
    ShutterOpened,
    ShutterClosed,
    // dynamic statuses (available only if AUTO)
    ShutterOpening,
    ShutterClosing
};


//////////


#define HOSTNAME "main-dome-controller"
#define WIFI_CONNECTED (WiFi.status() == WL_CONNECTED)

#error "Insert Wi-Fi SSID and password, then delete this line"
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#error "Insert OTA password, then delete this line"
#define OTA_PASSWORD ""

extern AsyncWebServer WebServer;
#error "Insert webpage user and password, then delete this line"
#define WEBPAGE_LOGIN_USER ""
#define WEBPAGE_LOGIN_PASSWORD ""

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
// log status
#define EEPROM_LOG_STATUS_ADDRESS (EEPROM_INITIALIZED_ADDRESS + EEPROM_INITIALIZED_SIZE)
#define EEPROM_LOG_STATUS_SIZE sizeof(bool)
// dome position
#define EEPROM_DOME_POSITION_ADDRESS (EEPROM_LOG_STATUS_ADDRESS + EEPROM_LOG_STATUS_SIZE)
#define EEPROM_DOME_POSITION_SIZE sizeof(int)
// error state
#define EEPROM_ERROR_STATE_ADDRESS (EEPROM_DOME_POSITION_ADDRESS + EEPROM_DOME_POSITION_SIZE)
#define EEPROM_ERROR_STATE_SIZE sizeof(bool)
// park state
#define EEPROM_PARK_STATE_ADDRESS (EEPROM_ERROR_STATE_ADDRESS + EEPROM_ERROR_STATE_SIZE)
#define EEPROM_PARK_STATE_SIZE sizeof(bool)


////////////////////////////////////////////////////////////////////////////////
// FUNCTIONS


/*!
    @brief  Network management task.
*/
void net_task(void *_parameter);


//////////


/*!
    @brief  Setup and connect to Wi-Fi.
*/
void connectToWiFi();

/*!
    @brief  Flash onboard led.
    @param  _color          led color
    @param  _delayInterval  time between led on and off
*/
void flashLed(const RgbColor &_color, const int &_delayInterval);

/*!
    @brief  Make HTTP request.
    @return A httpResponseSummary instance with response code and body.
*/
httpResponseSummary httpRequest(const String &_host, const String &_uri, const uint16_t &_port = 80);
/*!
    @brief  Make HTTP request.
    @return A httpResponseSummary instance with response code and body
*/
httpResponseSummary httpRequest(const char *_host, const char *_uri, const uint16_t &_port = 80);
/*!
    @brief  Make HTTP request.
    @param  _json   reference to a JsonDocument in which deserialize the response
    @return true on success, otherwise false.
*/
bool httpRequest(JsonDocument &_json, const String &_host, const String &_uri, const uint16_t &_port = 80);
/*!
    @brief  Make HTTP request.
    @param  _json   reference to a JsonDocument in which deserialize the response
    @return true on success, otherwise false.
*/
bool httpRequest(JsonDocument &_json, const char *_host, const char *_uri, const uint16_t &_port = 80);
/*!
    @brief  Make HTTP request.
    @param  _json   reference to a JsonDocument in which deserialize the response
    @param  _filter JsonDocument for deserialization filtering using the DeserializationOption::Filter
    @return true on success, otherwise false.
*/
bool httpRequest(JsonDocument &_json, const JsonDocument &_filter, const String &_host, const String &_uri, const uint16_t &_port = 80);
/*!
    @brief  Make HTTP request.
    @param  _json   reference to a JsonDocument in which deserialize the response
    @param  _filter JsonDocument for deserialization filtering using the DeserializationOption::Filter
    @return true on success, otherwise false.
*/
bool httpRequest(JsonDocument &_json, const JsonDocument &_filter, const char *_host, const char *_uri, const uint16_t &_port = 80);

/*!
    @brief  Log on serial and webserial, formatted as "[_identifier] _msg".
    @param  _identifier     calling task identifier, e.g. "setup", "loop", ...
    @param  _msg            logging message
*/
void logToSerial(const char *_identifier, const char *_msg);
/*!
    @brief  Log on serial and webserial, formatted as "[_identifier] _msg".
    @param  _identifier     calling task identifier, e.g. "setup", "loop", ...
    @param  _msg            logging message
*/
void logToSerial(const char *_identifier, const String &_msg);
/*!
    @brief  Log on serial and webserial, formatted as "[_identifier] _msg".
    @param  _identifier     calling task identifier, e.g. "setup", "loop", ...
    @param  _msg            logging message
*/
template <typename T, LOGTOSERIAL_ENABLEIF(T)>
void logToSerial(const char *_identifier, const T &_msg);
/*!
    @brief  Log on serial and webserial, formatted as "[_identifier1] (_identifier2) _msg".
    @param  _identifier1    calling task identifier, e.g. "setup", "loop", ...
    @param  _identifier2    nested calling task identifier
    @param  _msg            logging message
*/
void logToSerial(const char *_identifier1, const char *_identifier2, const char *_msg);
/*!
    @brief  Log on serial and webserial, formatted as "[_identifier1] (_identifier2) _msg".
    @param  _identifier1    calling task identifier, e.g. "setup", "loop", ...
    @param  _identifier2    nested calling task identifier
    @param  _msg            logging message
*/
void logToSerial(const char *_identifier1, const char *_identifier2, const String &_msg);
/*!
    @brief  Log on serial and webserial, formatted as "[_identifier1] (_identifier2) _msg".
    @param  _identifier1    calling task identifier, e.g. "setup", "loop", ...
    @param  _identifier2    nested calling task identifier
    @param  _msg            logging message
*/
template <typename T, LOGTOSERIAL_ENABLEIF(T)>
void logToSerial(const char *_identifier1, const char *_identifier2, const T &_msg);
/*!
    @brief  Log on serial and webserial, formatted as "[_identifier1] (_identifier2) _msg".
    @param  _identifier1    calling task identifier, e.g. "setup", "loop", ...
    @param  _identifier2    nested calling task identifier
    @param  _msg            logging message
*/
void logToSerial(const char *_identifier1, const String &_identifier2, const char *_msg);
/*!
    @brief  Log on serial and webserial, formatted as "[_identifier1] (_identifier2) _msg".
    @param  _identifier1    calling task identifier, e.g. "setup", "loop", ...
    @param  _identifier2    nested calling task identifier
    @param  _msg            logging message
*/
void logToSerial(const char *_identifier1, const String &_identifier2, const String &_msg);
/*!
    @brief  Log on serial and webserial, formatted as "[_identifier1] (_identifier2) _msg".
    @param  _identifier1    calling task identifier, e.g. "setup", "loop", ...
    @param  _identifier2    nested calling task identifier
    @param  _msg            logging message
*/
template <typename T, LOGTOSERIAL_ENABLEIF(T)>
void logToSerial(const char *_identifier1, const String &_identifier2, const T &_msg);
/*!
    @brief  Log on serial and webserial, formatted as "[_identifier1] (_identifier2) _msg".
    @param  _identifier1    calling task identifier, e.g. "setup", "loop", ...
    @param  _identifier2    nested calling task identifier
    @param  _msg            logging message
*/
template <typename T, LOGTOSERIAL_ENABLEIF(T)>
void logToSerial(const char *_identifier1, const T &_identifier2, const char *_msg);
/*!
    @brief  Log on serial and webserial, formatted as "[_identifier1] (_identifier2) _msg".
    @param  _identifier1    calling task identifier, e.g. "setup", "loop", ...
    @param  _identifier2    nested calling task identifier
    @param  _msg            logging message
*/
template <typename T, LOGTOSERIAL_ENABLEIF(T)>
void logToSerial(const char *_identifier1, const T &_identifier2, const String &_msg);
/*!
    @brief  Log on serial and webserial, formatted as "[_identifier1] (_identifier2) _msg".
    @param  _identifier1    calling task identifier, e.g. "setup", "loop", ...
    @param  _identifier2    nested calling task identifier
    @param  _msg            logging message
*/
template <typename T, typename V, LOGTOSERIAL_ENABLEIF(T), LOGTOSERIAL_ENABLEIF(V)>
void logToSerial(const char *_identifier1, const T &_identifier2, const V &_msg);

/*!
    @brief  Read from RS485 port until the end of the message.
    @return A std::vector<byte> containing the readed values.
*/
std::vector<byte> readFromSerial485();

/*!
    @brief  Setup and start OTA.
*/
void startOTA();

/*!
    @brief  Setup and start web server.
*/
void startWebServer();


//////////


/*!
    @brief  Return true if the specified button is pressed more than the specified seconds, false otherwise
    @param button Boolean of button is pressed
    @param ms Integer of seconds needed
*/
bool buttonPressed(const OptoInC &button, const unsigned long &ms);

/*!
    @brief  Enable motor
*/
void startMotion(const DomeDirection &direction);

/*!
    @brief  Impulsive command to power on switchboard
*/
void switchboardIgnition();

/*!
    @brief  Encoder command to start siren
*/
void startSiren();

/*!
    @brief  Encorder command to know dome position, discarded.
*/
void stopSiren();

/*!
    @brief  Disable motors
*/
void stopMotion();

/*!
    @brief  Encoder command to set dome position into the encoder
    @param position range from 0 to 359
*/
void writePositionToEncoder(const int position);


//////////


/*!
    @brief  Encoder command to know dome position, return -1 in case of fallure
*/
int domePosition();

/*!
    @brief  Encoder command to find zeros, followed by dome movment until encoder calls back
*/
void findZero();

/*!
    @brief  Save env vars, relays off, switchboard off
*/
void shutDown();

/*!
    @brief  Calculation for direction, start movement
*/
void startSlewing(const int az_target);

/*!
    @brief  Stop movement, save dome position
*/
void stopSlewing();

/*!
    @brief  Park dome
*/
void park();
