# Shutter controller code: documentation

- [Shutter controller code: documentation](#shutter-controller-code-documentation)
  - [Folders and files description](#folders-and-files-description)
  - [Hardware description](#hardware-description)
  - [Operation and logic](#operation-and-logic)
    - [State of the shutter and of the PLC](#state-of-the-shutter-and-of-the-plc)
    - [Opening and closing](#opening-and-closing)
      - [Automatic mode](#automatic-mode)
      - [Manual mode](#manual-mode)
    - [Alert states](#alert-states)
      - [Hardware alert](#hardware-alert)
      - [Network alert](#network-alert)
    - [Automatic-manual control and user input](#automatic-manual-control-and-user-input)
  - [Communication with the board](#communication-with-the-board)
    - [API description](#api-description)

We use the [KMP PRODINo ESP32 Ethernet v1](https://kmpelectronics.eu/products/prodino-esp32-ethernet-v1/) as the controller for the shutter movement.

![KMP PRODINo ESP32 Ethernet v1](https://kmpelectronics.eu/wp-content/uploads/2019/11/ProDinoESP32_E_5.jpg)

## Folders and files description

- [`data/`](data/). Files that will go to the filesystem, i.e. web pages.

- [`include/`](include/)

  - [`global_definitions.hpp`](include/global_definitions.hpp). Contains global variables definitions and global `#define` needed in the code. The variables are declared as `extern` in order to be able to use them in the various files located in the `src` folder; they are initialized in the `src/main.cpp` file. All imports are located here.

- [`lib/`](lib/)

  - [`ProDinoESP32`](lib/ProDinoESP32), version 2.0.0 commit 8ffb407, [official repository](https://github.com/kmpelectronics/ProDinoESP32). Library to use board features such as relay, optoin, ethernet, ...

- [`src/`](src/)

  - [`auxiliary_functions.cpp`](src/auxiliary_functions.cpp). Contains all the auxiliary functions, such as Wi-Fi connection, LED flashing, ...

  - [`main.cpp`](src/main.cpp). Contains the core of the code: initialization of the variables, setup, loop and any secondary tasks.

  - [`web_server.cpp`](src/web_server.cpp). Contains the web server with all its routes.

## Hardware description

The purchased PRODINo is equipped with an ethernet port, initially used as the primary communication port. After extensive tests and stress tests, it turned out that the Wi-Fi is more stable and responsive.

## Operation and logic

### State of the shutter and of the PLC

The global status can be checked from the web page or with the `status` command.

The board determines the state of the shutter using the limit switch sensors and the state of the relays in the following order:

- If one of the two relays is on, the shutter is considered to be opening or closing based on which relay is working. This mode only works if the shutter is in automatic mode, as in manual mode the relays are bypassed and there is no way to know if the shutter is moving or not.

- If one of the two limit switches is triggered and the relays are off, the shutter is considered closed or opened based on which limit switch is triggered.

- If none of the limit switches are triggered and the relays are off, the shutter is considered partially opened.

### Opening and closing

#### Automatic mode

The opening and closing operations of the shutter are controlled with the `open` and `close` commands. These requests only turn on the relay. The motion state check is done in the loop. When one of the two limit switch sensors (opening or closing) is reached, the board waits a few seconds to complete the opening/closing and then switches off the relays.

To early stop the motion, use the `abort` command.

If you give the `open` command while the shutter is closing, it stops and then opens (and vice versa).

A software lock can be activated to reject shutter change state commands.

#### Manual mode

The shutter is controlled by buttons.

### Alert states

#### Hardware alert

The hardware alert state completely blocks the shutter opening and closing remote control and persists even in the event of board restart or flashing of a new code as the state is saved in the EEPROM.

If the board detects that the relays remain on for longer than necessary (about 20 seconds, the time for a total opening or closing), then it stops everything and enters an alert state; in this case, in fact, it is very probable that mechanical problems have occurred in the shutter (such as a damage of a link in the engine transmission chains) and therefore it is vital to stop the engines; however, note that adverse conditions such as the presence of ice can slow down the movement of the shutter, causing the alert to go on (it shouldn't happen because the times have been calibrated, but you never know).

The alert status is activated even if the shutter does not deactivate the closing limit switch while it opens (or vice versa) within a few seconds.

The alert status can be checked with the `status` command and the only way to reset it is to use the `reset-alert-status` command. It is essential that the alert status is reset _only after careful checks on the state of the shutter mechanics_.

#### Network alert

If the shutter detects the absence of the internet network (LAN or WAN) for more than ten minutes, the network alert goes on: this status completely blocks the shutter opening and closing remote control, and causes the immediate closure of the shutter. Network discovery is done by pinging `www.google.com`.

This state of alert is necessary to ensure the safety of the instrumentation in the dome in the event of an internet failure in the middle of a remote observing session. For this reason, it is not possible to override this command or manually reset its state.

However, it is possible to move the shutter by placing it in manual mode and moving it with the buttons on the panel. _Attention, the alert status check is continuous: if the shutter is opened manually and then the switch is set to automatic, the shutter will close again._

### Automatic-manual control and user input

The dome can be controlled remotely only if the automatic-manual switch is positioned on "automatic": in this case, the manual controls are blocked, allowing only remote control. If the switch is in the "manual" position, remote control with the board is blocked, allowing only manual control with the buttons. The board monitors the state of the switch using an optical input.

## Communication with the board

You can interact with the board using:

- **Web page** at the `/` route; authentication required.
- **API** at the `/api` route.

The board log is at the `/log` route.

### API description

The APIs are accessible through http GET requests of the type:

```
/api?json={"cmd":"command"}
```

where the possible values of `command` are:

- Shutter related functions:

  - `abort`: stops the movement.
  - `close`: close the shutter.
  - `open`: open the shutter.
  - `lock-movement`: software lock of the shutter movement.
  - `unlock-movement`: software unlock of the shutter movement.

- Board management:

  - `reset-alert-status`: reset the alert status of the shutter to `false`.
  - `reset-EEPROM`: reset the EEPROM and restart the board to make the reset effective.
  - `restart`: restart the board (graceful restart).
  - `force-restart`: restart the board (hard restart).
  - `server-logging-toggle`: toggle webserver logging state.
  - `server-logging-status`: return the webserver log status.

- Status:

  - `status`: board status json.

The response (except for the cases indicated) will be in JSON of the type:

```json
{
  "rsp": "message"
}
```

where the possible values of `message` are:

- `done` in case of successful request.
- a message reporting the type of error found.

The response of the `status` command is instead more extensive and an example is shown here:

```json
{
  "rsp": {
    "firmware-version": "v1.0.3",
    "uptime": "0 days, 0 hours, 0 minutes, 21 seconds",
    "shutter-status": 1,
    "movement-status": false,
    "lock-movement": false,
    "network-status": true,
    "alert": {
      "hardware": {
        "status": false,
        "description": ""
      },
      "network": {
        "status": false,
        "security-procedures": 0
      }
    },
    "relay": {
      "list": [
        false,
        false,
        false,
        false
      ],
      "opening-motor": false,
      "closing-motor": false
    },
    "optoin": {
      "list": [
        true,
        true,
        false,
        false
      ],
      "auto": true,
      "closed-sensor": true,
      "opened-sensor": false
    },
    "wifi": {
      "hostname": "shutter-controller",
      "mac-address": "AA:BB:CC:DD:EE:FF"
    }
  }
}
```

Note that the `shutter-status` parameter indicates the status of the shutter, whose possible values are:

- `-1`: shutter partially opened
- `0`: shutter opened
- `1`: shutter closed
- `2`: shutter opening (only if automatic)
- `3`: shutter closing (only if automatic)
