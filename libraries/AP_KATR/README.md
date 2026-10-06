# KATR CAN control with ArduPilot Rover

This custom firmware connects Rover steering and throttle outputs to vehicle's
radio-replacement CAN interface. It sends only standard CAN ID `0x0F` on KATR
CANBUS **1ch**. Engine stop, maximum-speed mode, and leg mode are assigned through
ordinary `RCn_OPTION` parameters. The driver has no separate `KATR_*` parameters.

The protocol source is sections 2, 3, and 6 and the protocol definition sheets.
The workbook is supplied separately
and is not included in this repository. This implementation and guide were
AI-assisted; a human integrator must review the change and verify it on KATR.

## Protocol and vehicle preparation

Use classic CAN at **500,000 bit/s**, standard **11-bit ID `0x0F`**, DLC **8**, with
one command every **30 ms**. The workbook specifies Intel data layout and an
87.5% sample point. Every command field occupies one byte, so no multibyte
endianness conversion is needed.

| Byte | Signal | Signed value and meaning |
| --- | --- | --- |
| 0 | Steering / RadioCh1 | -68 = full left, 0 = neutral, +68 = full right |
| 1 | Drive / RadioCh2 | -68 = full forward, 0 = neutral, +68 = full reverse |
| 2 | Engine stop / RadioCh3 | -92 = normal, +92 = stop |
| 3 | Reserved | 0 |
| 4 | Maximum speed / RadioCh5 | -92 = high speed, +92 = low speed |
| 5 | Leg mode / RadioCh6 | -92 = rough ground, +92 = flat ground |
| 6–7 | Reserved | 0 |

The workbook's note that the original receiver normally mirrors CH1 onto CH3
conflicts with the formal two-value command table. The integration owner
confirmed that this driver must use **-92 for normal and +92 for engine stop**.
Returning to normal releases the stop command; it does not command engine start.
Engine-stop also suppresses steering and drive commands in this driver.

Prepare the KATR interface according to the workbook and the KATR maintenance
manual:

1. Shut down KATR before working on its harness. Disconnect the original KATR
   radio receiver harness; its messages would conflict with the replacement
   `0x0F` messages. Insulate the disconnected connector.
2. Connect the chosen CubeRed CAN port to **Vechiel's CANBUS 1ch**, using CAN H, CAN L,
   and the appropriate signal reference according to the board and KATR harness
   drawings. The transmitter receiver used by Rover connects to CubeRed's RC
   input, not to KATR's original CAN receiver connector. Confirm connector
   pinouts and voltage ratings; do not connect KATR vehicle power to a CubeRed
   CAN connector's low-voltage supply pin.
3. The workbook calls for a **120-ohm termination** at this interface for both
   engine and electric KATR variants. Account for termination already present in
   the CubeRed/carrier and harness before adding one. The CubeRedPrimary hardware
   definition configures CAN termination-control pins; confirm the actual
   carrier's termination arrangement and measure the completed, unpowered bus.
4. Set KATR's external-command operation mode to **normal operation mode**.
In the maintenance menu this is value 4, left LED blinking;
   follow the workbook's maintenance sequence and power-cycle to retain it.
5. Set KATR's physical operation-selection switch to **radio**, indicated
   by a blinking lamp. This is distinct from selecting the external-command
   operation mode.

The workbook does not define what KATR does after CAN communication disappears.
Verify that behavior on the actual machine before operation. This interface has
no command acknowledgments, engine-state feedback, or measured-speed telemetry.
Rover cannot confirm that KATR received or obeyed a command.

## Build custom firmware

Use this modified source tree, including its normal submodule revisions. Set up
the supported compiler and dependencies using ArduPilot's
[Linux build-environment instructions](https://ardupilot.org/dev/docs/building-setup-linux.html).
For Windows development, use the documented
[Windows 11 WSL environment](https://ardupilot.org/dev/docs/building-setup-windows11.html),
then copy the resulting `.apj` to Windows for upload. Run `./waf` from the
repository root without `sudo`.

Build the CubeRed **primary** processor's Rover firmware:

```sh
./waf configure --board CubeRedPrimary --enable-KATR
./waf rover
```

The upload artifact is `build/CubeRedPrimary/bin/ardurover.apj`. Keep a copy of
the source revision, build output, and parameter backup with the firmware.
`CubeRedSecondary` is a different target; do not upload this primary image to
the secondary processor.

`AP_KATR_ENABLED` is an optional compile-time feature. `--enable-KATR` includes
it; `--disable-KATR` excludes it. The runtime selection is `CAN_D1_PROTOCOL=15`.
A stock firmware download or a public custom-build server that does not contain
this source change will not provide the driver merely because that parameter is
set to 15.

Build SITL from the same source:

```sh
./waf configure --board sitl --enable-KATR
./waf rover
```

## Upload with Mission Planner

Back up existing parameters before changing firmware. Keep the machine disabled
and disconnect actuator/engine power while programming; use USB to communicate
with the CubeRed. These steps assume a compatible bootloader is already present.

1. Select the CubeRed COM port without pressing **Connect**.
2. Open **SETUP > Install Firmware > Load custom firmware** and choose
   `ardurover.apj` from `build/CubeRedPrimary/bin`.
3. If the custom-firmware link is hidden, select **CONFIG > Planner > Layout >
   Advanced**. Follow the USB reconnect prompts and wait for upload verification
   to finish.
4. Connect after reboot, confirm that Rover is running, and read the parameters.

The official
[Mission Planner custom-firmware procedure](https://ardupilot.org/planner/docs/common-loading-firmware-onto-pixhawk.html)
describes the upload controls and bootloader prompts.

## Upload with QGroundControl

Use the desktop application. Save the current parameters from **Vehicle Setup >
Parameters > Tools > Save to file** before disconnecting.

1. Disconnect USB and vehicle battery power, then open **Vehicle Setup >
   Firmware**.
2. Connect CubeRed directly by USB. Select the ArduPilot firmware family and
   **Advanced settings**, then the option to install a custom/local firmware
   file.
3. Choose `build/CubeRedPrimary/bin/ardurover.apj`, start the update, and wait for
   completion and reboot. Confirm the connected vehicle reports Rover.

QGroundControl documents the desktop upload process in
[Loading Firmware](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/setup_view/firmware.html).
Menu wording varies with the installed release.

## Configure the CAN driver and outputs

The following example uses **CubeRed CAN1**, driver slot **1**, and KATR CANBUS
**1ch**. The physical CubeRed port and driver slot are separate choices. If using
CubeRed CAN2, use `CAN_P2_DRIVER=1` and `CAN_P2_BITRATE=500000` instead. Keep the
selected port dedicated to this KATR bus.

| Parameter | Value | Purpose |
| --- | --- | --- |
| `CAN_P1_DRIVER` | 1 | Connect physical CAN1 to driver 1 |
| `CAN_P1_BITRATE` | 500000 | KATR nominal CAN rate |
| `CAN_D1_PROTOCOL` | 15 | KATR driver |
| `SERVO1_FUNCTION` | 26 | Rover ground steering |
| `SERVO3_FUNCTION` | 70 | Rover throttle |
| `SERVO1_MIN`, `SERVO3_MIN` | 1000 | Negative output endpoint |
| `SERVO1_TRIM`, `SERVO3_TRIM` | 1500 | Neutral output |
| `SERVO1_MAX`, `SERVO3_MAX` | 2000 | Positive output endpoint |
| `SERVO1_REVERSED`, `SERVO3_REVERSED` | 0 | Initial direction setting |

Write the CAN settings and reboot. A second parameter download may be needed
after enabling a CAN driver before its `CAN_D1_*` parameters appear. Read back
the values after the reboot. The driver emits classic CAN frames even on
CAN-FD-capable CubeRed hardware.

The driver uses the final servo outputs assigned to ground steering and
throttle. Keep one output assignment for each function. It normalizes PWM about
trim and respects output reversal and limits; no physical PWM connection from
these channels to KATR is needed. Start with the symmetric endpoints above.
The driver converts positive Rover throttle to KATR's negative forward command.
Do not reverse throttle merely to compensate for that documented wire sign.
If commissioning identifies an installation-specific reversal, change the
corresponding `SERVOx_REVERSED` and verify the CAN bytes and physical direction.

These channels can still produce physical PWM. Leave unrelated actuators
disconnected from them. Skid steering, separate left/right motor functions,
wheel-speed feedback, and physical KATR dynamics are outside this interface.

## Assign transmitter switches

Calibrate the transmitter using the ground station's radio calibration and
verify channel ranges. Assign three unused RC inputs; for example:

| Parameter | Option | LOW position | MIDDLE position | HIGH position |
| --- | --- | --- | --- | --- |
| `RC7_OPTION` | 250: KATR engine stop | Normal/run permitted | Stop | Stop |
| `RC8_OPTION` | 251: KATR high speed | Low speed | Low speed | High speed |
| `RC9_OPTION` | 252: KATR rough ground | Flat ground | Flat ground | Rough ground |

`RC7`, `RC8`, and `RC9` are examples: use any three available `RCn_OPTION`
channels. The usual auxiliary-function switch positions are based on low,
middle, and high RC PWM; verify the actual decoded positions and transmitter
reversal during commissioning. A two-position switch can use LOW and HIGH.
Assign one input to each function and keep engine stop readily accessible.
Options 250–252 are custom-integration numbers in this firmware.

In **Mission Planner**, open **CONFIG > Full Parameter List**, search by the
exact parameter name, enter the numeric value in the **Value** cell, then select
**Write Params**. Stock metadata may omit the new labels from the **Options**
dropdown. The raw Value cell is independent of that dropdown, as implemented in
the official
[Mission Planner parameter editor](https://github.com/ArduPilot/MissionPlanner/blob/master/GCSViews/ConfigurationView/ConfigRawParams.cs).

In **QGroundControl**, open **Vehicle Setup > Parameters**, search the exact
parameter name, select its row, and save the value. For a dropdown that lacks a
new option, select **Advanced settings > Manual Entry** and enter `15`, `250`,
`251`, or `252` as appropriate. This numeric-entry control is implemented in the
official
[QGroundControl parameter dialog](https://github.com/mavlink/qgroundcontrol/blob/master/src/QmlControls/ParameterEditorDialog.qml).
The general search, save, refresh, and backup workflow is described in
[QGroundControl Parameters](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/setup_view/parameters.html).

After writing, refresh/reconnect and verify the numeric values returned by the
vehicle. Do not select an unrelated listed option because the KATR label is
missing. If using a release without a numeric-entry control, save parameters
to a file, edit only the required named values while retaining that application's
file format, and use its parameter-file load function; review the proposed
changes and read back the result.

Firmware parameter values travel over MAVLink; descriptive labels normally
come from ground-station metadata and may remain stale. To generate metadata
from this source tree for inspection or a custom ground-station distribution:

```sh
python3 Tools/autotest/param_metadata/param_parse.py --vehicle Rover --format xml
```

This writes `apm.pdef.xml` in the working directory. Generating it does not
automatically install it in Mission Planner or QGroundControl. Numeric entry
and readback remain the configuration path when bundled labels are outdated.

## Stop behavior and failsafes

- At boot, the driver requests engine stop until the engine-stop auxiliary
  function receives a valid LOW input. If no such option is assigned, engine stop
  remains requested. Speed and leg defaults are low speed and flat ground.
- Disarming or engaging CubeRed's hardware safety inhibits both steering and
  drive. It preserves the requested engine-stop state, allowing the machine's
  engine to remain running while Rover is disarmed.
- Engine-stop option 250 in MIDDLE/HIGH, or Rover's servo emergency stop,
  requests engine stop and neutral steering/drive.
- If servo output updates stop for more than 200 ms, the driver requests stop
  and neutral commands. This detects stale output updates while the CAN thread
  is running; it cannot deliver a stop through a broken or disconnected bus.
- RC loss follows Rover's configured failsafe action. The KATR driver does not
  independently convert all RC loss into engine stop, because autonomous modes
  may be configured to continue. Choose and test Rover's RC, ground-station,
  and battery failsafes for the intended operation. The most recent auxiliary
  mode selections are retained until changed or rebooted.

Ordinary disarmed ground-station motor tests do not move KATR through this
driver. Keep KATR's independent physical stop available throughout testing.

## SITL and commissioning verification

Run the integration test from the repository root with KATR included in the
test build:

```sh
Tools/autotest/autotest.py --waf-configure-args='--enable-KATR' build.Rover test.Rover.KATR
```

For interactive Rover SITL, start the previously built executable through the
normal simulation launcher:

```sh
Tools/autotest/sim_vehicle.py -v Rover -f rover --no-rebuild --console --map
```

Connect the ground station to the simulator's MAVLink endpoint and apply the
same CAN and RC parameters. In SITL, also set `SIM_CAN_TYPE1=1` and reboot to
enable the simulated CAN1 multicast interface; the automated test sets this
itself. CAN1 uses multicast `239.65.82.0:57732` by default. The standard Rover
SITL physics model is not a
KATR hydraulic/leg model. It is useful for Rover output and MAVLink
configuration checks; CAN byte verification requires the KATR integration
test or a CAN capture, and physical response requires KATR hardware.

Before enabling physical motion, capture transmitted commands with a CAN
analyzer and verify all of the following:

1. Standard ID `0x0F`, DLC 8, approximately 30 ms spacing, and reserved bytes
   3/6/7 always zero. No other KATR command IDs should be transmitted.
2. Neutral drive/steering at startup, while disarmed, and with safety engaged.
   With low-speed, flat-ground, and engine-stop selected, the payload is
   `00 00 5C 00 5C 5C 00 00`.
3. LOW engine switch changes byte 2 to `A4` (-92); MIDDLE/HIGH changes it to
   `5C` (+92) and neutralizes movement. Verify the physical engine-stop action.
4. With permitted motion, positive Rover steering produces positive/right
   KATR steering; positive Rover throttle produces negative/forward KATR drive.
   Full-scale +68 is `44` and -68 is `BC`.
5. High-speed and rough-ground selections each produce `A4` in their respective
   bytes; low-speed and flat-ground each produce `5C`.
6. Disarm, safety, servo emergency stop, RC loss, ground-station loss, CubeRed
   reboot, and unplugged CAN have the expected physical results. CAN loss must
   be evaluated independently of the driver's stale-output test.

Only after stationary checks pass, validate slow-speed motion in a controlled
area, then tune Rover steering and speed control for the actual KATR. The
command magnitude is a normalized request, not a measured speed in m/s.

### Verification record

Local verification on 2026-10-04:

- Rover SITL build with `--enable-KATR`: passed.
- CubeRedPrimary Rover build with `--enable-KATR`: passed; generated
  `build/CubeRedPrimary/bin/ardurover.apj`.
- Rover SITL build with KATR enabled and DroneCAN disabled: passed.
- Rover SITL build with KATR disabled: passed.
- `test.Rover.KATR`: passed using actual SITL CAN multicast frames. Checks cover
  payload layout, periodic transmission, independent steering/throttle and
  mode fields, endpoints and midpoint scaling, servo reversal, engine stop,
  safety, emergency stop, radio failsafe to Hold, disarming, and a simulated
  main-loop lockup that triggers the stale-output engine stop.
- Rover parameter metadata validation and XML generation: passed; verified
  CAN protocol 15 and RC options 250–252 in the generated metadata.
- Python lint, new driver C++ formatting, and patch whitespace checks: passed.

No CubeRed upload, GCS GUI session, or physical KATR commissioning has been
performed. The SITL test validates transmitted requests, not KATR's physical
response. Record the firmware revision, board/carrier, parameters, CAN captures,
and the operator's hardware observations before deployment.
