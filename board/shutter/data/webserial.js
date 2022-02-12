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
const DEBUG_IP = ""

const UPDATE_TIME = 1000;
var ws;
var follow = true;

function start() {
    createWebSocket();
    setTimeout(getLogStatus, 0);
    setInterval(handleWebSocket, UPDATE_TIME);
}

function createWebSocket() {
    ws = new WebSocket(`ws://${DEBUG ? DEBUG_IP : window.location.hostname}/webserialws`);
    ws.onmessage = event => {
        const text = document.getElementById("text");
        text.value += `${text.value ? "\n\n" : ""}${event.data}`;
        if (follow) text.scrollTop = text.scrollHeight;
    };
}

function handleWebSocket() {
    const msg = document.getElementById("msg");
    if (ws.readyState == WebSocket.OPEN) {
        ws.send("");
        msg.style.display = "none";
    }
    if (ws.readyState == WebSocket.CLOSED) {
        msg.style.display = "";
        createWebSocket();
    }
}

function clearText() {
    document.getElementById("text").value = "";
}

function toggleFollow() {
    document.getElementById("follow-button").value = follow ? "Enable follow" : "Disable follow";
    follow = !follow;
}

function getLogStatus() {
    const request = new Request(`${DEBUG ? `http://${DEBUG_IP}` : ""}/api?json=${encodeURIComponent(JSON.stringify({ "cmd": "server-logging-status" }))}`);
    const button = document.getElementById("disable-log-button");
    fetchTimeout(request)
        .then(rsp => { button.value = `${rsp ? "Disable" : "Enable"} webserver log`; })
        .catch(error => { button.value = "Toggle webserver log"; });
}

function toggleLog() {
    const request = new Request(`${DEBUG ? `http://${DEBUG_IP}` : ""}/api?json=${encodeURIComponent(JSON.stringify({ "cmd": "server-logging-toggle" }))}`);
    const button = document.getElementById("disable-log-button");
    fetchTimeout(request)
        .then(rsp => {
            if (rsp != "done") throw new Error(rsp);
            button.className = "green";
            setTimeout(getLogStatus, 0);
            setTimeout(() => { button.className = "gray"; }, 2000);
        })
        .catch(error => {
            button.className = "red";
            setTimeout(() => { button.className = "gray"; }, 2000);
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
