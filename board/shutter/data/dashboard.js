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

const DEBUG = false;
const DEBUG_IP = "";

const UPDATE_TIME = 1000;
var getStatus_interval;

function start() {
    setTimeout(getStatus, 0);
    getStatus_interval = setInterval(getStatus, UPDATE_TIME);
}

function getStatus() {
    const request = new Request(`${DEBUG ? `http://${DEBUG_IP}` : ""}/api?json=${encodeURIComponent(JSON.stringify({ "cmd": "status" }))}`);
    fetchTimeout(request)
        .then(rsp => {
            // update main buttons
            document.getElementById("current-status-box").style.opacity = 1;
            document.getElementById("current-status-man").style.display = !rsp["optoin"]["auto"] ? "" : "none";
            document.getElementById("current-status-alert").style.display = rsp["alert"]["alert-status"] ? "" : "none";
            document.getElementById("current-status-network-alert").style.display = rsp["alert"]["network-alert-status"] ? "" : "none";
            const disable_motion_buttons = !rsp["optoin"]["auto"] || rsp["alert"]["alert-status"] || rsp["alert"]["network-alert-status"];
            document.getElementById("abort-button").disabled = disable_motion_buttons;
            switch (rsp["shutter-status"]) {
                case -1:
                    document.getElementById("current-status").innerHTML = "Shutter partially opened";
                    document.getElementById("open-button").disabled = disable_motion_buttons;
                    document.getElementById("open-button").className = "gray";
                    document.getElementById("close-button").disabled = disable_motion_buttons;
                    document.getElementById("close-button").className = "gray";
                    break;
                case 0:
                    document.getElementById("current-status").innerHTML = "Shutter opened";
                    document.getElementById("open-button").disabled = true;
                    document.getElementById("open-button").className = "gray";
                    document.getElementById("close-button").disabled = disable_motion_buttons;
                    document.getElementById("close-button").className = "gray";
                    break;
                case 1:
                    document.getElementById("current-status").innerHTML = "Shutter closed";
                    document.getElementById("open-button").disabled = disable_motion_buttons;
                    document.getElementById("open-button").className = "gray";
                    document.getElementById("close-button").disabled = true;
                    document.getElementById("close-button").className = "gray";
                    break;
                case 2:
                    document.getElementById("current-status").innerHTML = "Shutter opening...";
                    document.getElementById("open-button").disabled = true;
                    document.getElementById("open-button").className = "orange";
                    document.getElementById("close-button").disabled = disable_motion_buttons;
                    document.getElementById("close-button").className = "gray";
                    break;
                case 3:
                    document.getElementById("current-status").innerHTML = "Shutter closing...";
                    document.getElementById("open-button").disabled = disable_motion_buttons;
                    document.getElementById("open-button").className = "gray";
                    document.getElementById("close-button").disabled = true;
                    document.getElementById("close-button").className = "orange";
                    break;
                default:
                    throw new Error("Unknown shutter-status from shutter");
                    break;
            }
            // update other buttons
            document.getElementById("restart-button").disabled = false;
            document.getElementById("force-restart-button").disabled = false;
            document.getElementById("reset-alert-status-button").disabled = false;
            document.getElementById("reset-EEPROM-button").disabled = false;
            document.getElementById("webserial").disabled = false;
            document.getElementById("infoButton").disabled = false;
            // update informations
            document.getElementById("firmware-version").innerHTML = rsp["firmware-version"];
            document.getElementById("uptime").innerHTML = rsp["uptime"];
            document.getElementById("shutter-status").innerHTML = rsp["shutter-status"];
            document.getElementById("movement-status").innerHTML = rsp["movement-status"];
            document.getElementById("network-status").innerHTML = rsp["network-status"];
            document.getElementById("alert-status").innerHTML = rsp["alert"]["alert-status"];
            document.getElementById("alert-status-description-li").style.display = rsp["alert"]["alert-status"] ? "" : "none";
            document.getElementById("alert-status-description").innerHTML = rsp["alert"]["alert-status-description"];
            document.getElementById("network-alert-status").innerHTML = rsp["alert"]["network-alert-status"];
            document.getElementById("opening-motor").innerHTML = rsp["relay"]["opening-motor"];
            document.getElementById("closing-motor").innerHTML = rsp["relay"]["closing-motor"];
            document.getElementById("auto").innerHTML = rsp["optoin"]["auto"];
            document.getElementById("opened-sensor").innerHTML = rsp["optoin"]["opened-sensor"];
            document.getElementById("closed-sensor").innerHTML = rsp["optoin"]["closed-sensor"];
            document.getElementById("hostname").innerHTML = rsp["wifi"]["hostname"];
            document.getElementById("mac-address").innerHTML = rsp["wifi"]["mac-address"];
        })
        .catch(error => {
            console.trace(`An error has occured: ${error.message}`);
            // update page with default
            // disable main buttons
            document.getElementById("current-status-box").style.opacity = 0.4;
            document.getElementById("current-status-man").style.display = "none";
            document.getElementById("current-status-alert").style.display = "none";
            document.getElementById("current-status-network-alert").style.display = "none";
            document.getElementById("current-status").innerHTML = "State not available";
            document.getElementById("open-button").disabled = true;
            document.getElementById("open-button").className = "gray";
            document.getElementById("close-button").disabled = true;
            document.getElementById("close-button").className = "gray";
            document.getElementById("abort-button").disabled = true;
            // disable other buttons
            document.getElementById("restart-button").disabled = true;
            document.getElementById("force-restart-button").disabled = true;
            document.getElementById("reset-alert-status-button").disabled = true;
            document.getElementById("reset-EEPROM-button").disabled = true;
            document.getElementById("webserial").disabled = true;
            document.getElementById("infoButton").disabled = true;
            // default info
            document.getElementById("firmware-version").innerHTML = "ND";
            document.getElementById("uptime").innerHTML = "ND";
            document.getElementById("shutter-status").innerHTML = "ND";
            document.getElementById("movement-status").innerHTML = "ND";
            document.getElementById("network-status").innerHTML = "ND";
            document.getElementById("alert-status").innerHTML = "ND";
            document.getElementById("alert-status-description-li").style.display = "";
            document.getElementById("alert-status-description").innerHTML = "ND";
            document.getElementById("network-alert-status").innerHTML = "ND";
            document.getElementById("opening-motor").innerHTML = "ND";
            document.getElementById("closing-motor").innerHTML = "ND";
            document.getElementById("auto").innerHTML = "ND";
            document.getElementById("opened-sensor").innerHTML = "ND";
            document.getElementById("closed-sensor").innerHTML = "ND";
            document.getElementById("hostname").innerHTML = "ND";
            document.getElementById("mac-address").innerHTML = "ND";
        });
}

function command(command) {
    // setup request
    const moving_command = (command == "open" || command == "close");
    if (moving_command) clearInterval(getStatus_interval);
    const button = `${command}-button`;
    document.getElementById(button).className = "orange";
    // make request
    const request = new Request(`${DEBUG ? `http://${DEBUG_IP}` : ""}/api?json=${encodeURIComponent(JSON.stringify({ "cmd": command }))}`);
    fetchTimeout(request)
        .then(rsp => {
            if (rsp != "done") throw new Error(rsp);
            if (!moving_command) document.getElementById(button).className = "green";
            setTimeout(moving_command ? start : () => { document.getElementById(button).className = "gray"; }, 2000);
        })
        .catch(error => {
            console.trace(`An error has occured: ${error.message}`);
            document.getElementById(button).className = "red";
            setTimeout(moving_command ? start : () => { document.getElementById(button).className = "gray"; }, 2000);
            alert(`Request "${command}" failed:\n\n${error.message}`);
        });
}

function toggleInfo() {
    if (document.getElementById("infoBox").style.display == "none") {
        document.getElementById("infoBox").style.display = "";
        document.getElementById("infoButton").value = "Less informations";
    } else {
        document.getElementById("infoBox").style.display = "none";
        document.getElementById("infoButton").value = "More informations";
    }
}

async function fetchTimeout(url, ms = 3500, { signal, ...options } = {}) {
    const controller = new AbortController();
    const promise = fetch(url, { signal: controller.signal, ...options });
    if (signal) signal.addEventListener("abort", () => controller.abort());
    const timeout = setTimeout(() => controller.abort(), ms);
    const response = await promise.finally(() => clearTimeout(timeout));
    if (!response.ok) throw new Error(`${response.status}, ${response.statusText}`);
    const json = await response.json();
    return json["rsp"];
}
