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


#define FIRMWARE_VERSION "v0.1.1"


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

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <esp_wifi.h>
#include <ESP32Ping.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <uptime_formatter.h>
#include <uptime.h>
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


//////////


// shutter opening motor
#define OPENING_MOTOR Relay1
#define IS_OPENING KMPProDinoESP32.getRelayState(OPENING_MOTOR)
// shutter closing motor
#define CLOSING_MOTOR Relay2
#define IS_CLOSING KMPProDinoESP32.getRelayState(CLOSING_MOTOR)

// indicate if the shutter is moving, aka if any relays is on
#define MOVEMENT_STATUS (IS_OPENING || IS_CLOSING)

// automatic-manual switch status
#define AUTO KMPProDinoESP32.getOptoInState(OptoIn1)
// closing limit switch sensor
#define CLOSED_SENSOR KMPProDinoESP32.getOptoInState(OptoIn2)
// opening limit switch sensor
#define OPENED_SENSOR KMPProDinoESP32.getOptoInState(OptoIn3)

// empirical time to let both motors close or open, aka the time between toggle and real open/close
#define SENSOR_TOGGLING_TIME 4500
// max time to wait before trigger the alert status
#define ALERT_STATUS_WAIT 22000
// aux variable to check that the shutter moving time is below ALERT_STATUS_WAIT
extern unsigned long start_movement_time;
// alert status triggered if the shutter do not complete motion in the predicted time, e.g. if a mechanical problem occurs
extern bool alert_status;
#define ALERT_STATUS_DESCRIPTION_SIZE 128
// description of what caused the alert status
extern char alert_status_description[ALERT_STATUS_DESCRIPTION_SIZE];

// time between each ping
#define INTERNET_PING_TIME 60000
// ping target website on WAN
#define INTERNET_PING_WEBSITE "www.google.com"
// max time to wait without network before the emergency close of the shutter
#define MAX_TIME_NO_NETWORK 600000
// true if connected to wifi and internet is ok, otherwise false
extern bool network_connection_status;
// alert status triggered if no network for too much time
extern bool network_alert_status;

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


#define HOSTNAME "dome-shutter-controller"
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
    @brief  Return the shutter status based on sensors connected to the board.
*/
ShutterStatus getShutterStatus();

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
    @brief  Setup and start OTA.
*/
void startOTA();

/*!
    @brief  Setup and start web server.
*/
void startWebServer();
