# TPMS Reader for Flipper Zero

A focused TPMS (Tire Pressure Monitoring System) signal reader for the
[Flipper Zero](https://flipperzero.one/), built on top of
[ProtoView](https://github.com/antirez/protoview) by Salvatore Sanfilippo.

Monitors **315 MHz** for US-market tire pressure sensors. Automatically
cycles through OOK and FSK modulations to detect signals from a wide
range of vehicle makes.

![ProtoView screenshot raw signal](/images/protoview_1.jpg)

## Supported Vehicles

| Decoder | Vehicles | Modulation | Encoding |
|---------|----------|------------|----------|
| **Schrader GEN1** | Chrysler, Dodge (Durango, etc.), Jeep, Ram, Mercedes | OOK | Manchester |
| **Schrader EG53MA4** | Chevrolet (Tahoe, Silverado), GMC (Sierra, Yukon), Saab, Opel | OOK | Manchester |
| **Schrader SMD3MA4** | Subaru, Nissan, Infiniti | OOK | Manchester |
| **GM Aftermarket** | GM vehicles with aftermarket sensors (EL-50448 compatible) | OOK | Manchester |
| **Ford** | Ford (Transit, F-150, Escape, Focus, Fiesta, Kuga) | FSK | Manchester |
| **Toyota PMV-107J** | Toyota Highlander, Camry, Corolla (US); Lexus | FSK | Diff. Manchester |
| **Toyota (EU)** | Toyota Auris/Corolla (EU 433 MHz) | FSK | Diff. Manchester |
| **Elantra 2012** | Hyundai Elantra 2012, Honda Civic, Genesis | FSK | Manchester |
| **BMW Gen4/5** | BMW (Gen4/Gen5), Audi | FSK | Manchester |
| **BMW Gen2/3** | BMW (Gen2/Gen3) | FSK | Diff. Manchester |
| **Porsche** | Porsche Boxster/Cayman (Typ 987) | FSK | Diff. Manchester |
| **Renault** | Renault Clio, Captur, Zoe, Dacia Sandero | FSK | Manchester |
| **Citroen** | Citroen, Peugeot, Fiat, Mitsubishi (VDO) | FSK | Manchester |
| **Hyundai/Kia** | Hyundai i30, Kia (VDO/Continental sensors) | FSK | Manchester |

## Features

- **Auto-cycling modulations**: Rotates through OOK and FSK presets every
  ~3 seconds to catch different sensor types without manual switching.
- **Sensor tracking**: Detected sensors are listed with tire ID, pressure
  (PSI), temperature (F), and receive count.
- **Crash resilience**: Every detection is appended to
  `/ext/apps_data/tpms_reader/tpms_log.csv` on the SD card, so data
  survives app crashes or restarts.
- **14 protocol decoders** covering most US-market vehicles at 315 MHz,
  plus several EU 433 MHz protocols.

## Decoded Fields

Each detected TPMS signal shows:

- **Tire ID** — unique sensor identifier (hex)
- **Pressure** — in PSI (converted from kPa where needed)
- **Temperature** — in Fahrenheit (converted from Celsius where needed)
- **Protocol name** — which decoder matched the signal

## Installing

### Pre-built binary

Drop `dist/tpms_reader.fap` into your Flipper Zero at:

    /ext/apps/Tools/tpms_reader.fap

### Building from source

Requires [ufbt](https://github.com/flipperdevices/flipperzero-ufbt)
targeting firmware **v1.4.3** or compatible:

```bash
pip install ufbt
ufbt update --hw-target f7 --channel release
ufbt build
```

Deploy directly to a connected Flipper:

```bash
ufbt launch
```

## How It Works

The app uses the Flipper Zero's CC1101 radio to capture raw RF pulses at
315 MHz. A signal detection algorithm classifies pulse durations into
timing classes to identify coherent transmissions. Each detected signal
is then passed through all registered protocol decoders.

TPMS sensors transmit periodically (typically every 30-60 seconds while
driving, less frequently when stationary). The app cycles through
modulation presets (OOK, FSK, Toyota-optimized FSK) so it can receive
signals from sensors using different modulation schemes. Because cycling
means you're only listening on one modulation at a time, a given
transmission may be missed — but sensors repeat frequently enough that
detections accumulate over a few minutes of driving.

## CSV Log Format

Detections are logged to `/ext/apps_data/tpms_reader/tpms_log.csv`:

```
id,protocol,pressure_psi,temperature_f,rx_count
A1B2C3D4,Toyota PMV-107J,32.5,72.0,3
```

## License

The base ProtoView application is released under the **BSD-2-Clause**
license by Salvatore Sanfilippo. See [LICENSE](LICENSE).

Several TPMS protocol decoders were written as independent
implementations using protocol specifications derived from the
[rtl_433](https://github.com/merbanan/rtl_433) project (**GPL-2.0**
by Benjamin Larsson and contributors). No rtl_433 source code was
copied. See [CREDITS](CREDITS) for full attribution.

## Credits

- **[ProtoView](https://github.com/antirez/protoview)** by Salvatore
  Sanfilippo — the base application and signal detection framework.
- **[rtl_433](https://github.com/merbanan/rtl_433)** by Benjamin Larsson
  and contributors — protocol documentation and specifications used as
  reference for writing TPMS decoders.
- Application icon designed by Stefano Liuzzo.
