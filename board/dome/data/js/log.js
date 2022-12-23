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

var follow = true;
var webserver_log;
var connection_error_alert = true;

function start() {
    setTimeout(getLogStatus, 0);
    Toast2.fire({
        icon: "info",
        title: "Connection initialization...",
    });
    const source = new ReconnectingEventSource(`${DEBUG ? `http://${DEBUG_IP}` : ""}/log_sse`);
    source.onopen = function (e) {
        console.log("Connection ok, start receiving updates.");
        if (connection_error_alert) {
            connection_error_alert = false;
            Toast.fire({
                icon: "success",
                title: "Connection established"
            });
        }
    };
    source.onmessage = function (e) {
        console.log("New update received!");
        const text = document.getElementById("text");
        text.value += `${text.value ? "\n\n" : ""}${e.data}`;
        if (follow) text.scrollTop = text.scrollHeight;
    };
    source.onerror = function (e) {
        if (!connection_error_alert) {
            connection_error_alert = true;
            Toast2.fire({
                icon: "error",
                title: "Connection lost",
                text: "Connection dropped, reinitialization in progress..."
            });
        }
        if (e.target.readyState != EventSource.OPEN) console.log("Disconnected, retry...");
        else console.log("Error during response handling.");
    };
}

function clearText() {
    Toast.fire({
        icon: "success",
        title: "Log page cleaned up"
    });
    document.getElementById("text").value = "";
}

function toggleFollow() {
    Toast.fire({
        icon: "success",
        title: `Follow ${follow ? "disabled" : "enabled"}`
    });
    document.getElementById("follow-button").value = follow ? "Follow enabled" : "Follow disabled";
    follow = !follow;
}

function getLogStatus() {
    const request = new Request(`${DEBUG ? `http://${DEBUG_IP}` : ""}/api?json=${encodeURIComponent(JSON.stringify({ "cmd": "server-logging-status" }))}`);
    const button = document.getElementById("disable-log-button");
    fetchTimeout(request)
        .then(rsp => {
            webserver_log = rsp;
            button.value = `${rsp ? "Disable" : "Enable"} 'status' command log`;
        })
        .catch(error => {
            webserver_log = undefined;
            button.value = "Toggle 'status' command log";
        });
}

function toggleLog() {
    const request = new Request(`${DEBUG ? `http://${DEBUG_IP}` : ""}/api?json=${encodeURIComponent(JSON.stringify({ "cmd": "server-logging-toggle" }))}`);
    fetchTimeout(request)
        .then(rsp => {
            if (rsp != "done") throw new Error(rsp);
            Toast.fire({
                icon: "success",
                title: webserver_log == undefined ? "Command sent" : webserver_log == true ? "'status' log disabled" : "'status' log enabled"
            });
            setTimeout(getLogStatus, 0);
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
