# Diesel_Heater_Turbo_ESP32

ESP32 diagnostics project for common 3-wire Chinese diesel-heater controller buses.

## Current GitHub build

The repository now contains a **passive RX-only monitor**. It does not transmit commands to the heater bus.

Implemented:

- ESP32 / PlatformIO project
- GPIO16 RX at 25,000 baud
- common 48-byte frame decoder
- temperature, voltage, fan RPM, exchanger temperature, glow telemetry, pump frequency, state and error-code fields
- raw frame capture and valid/rejected frame counters
- 30-minute history buffer source
- compact browser graph-page source
- GitHub Actions PlatformIO build

The current `src/main.cpp` is intentionally a simple Serial monitor for the first hardware test. The browser page is present in `src/web_page.h`; wiring it into the running firmware is the next pass after confirming real frames from this heater.

## First test wiring

Keep the factory LCD connected.

```text
BLACK / GND  ---------------- ESP32 GND
BLUE / DATA  ---- level interface ---- ESP32 GPIO16 RX
RED / POWER  ---------------- not connected for the first test
```

Power the ESP32 from USB. Measure the data-line voltage before connecting it to a GPIO.

## Safety architecture

The diagnostic build is RX-only and leaves the factory heater ECU responsible for its normal protection and shutdown behaviour. The first development goal is to identify the exact protocol variant and diagnose faults before adding any command/control layer.

## Build

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

## Next pass

1. confirm frames from the actual heater
2. validate field offsets against real readings
3. activate the browser dashboard + rolling graphs
4. add CSV/event logging
5. only then evaluate optional factory-controller button emulation
