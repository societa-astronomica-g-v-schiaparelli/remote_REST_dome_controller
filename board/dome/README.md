# Dome controller code: documentation

- [Dome controller code: documentation](#dome-controller-code-documentation)
  - [Descrizione cartelle e file](#descrizione-cartelle-e-file)
  - [Folders and files description](#folders-and-files-description)
  - [Hardware description](#hardware-description)
  - [Operation and logic](#operation-and-logic)
    - [State of the shutter and of the PLC](#state-of-the-shutter-and-of-the-plc)
    - [PLC encoder and RS485 communication](#plc-encoder-and-rs485-communication)
    - [Switchboard ignition](#switchboard-ignition)
      - [Automatic mode](#automatic-mode)
      - [Manual mode](#manual-mode)
    - [Dome rotation](#dome-rotation)
      - [Automatic mode](#automatic-mode-1)
      - [Manual mode](#manual-mode-1)
    - [Alert states](#alert-states)
      - [Power failure](#power-failure)
    - [Automatic-manual control and user input](#automatic-manual-control-and-user-input)
  - [Communication with the board](#communication-with-the-board)
    - [API description](#api-description)

We use the [KMP PRODINo ESP32 Ethernet v1](https://kmpelectronics.eu/products/prodino-esp32-ethernet-v1/) as the controller for the shutter movement.

![KMP PRODINo ESP32 Ethernet v1](https://kmpelectronics.eu/wp-content/uploads/2019/11/ProDinoESP32_E_5.jpg)

## Descrizione cartelle e file

## Folders and files description

- [`data/`](data/). Files that will go to the filesystem, i.e. web pages.

- [`include/`](include/)

  - [`global_definitions.hpp`](include/global_definitions.hpp). Contains global variables definitions and global `#define` needed in the code. The variables are declared as `extern` in order to be able to use them in the various files located in the `src` folder; they are initialized in the `src/main.cpp` file. All imports are located here.

- [`lib/`](lib/)

  - [`CustomOptoIn`](lib/CustomOptoIn), version 1.0.0. Library for reading custom optical inputs.

  - [`ProDinoESP32`](lib/ProDinoESP32), version 2.0.0 commit 8ffb407, [official repository](https://github.com/kmpelectronics/ProDinoESP32). Library to use board features such as relay, optoin, ethernet, ...

- [`src/`](src/)

  - [`auxiliary_functions.cpp`](src/auxiliary_functions.cpp). Contains all the auxiliary functions, such as Wi-Fi connection, LED flashing, ...

  - [`main.cpp`](src/main.cpp). Contains the core of the code: initialization of the variables, setup, loop and any secondary tasks.

  - [`web_server.cpp`](src/web_server.cpp). Contains the web server with all its routes.

## Hardware description

The purchased PRODINo is equipped with an ethernet port, initially used as the primary communication port. After extensive tests and stress tests, it turned out that the Wi-Fi is more stable and responsive.

A custom board has been added to the PRODINo to have 4 more optically isolated inputs, which connects to the digital pins.

Furthermore, the card communicates via serial 485 with a custom PLC for reading the encoder and playing the pre-rotation buzzer.

## Operation and logic

### State of the shutter and of the PLC

The global status can be checked from the web page or with the `status` command.

The board determines the position of the dome through the encoder mounted on the transmission pinion, read by the custom PLC.

### PLC encoder and RS485 communication

The PLC code is not provided in this repository.

Commands:

- `0x42` (`B`): turn on buzzer, turn off with any other command.

- `0x52` (`R`): dome position request, in response two hex bytes with dome position in degrees, e.g. `0x00B4 -> 180dec -> SOUTH`.

- `0x57` (`W`): write dome position, the command is followed by two hex bytes of the position.

- `0x5A` (`Z`): zero request, the command must be followed by the rotation of the dome towards the zero switch, upon reaching which the command and the two hex bytes of the zero position are retransmitted.

- `0x44` (`D`): restore the default configuration.

- `0x23` (`#`): disable zero switch, only for first calibration.

- `0xC0`: write configuration data command, total 9 hex bytes:

  - byte 1: `0xC0`
  - byte 2: encoder steps / dome rotation degree
  - byte 3 and 4: encoder steps / one dome revolution
  - byte 5 and 6: first zero switch position in degrees
  - byte 7 and 8: second zero switch position in degrees
  - byte 9: checksum, calculated as `neg(sum(bytes)) + 1` (note that the sum is done in a byte, so the overflow must be used), in C++:

    ```c++
    uint8_t checksum{};
    for (uint8_t i : config) checksum += i;
    checksum = ~checksum + 1;
    ```

- `0xC1`: configuration data read request, response 8 byte hex (7 data as above + checksum).

### Switchboard ignition

#### Automatic mode

The switchboard can be turned on with the appropriate network request.

#### Manual mode

The loop checks if the corresponding button is pressed for 2 seconds and, if so, turns on the ignition.

### Dome rotation

To calibrate the dome position you can use the `find-zero` request. In any case, the dome calibrates itself when it encounters the zero switch. The calibration procedure should therefore only be used if the dome loses its reference.

The encoder PLC does not keep in memory the position reached when it is switched off, therefore at start up it is necessary to write the last known position with the command `0x57` (`W`). For this reason, at the end of each movement, the PRODINo writes the position reached in EEPROM. In case of current loss during a movement of the dome, it is therefore necessary to implement the calibration procedure (the saved position does not coincide with the real position as it has not yet been written).

#### Automatic mode

The rotation of the dome is controlled with the `slew-to-az` request. This request only turn on the motor control relays. The motion state check is done in the loop. When the correct position is reached (checked through the encoder), the board switches off the relays.

To early stop the motion, use the `abort` command.

Giving a move command while the dome is already rotating updates the target or, if it is on the opposite side, stops the rotation and restarts it.

#### Manual mode

Rotation is controlled by buttons.

### Alert states

#### Power failure

Although the entire dome board is placed under an UPS, to avoid leaving the dome open and in an inconsistent state, one of the board's inputs has been assigned to control the current flow upstream of the UPS. If this fails, a function is triggered in the `loop` to tell the entire system to close the shutter and shut down.

### Automatic-manual control and user input

The dome can be controlled remotely only if the automatic-manual switch is positioned on "automatic": in this case, the manual controls are blocked, allowing only remote control. If the switch is in the "manual" position, remote control with the board is blocked, allowing only manual control with the buttons. The board monitors the state of the switch using an optical input.

In case of automatic-manual switching, the automatic management and control services - such as the automatic shutdown or follow procedure - are enabled or disabled (and reset).

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

- Dome-related functions:

  - `abort`: stop the movement.
  - `slew-to-az`: move the dome to the specified azimuth, requires the `az-target` key containing an integer between 0 and 360.
  - `park`: park the dome.
  - `find-zero`: calibrate the dome looking for the zero switch.

- Encoder-related functions:

  - `encoder-readconf`: read the PLC configuration.
  - `encoder-writeconf`: write a new PLC configuration, requires the `config` key containing a seven byte array (see encoder parameters description).
  - `encoder-resetconf`: reset the PLC configuration.
  - `encoder-disablezero`: read the PLC configuration.

- Board management:

  - `ignite-switchboard`: switch on the dome switchboard.
  - `reset-EEPROM`: reset the EEPROM and restart the board to make the reset effective.
  - `restart`: restart the board (graceful restart).
  - `force-restart`: restart the board (hard restart).
  - `turn-off`: save essential parameters and prepare the board for shutdown.
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

The responses of other commands are instead more extensive and an example is shown here:

- `encoder-readconf`:

  ```json
  {
    "bytes": [
      47,
      65,
      251,
      0,
      248,
      0,
      37,
      120
    ],
    "decoded data": {
      "steps / degree": 47,
      "steps / dome revolution": 16891,
      "zero azimuth": 248,
      "zero offset steps": 37,
      "checksum": 120
    },
    "validation": true
  }
  ```

- `status`:

  ```json
  {
    "rsp": {
      "firmware-version": "v1.0.3",
      "uptime": "0 days, 0 hours, 23 minutes, 32 seconds",
      "dome-azimuth": 91,
      "target-azimuth": 91,
      "movement-status": false,
      "in-park": true,
      "finding-park": false,
      "finding-zero": false,
      "relay": {
        "list": [
          false,
          false,
          false,
          false
        ],
        "cw-motor": false,
        "ccw-motor": false,
        "switchboard": false
      },
      "optoin": {
        "list": [
          false,
          true,
          true,
          false,
          false,
          false,
          true,
          false
        ],
        "auto": true,
        "switchboard-status": true,
        "ac-presence": true,
        "manual-cw-button": false,
        "manual-ccw-button": false,
        "manual-ignition": false
      },
      "wifi": {
        "hostname": "dome-controller",
        "mac-address": "AA:BB:CC:DD:EE:FF"
      }
    }
  }
  ```
