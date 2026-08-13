# Diesel_Heater_Turbo_ESP32

ESP32 diagnostic/logger/controller for common 3-wire Chinese diesel heaters using the 25,000-baud "blue-wire" controller bus.

## v0.1 goal

**Start passive. Diagnose first.** The ESP32 listens in parallel with the factory LCD controller and gives you a phone-friendly live dashboard with graphs and CSV logging.

Telemetry currently decoded:

- controller/cabin temperature
- desired temperature
- heater supply voltage
- fan RPM and fan voltage
- heat-exchanger temperature
- glow-plug voltage and current
- fuel-pump frequency
- run state
- error code
- raw 48-byte frame

The dashboard keeps a 30-minute rolling graph/history (5 s samples) and can download that history as CSV.

## Safety architecture

This project **does not bypass the heater ECU's combustion safeties**. Over-temperature, failed ignition/flame, fan/motor faults, sensor faults and cooldown remain the heater ECU's job.

The optional control module only emulates the existing controller's Power / Up / Down buttons through external transistors or optocouplers. It is locked by default.

## First wiring: passive monitor only

Keep the factory LCD plugged in.

```text
Heater/controller harness

BLACK / GND  ---------------- ESP32 GND
BLUE / DATA  ---- level interface ---- ESP32 GPIO16 (RX)
RED / POWER  ---------------- NOT NEEDED for first test

ESP32 powered from USB for first test.
```

### Important

Measure the harness before connecting it. Do not assume wire colours are electrically identical between heater variants.

If the blue data line is 5 V logic, use a proper level shifter or an RX-only resistor divider (for example 10 kΩ from BLUE to GPIO and 20 kΩ from GPIO to GND). If you measure anything significantly above 5 V, stop and use a suitable interface instead.

Do **not** connect heater data directly to an ESP32 GPIO until the voltage is known.

## Build / flash

1. Install VS Code + PlatformIO, or use PlatformIO CLI.
2. Copy `include/secrets.h.example` to `include/secrets.h` if you want it to join your Wi-Fi.
3. Connect the ESP32 by USB.
4. Build/upload:

```bash
pio run -t upload
```

If Wi-Fi credentials are blank or fail, it creates:

- SSID: `Diesel-Heater-Lab`
- password: `dieselheater`
- dashboard: `http://192.168.4.1/`

## Dashboard

The dashboard includes live cards and rolling graphs for:

- temperature
- supply voltage
- fan RPM
- pump frequency / glow current

There is also:

- raw frame display
- valid/rejected frame counters
- CSV history download
- browser firmware `.bin` update page
- Arduino OTA support

## Optional button control

Pins are currently defined as:

- GPIO25 -> external transistor/opto -> Power button pad
- GPIO26 -> external transistor/opto -> Up button pad
- GPIO27 -> external transistor/opto -> Down button pad

Do **not** connect these GPIOs directly to the controller buttons. Use a transistor/opto interface and confirm the controller pad voltages first.

After the hardware is verified, button control can be explicitly unlocked with:

```text
POST /api/control/unlock?confirm=I_WIRED_TRANSISTORS
```

The thermostat logic will not automatically restart the heater while a real error is reported, and it avoids commands during startup/stopping/cooldown states.

## Protocol basis / acknowledgements

The common blue-wire protocol was reverse engineered by the Chinese-diesel-heater community, notably Ray Jones / Afterburner. This project uses a clean implementation based on publicly documented behaviour and field locations demonstrated by projects including:

- `daoudeddy/cdh-esphome`
- `timmchugh11/Chinese-Diesel-Heater---ESPHome`
- `TMakins/CDH_I2C_Interface`

Controller variants exist. The passive logger and raw-frame view are deliberately the first step so we can verify your particular heater before enabling control.

## Next development pass

- verify your controller's exact 48-byte traffic
- add fault/event timeline markers to graphs
- persistent SD/LittleFS logging
- safer control interface board schematic
- optional MQTT / POWER_CENTER integration
- remote GitHub firmware update workflow
