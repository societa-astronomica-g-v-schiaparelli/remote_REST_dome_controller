/*
CUSTOM OPTOIN CONTROLLER LIBRARY

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

#ifndef _CUSTOM_OPTOIN_HPP_
#define _CUSTOM_OPTOIN_HPP_

#include <Arduino.h>

#include "KMPProDinoESP32.h"

enum OptoInC {
    OptoInC1 = OptoIn::OptoIn1,
    OptoInC2 = OptoIn::OptoIn2,
    OptoInC3 = OptoIn::OptoIn3,
    OptoInC4 = OptoIn::OptoIn4,
    OptoInC5 = J14_11,
    OptoInC6 = J14_10,
    OptoInC7 = J14_12,
    OptoInC8 = J14_9
};

class CustomOptoInClass {
   public:
    /**
     * @brief Setup pins for custom optoin
     */
    void setup(const int input_type);

    /**
     * @brief Get OptoIn (optical input) status
     * @param number The input number to be readed
     * @return `true` if high, false if low
     */
    bool getState(const int optoIn_number);

    /**
     * @brief Get OptoIn (optical input) status
     * @param optoIn The input to be readed
     * @return `true` if high, false if low
     */
    bool getState(const OptoInC optoIn);
};

extern CustomOptoInClass customOptoIn;

#endif  // _CUSTOM_OPTOIN_HPP_
