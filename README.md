# Mesh ESP32S3 Compact Component

This is a **proof-of-concept (POC)** compact Meshtastic + MeshCore component for the ESP32S3.

## Overview

- Minimal implementation for ESP32S3 hardware.
- Intended for experimentation and development.
- Not production ready.

## Features

- Basic Meshtastic protocol support.
- Basic MeshCore protocol support: signed adverts, group text, direct messages
  with ECDH, delivery ACKs, and an optional (off by default) repeater mode.
  Path learning, TRACE, GRP_DATA and ANON_REQ are not implemented yet.

## Requirements

- ESP32S3 development board
- ESP-IDF >5
- RadioLib

## Disclaimer

This project is for **POC and educational purposes only**. Use at your own risk.

## Using this component

Add it to your project's `main/idf_component.yml`:

```yaml
dependencies:
  htotoo/espmeshcompact:
    version: "*"
```

## Repository layout

```
CMakeLists.txt          component registration
idf_component.yml       component manifest
include/                public headers
src/                    implementation
meshtastic/             generated nanopb protobuf definitions
lib/                    vendored crypto (rweather Crypto, orlp ed25519)
examples/               buildable IDF projects
```

## Examples

Two examples live in this repository and build against these sources directly
through `override_path`, so they always track the working tree:

- `examples/mc_receiver` - a MeshCore chat node: advertises itself, sends and
  receives group messages, and echoes direct messages back to the sender.
- `examples/mt_listener` - listens for Meshtastic text, node info and positions.

```sh
cd examples/mt_listener
idf.py set-target esp32s3
idf.py build flash monitor
```

This lib is used in some projects, so those are for usage examples.
- https://github.com/htotoo/meshbaba - a simple Mesh notificator, and Ping responder.
- https://github.com/htotoo/DarkMesh - a complex app that can abuse the MT's weak points for demonstration.
- https://github.com/htotoo/TPagerMesh - a work in progress / abandoned T-Pager FW using this lib for communication.


## Using this project needs these cmake options:
```
add_compile_definitions(RADIOLIB_EXCLUDE_RF69)
add_compile_definitions(RADIOLIB_EXCLUDE_SX1231)
add_compile_definitions(RADIOLIB_EXCLUDE_AFSK)
add_compile_definitions(RADIOLIB_EXCLUDE_APRS)
add_compile_definitions(RADIOLIB_EXCLUDE_AX25)
add_compile_definitions(RADIOLIB_EXCLUDE_BELL)
add_compile_definitions(RADIOLIB_EXCLUDE_FSK4)    
add_compile_definitions(RADIOLIB_EXCLUDE_HELLSCHREIBER)
add_compile_definitions(RADIOLIB_EXCLUDE_MORSE)
add_compile_definitions(RADIOLIB_EXCLUDE_PAGER)
add_compile_definitions(RADIOLIB_EXCLUDE_RTTY)
add_compile_definitions(RADIOLIB_EXCLUDE_SSTV)
```

They belong in your **project** `CMakeLists.txt`, before the `project()` call.
See `examples/mc_receiver/CMakeLists.txt` for a working project file.

## WIP
This project is under active development and subject to change. Many features are incomplete or not yet implemented. Contributions and pull requests are welcome!
