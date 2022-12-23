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

const DEBUG = false;
var DEBUG_IP = "IP_address_here"; // TODO put your IP address here

const UPDATE_TIME = 1000;
var getStatus_interval;
var connection_error_alert = false;

function start() {
    setTimeout(getStatus, 0);
    getStatus_interval = setInterval(getStatus, UPDATE_TIME);
}

function getStatus() {
    const request = new Request(`${DEBUG ? `http://${DEBUG_IP}` : ""}/api?json=${encodeURIComponent(JSON.stringify({ "cmd": "status" }))}`);
    fetchTimeout(request)
        .then(rsp => {
            if (connection_error_alert) {
                connection_error_alert = false;
                Toast.fire({
                    icon: "success",
                    title: "Connection restored"
                });
            }
            // update main buttons
            document.getElementById("current-status-box").style.opacity = 1;
            document.getElementById("current-status-man").style.display = !rsp["optoin"]["auto"] ? "" : "none";
            document.getElementById("current-status").innerText = `Azimuth: ${rsp["dome-azimuth"]}°${rsp["movement-status"] ? `, moving to ${rsp["target-azimuth"]}°` : ""}`;
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
            // document.getElementById("log").disabled = false;
            document.getElementById("infoButton").disabled = false;
            // update informations
            document.getElementById("firmware-version").innerText = rsp["firmware-version"];
            document.getElementById("uptime").innerText = rsp["uptime"];
            document.getElementById("dome-azimuth").innerText = rsp["dome-azimuth"];
            document.getElementById("target-azimuth").innerText = rsp["target-azimuth"];
            document.getElementById("movement-status").innerText = rsp["movement-status"];
            document.getElementById("in-park").innerText = rsp["in-park"];
            document.getElementById("finding-park").innerText = rsp["finding-park"];
            document.getElementById("finding-zero").innerText = rsp["finding-zero"];
            document.getElementById("relay-list").innerText = "";
            for (const el of rsp["relay"]["list"]) document.getElementById("relay-list").innerText += `${(el | 0)}`;
            document.getElementById("cw-motor").innerText = rsp["relay"]["cw-motor"];
            document.getElementById("ccw-motor").innerText = rsp["relay"]["ccw-motor"];
            document.getElementById("switchboard").innerText = rsp["relay"]["switchboard"];
            document.getElementById("optoin-list").innerText = "";
            for (const el of rsp["optoin"]["list"]) document.getElementById("optoin-list").innerText += `${(el | 0)}`;
            document.getElementById("auto").innerText = rsp["optoin"]["auto"];
            document.getElementById("switchboard-status").innerText = rsp["optoin"]["switchboard-status"];
            document.getElementById("ac-presence").innerText = rsp["optoin"]["ac-presence"];
            document.getElementById("manual-cw-button").innerText = rsp["optoin"]["manual-cw-button"];
            document.getElementById("manual-ccw-button").innerText = rsp["optoin"]["manual-ccw-button"];
            document.getElementById("manual-ignition").innerText = rsp["optoin"]["manual-ignition"];
            document.getElementById("hostname").innerText = rsp["wifi"]["hostname"];
            document.getElementById("mac-address").innerText = rsp["wifi"]["mac-address"];
        })
        .catch(error => {
            console.trace(`An error has occured: ${error.message}`);
            if (!connection_error_alert) {
                connection_error_alert = true;
                Toast2.fire({
                    icon: "error",
                    title: "Connection error",
                    text: "Unable to connect to the controller."
                });
            }
            // update page with default
            // disable main buttons
            document.getElementById("current-status-box").style.opacity = 0.4;
            document.getElementById("current-status-man").style.display = "none";
            document.getElementById("current-status").innerText = "Status unavailable";
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
            // document.getElementById("log").disabled = true;
            document.getElementById("infoButton").disabled = true;
            // default info
            document.getElementById("firmware-version").innerText = "ND";
            document.getElementById("uptime").innerText = "ND";
            document.getElementById("dome-azimuth").innerText = "ND";
            document.getElementById("target-azimuth").innerText = "ND";
            document.getElementById("movement-status").innerText = "ND";
            document.getElementById("in-park").innerText = "ND";
            document.getElementById("finding-park").innerText = "ND";
            document.getElementById("finding-zero").innerText = "ND";
            document.getElementById("relay-list").innerText = "ND";
            document.getElementById("cw-motor").innerText = "ND";
            document.getElementById("ccw-motor").innerText = "ND";
            document.getElementById("switchboard").innerText = "ND";
            document.getElementById("optoin-list").innerText = "ND";
            document.getElementById("auto").innerText = "ND";
            document.getElementById("switchboard-status").innerText = "ND";
            document.getElementById("ac-presence").innerText = "ND";
            document.getElementById("manual-cw-button").innerText = "ND";
            document.getElementById("manual-ccw-button").innerText = "ND";
            document.getElementById("manual-ignition").innerText = "ND";
            document.getElementById("hostname").innerText = "ND";
            document.getElementById("mac-address").innerText = "ND";
        });
}

function command(command) {
    // setup request
    let json = { "cmd": command }
    const moving_command = ["slew-to-az"].includes(command);
    if (moving_command) {
        const az = document.getElementById("az-target").value;
        document.getElementById("az-target").value = "";
        if (!az || az < 0 || az > 360) {
            console.trace(`Invalid azimuth`);
            Swal.fire({
                icon: "error",
                title: "Error",
                text: `Invalid azimuth '${az}'.`
            });
            return;
        }
        json["az-target"] = az;
    }
    // make request
    const request = new Request(`${DEBUG ? `http://${DEBUG_IP}` : ""}/api?json=${encodeURIComponent(JSON.stringify(json))}`);
    fetchTimeout(request)
        .then(rsp => {
            if (rsp != "done") throw new Error(rsp);
            Toast.fire({
                icon: "success",
                title: "Command sent"
            });
        })
        .catch(error => {
            console.trace(`An error has occured: ${error.message}`);
            Swal.fire({
                icon: "error",
                title: "Error",
                text: "Failed to send the command."
            });
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

const Toast = Swal.mixin({
    toast: true,
    position: 'top-end',
    showConfirmButton: false,
    timer: 1500,
    timerProgressBar: true,
});

const Toast2 = Swal.mixin({
    toast: true,
    position: 'top-end',
    showConfirmButton: false
});
