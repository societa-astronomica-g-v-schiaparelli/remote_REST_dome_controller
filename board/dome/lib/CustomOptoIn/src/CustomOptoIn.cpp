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

#include "CustomOptoIn.hpp"

const OptoInC optoIn_number_list[]{OptoInC::OptoInC1,
                                   OptoInC::OptoInC2,
                                   OptoInC::OptoInC3,
                                   OptoInC::OptoInC4,
                                   OptoInC::OptoInC5,
                                   OptoInC::OptoInC6,
                                   OptoInC::OptoInC7,
                                   OptoInC::OptoInC8};

void CustomOptoInClass::setup(const int input_type) {
    pinMode(OptoInC::OptoInC5, input_type);
    pinMode(OptoInC::OptoInC6, input_type);
    pinMode(OptoInC::OptoInC7, input_type);
    pinMode(OptoInC::OptoInC8, input_type);
}

bool CustomOptoInClass::getState(const int optoIn_number) {
    return getState(optoIn_number_list[optoIn_number]);
}

bool CustomOptoInClass::getState(const OptoInC optoIn) {
    return (optoIn <= 4) ? KMPProDinoESP32.getOptoInState(optoIn) : !digitalRead(optoIn);
}

CustomOptoInClass customOptoIn{};
