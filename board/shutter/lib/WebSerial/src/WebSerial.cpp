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

#include "WebSerial.h"

WebSerialClass::WebSerialClass(const char *ws_address) {
    _server = nullptr;
    _ws = new AsyncWebSocket {ws_address};
}

WebSerialClass::~WebSerialClass() {
    delete _ws;
}

void WebSerialClass::begin(AsyncWebServer *server) {
    _server = server;
    _server->addHandler(_ws);
}

void WebSerialClass::end() {
    _ws->enable(false);
    _ws->closeAll();
    _server->removeHandler(_ws);
    _server = nullptr;
}

bool WebSerialClass::ready() const {
    return (_server != nullptr) && (_ws->enabled());
}

void WebSerialClass::print(char *msg) const {
    _ws->textAll(msg);
}

void WebSerialClass::println(char *msg) const {
    _ws->textAll(msg);
    _ws->textAll("\n");
}

WebSerialClass WebSerial {};
