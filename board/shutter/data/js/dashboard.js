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
            document.getElementById("current-status-lock-movement").style.display = rsp["lock-movement"] ? "" : "none";
            document.getElementById("current-status-hardware-alert").style.display = rsp["alert"]["hardware"]["status"] ? "" : "none";
            document.getElementById("current-status-network-alert").style.display = rsp["alert"]["network"]["status"] ? "" : "none";
            const disable_motion_buttons = !rsp["optoin"]["auto"] || rsp["lock-movement"] || rsp["alert"]["hardware"]["status"] || rsp["alert"]["network"]["status"];
            document.getElementById("abort-button").disabled = disable_motion_buttons;
            switch (rsp["shutter-status"]) {
                case -1:
                    document.getElementById("current-status").innerText = "Shutter partially opened";
                    document.getElementById("open-button").disabled = disable_motion_buttons;
                    document.getElementById("open-button").className = "gray";
                    document.getElementById("close-button").disabled = disable_motion_buttons;
                    document.getElementById("close-button").className = "gray";
                    break;
                case 0:
                    document.getElementById("current-status").innerText = "Shutter opened";
                    document.getElementById("open-button").disabled = true;
                    document.getElementById("open-button").className = "gray";
                    document.getElementById("close-button").disabled = disable_motion_buttons;
                    document.getElementById("close-button").className = "gray";
                    break;
                case 1:
                    document.getElementById("current-status").innerText = "Shutter closed";
                    document.getElementById("open-button").disabled = disable_motion_buttons;
                    document.getElementById("open-button").className = "gray";
                    document.getElementById("close-button").disabled = true;
                    document.getElementById("close-button").className = "gray";
                    break;
                case 2:
                    document.getElementById("current-status").innerText = "Shutter opening...";
                    document.getElementById("open-button").disabled = true;
                    document.getElementById("open-button").className = "orange";
                    document.getElementById("close-button").disabled = disable_motion_buttons;
                    document.getElementById("close-button").className = "gray";
                    break;
                case 3:
                    document.getElementById("current-status").innerText = "Shutter closing...";
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
            document.getElementById("lock-movement-button").disabled = rsp["lock-movement"];
            document.getElementById("unlock-movement-button").disabled = !rsp["lock-movement"];
            document.getElementById("lock-movement-button").style.display = rsp["lock-movement"] ? "none" : "";
            document.getElementById("unlock-movement-button").style.display = !rsp["lock-movement"] ? "none" : "";
            document.getElementById("restart-button").disabled = false;
            document.getElementById("force-restart-button").disabled = false;
            document.getElementById("reset-alert-status-button").disabled = false;
            document.getElementById("reset-EEPROM-button").disabled = false;
            // document.getElementById("log").disabled = false;
            document.getElementById("infoButton").disabled = false;
            // update informations
            document.getElementById("firmware-version").innerText = rsp["firmware-version"];
            document.getElementById("uptime").innerText = rsp["uptime"];
            document.getElementById("shutter-status").innerText = rsp["shutter-status"];
            document.getElementById("movement-status").innerText = rsp["movement-status"];
            document.getElementById("lock-movement").innerText = rsp["lock-movement"];
            document.getElementById("network-status").innerText = rsp["network-status"];
            document.getElementById("hardware-alert-status").innerText = rsp["alert"]["hardware"]["status"];
            if (rsp["alert"]["hardware"]["status"]) document.getElementById("hardware-alert-status-description").innerText = `, ${rsp["alert"]["hardware"]["description"]}`;
            document.getElementById("network-alert-status").innerText = rsp["alert"]["network"]["status"];
            switch (rsp["alert"]["network"]["security-procedures"]) {
                case 0:
                    document.getElementById("EP-status").innerText = "not needed";
                    break;
                case 1:
                    document.getElementById("EP-status").innerText = "waiting...";
                    break;
                case 2:
                    document.getElementById("EP-status").innerText = "running";
                    break;
                case 3:
                    document.getElementById("EP-status").innerText = "finished";
                    break;
                case 4:
                    document.getElementById("EP-status").innerText = "ERROR";
                    break;
                case 5:
                    document.getElementById("EP-status").innerText = "deactivated (manual)";
                    break;
                default:
                    throw new Error("Unknown EP-status from shutter");
                    break;
            }
            document.getElementById("EP-status").innerText += ` (${rsp["alert"]["network"]["security-procedures"]})`;
            document.getElementById("relay-list").innerText = "";
            for (const el of rsp["relay"]["list"]) document.getElementById("relay-list").innerText += `${(el | 0)}`;
            document.getElementById("opening-motor").innerText = rsp["relay"]["opening-motor"];
            document.getElementById("closing-motor").innerText = rsp["relay"]["closing-motor"];
            document.getElementById("optoin-list").innerText = "";
            for (const el of rsp["optoin"]["list"]) document.getElementById("optoin-list").innerText += `${(el | 0)}`;
            document.getElementById("auto").innerText = rsp["optoin"]["auto"];
            document.getElementById("opened-sensor").innerText = rsp["optoin"]["opened-sensor"];
            document.getElementById("closed-sensor").innerText = rsp["optoin"]["closed-sensor"];
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
            document.getElementById("current-status-lock-movement").style.display = "none";
            document.getElementById("current-status-hardware-alert").style.display = "none";
            document.getElementById("current-status-network-alert").style.display = "none";
            document.getElementById("current-status").innerText = "Status unavailable";
            document.getElementById("open-button").disabled = true;
            document.getElementById("open-button").className = "gray";
            document.getElementById("close-button").disabled = true;
            document.getElementById("close-button").className = "gray";
            document.getElementById("abort-button").disabled = true;
            // disable other buttons
            document.getElementById("lock-movement-button").disabled = true;
            document.getElementById("unlock-movement-button").disabled = true;
            document.getElementById("restart-button").disabled = true;
            document.getElementById("force-restart-button").disabled = true;
            document.getElementById("reset-alert-status-button").disabled = true;
            document.getElementById("reset-EEPROM-button").disabled = true;
            // document.getElementById("log").disabled = true;
            document.getElementById("infoButton").disabled = true;
            // default info
            document.getElementById("firmware-version").innerText = "ND";
            document.getElementById("uptime").innerText = "ND";
            document.getElementById("shutter-status").innerText = "ND";
            document.getElementById("movement-status").innerText = "ND";
            document.getElementById("lock-movement").innerText = "ND";
            document.getElementById("network-status").innerText = "ND";
            document.getElementById("hardware-alert-status").innerText = "ND";
            document.getElementById("hardware-alert-status-description").innerText = "";
            document.getElementById("network-alert-status").innerText = "ND";
            document.getElementById("EP-status").innerText = "ND";
            document.getElementById("relay-list").innerText = "ND";
            document.getElementById("opening-motor").innerText = "ND";
            document.getElementById("closing-motor").innerText = "ND";
            document.getElementById("optoin-list").innerText = "ND";
            document.getElementById("auto").innerText = "ND";
            document.getElementById("opened-sensor").innerText = "ND";
            document.getElementById("closed-sensor").innerText = "ND";
            document.getElementById("hostname").innerText = "ND";
            document.getElementById("mac-address").innerText = "ND";
        });
}

function command(command) {
    // make request
    const request = new Request(`${DEBUG ? `http://${DEBUG_IP}` : ""}/api?json=${encodeURIComponent(JSON.stringify({ "cmd": command }))}`);
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
