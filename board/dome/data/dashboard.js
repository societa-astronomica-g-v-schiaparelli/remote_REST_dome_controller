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
            document.getElementById("current-status").innerHTML = `Azimuth: ${rsp["dome-azimuth"]}°${rsp["movement-status"] ? `, moving to ${rsp["target-azimuth"]}°` : ""}`;
            const disable_motion_buttons = !rsp["optoin"]["auto"];
            document.getElementById("az-target").disabled = disable_motion_buttons;
            document.getElementById("slew-to-az-button").disabled = disable_motion_buttons;
            document.getElementById("slew-to-az-button").className = rsp["movement-status"] ? "orange" : "gray";
            document.getElementById("park-button").disabled = disable_motion_buttons;
            document.getElementById("abort-button").disabled = disable_motion_buttons;
            // update other buttons
            document.getElementById("ignite-switchboard-button").disabled = rsp["optoin"]["switchboard-status"] || disable_motion_buttons;
            document.getElementById("find-zero-button").disabled = disable_motion_buttons;
            document.getElementById("restart-button").disabled = false;
            document.getElementById("force-restart-button").disabled = false;
            document.getElementById("turn-off-button").disabled = disable_motion_buttons;
            document.getElementById("reset-EEPROM-button").disabled = false;
            document.getElementById("webserial").disabled = false;
            document.getElementById("infoButton").disabled = false;
            // update informations
            document.getElementById("firmware-version").innerHTML = rsp["firmware-version"];
            document.getElementById("uptime").innerHTML = rsp["uptime"];
            document.getElementById("dome-azimuth").innerHTML = rsp["dome-azimuth"];
            document.getElementById("target-azimuth").innerHTML = rsp["target-azimuth"];
            document.getElementById("movement-status").innerHTML = rsp["movement-status"];
            document.getElementById("in-park").innerHTML = rsp["in-park"];
            document.getElementById("finding-park").innerHTML = rsp["finding-park"];
            document.getElementById("finding-zero").innerHTML = rsp["finding-zero"];
            document.getElementById("cw-motor").innerHTML = rsp["relay"]["cw-motor"];
            document.getElementById("ccw-motor").innerHTML = rsp["relay"]["ccw-motor"];
            document.getElementById("switchboard").innerHTML = rsp["relay"]["switchboard"];
            document.getElementById("auto").innerHTML = rsp["optoin"]["auto"];
            document.getElementById("switchboard-status").innerHTML = rsp["optoin"]["switchboard-status"];
            document.getElementById("ac-presence").innerHTML = rsp["optoin"]["ac-presence"];
            document.getElementById("manual-cw-button").innerHTML = rsp["optoin"]["manual-cw-button"];
            document.getElementById("manual-ccw-button").innerHTML = rsp["optoin"]["manual-ccw-button"];
            document.getElementById("manual-ignition").innerHTML = rsp["optoin"]["manual-ignition"];
            document.getElementById("hostname").innerHTML = rsp["wifi"]["hostname"];
            document.getElementById("mac-address").innerHTML = rsp["wifi"]["mac-address"];
        })
        .catch(error => {
            console.trace(`An error has occured: ${error.message}`);
            // update page with default
            // disable main buttons
            document.getElementById("current-status-box").style.opacity = 0.4;
            document.getElementById("current-status-man").style.display = "none";
            document.getElementById("current-status").innerHTML = "State not available";
            document.getElementById("az-target").disabled = true;
            document.getElementById("slew-to-az-button").disabled = true;
            document.getElementById("slew-to-az-button").className = "gray";
            document.getElementById("park-button").disabled = true;
            document.getElementById("abort-button").disabled = true;
            // disable other buttons
            document.getElementById("ignite-switchboard-button").disabled = true;
            document.getElementById("find-zero-button").disabled = true;
            document.getElementById("restart-button").disabled = true;
            document.getElementById("force-restart-button").disabled = true;
            document.getElementById("turn-off-button").disabled = true;
            document.getElementById("reset-EEPROM-button").disabled = true;
            document.getElementById("webserial").disabled = true;
            document.getElementById("infoButton").disabled = true;
            // default info
            document.getElementById("firmware-version").innerHTML = "ND";
            document.getElementById("uptime").innerHTML = "ND";
            document.getElementById("dome-azimuth").innerHTML = "ND";
            document.getElementById("target-azimuth").innerHTML = "ND";
            document.getElementById("movement-status").innerHTML = "ND";
            document.getElementById("in-park").innerHTML = "ND";
            document.getElementById("finding-park").innerHTML = "ND";
            document.getElementById("finding-zero").innerHTML = "ND";
            document.getElementById("cw-motor").innerHTML = "ND";
            document.getElementById("ccw-motor").innerHTML = "ND";
            document.getElementById("switchboard").innerHTML = "ND";
            document.getElementById("auto").innerHTML = "ND";
            document.getElementById("switchboard-status").innerHTML = "ND";
            document.getElementById("ac-presence").innerHTML = "ND";
            document.getElementById("manual-cw-button").innerHTML = "ND";
            document.getElementById("manual-ccw-button").innerHTML = "ND";
            document.getElementById("manual-ignition").innerHTML = "ND";
            document.getElementById("hostname").innerHTML = "ND";
            document.getElementById("mac-address").innerHTML = "ND";
        });
}

function command(command) {
    // setup request
    const moving_command = command == "slew-to-az";
    let json = { "cmd": command }
    if (moving_command) {
        const az = document.getElementById("az-target").value;
        document.getElementById("az-target").value = "";
        if (!az || az < 0 || az > 360) {
            alert("Error: invalid azimuth.");
            return;
        }
        clearInterval(getStatus_interval);
        json["az-target"] = az;
    }
    const button = `${command}-button`;
    document.getElementById(button).className = "orange";
    // make request
    const request = new Request(`${DEBUG ? `http://${DEBUG_IP}` : ""}/api?json=${encodeURIComponent(JSON.stringify(json))}`);
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
