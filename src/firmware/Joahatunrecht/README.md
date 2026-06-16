# Joahatunrecht (AALeC Firmware)

PlatformIO project for the AALeC V3 (ESP8266 / esp12e) board, built with the
Arduino framework.

## Setup

```bash
cp src/config.h.example src/config.h
# fill in WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER, MQTT_PORT
```

`src/config.h` is gitignored — never commit it.

## Building

Normally just use the PlatformIO IDE/CLI (`pio run`).

### Apple Silicon Macs (no Rosetta 2)

PlatformIO's `toolchain-xtensa` package (the `xtensa-lx106-elf` compiler for
ESP8266) is only published as an `x86_64` macOS binary — there is no native
`arm64` macOS build, including in the latest upstream
[esp-quick-toolchain](https://github.com/earlephilhower/esp-quick-toolchain)
releases. Running it requires Rosetta 2, which newer macOS versions no longer
provide, so a plain `pio run` fails with:

```
sh: .../xtensa-lx106-elf-g++: Bad CPU type in executable
```

Workaround: build inside a native arm64 Linux container (via `podman`), where
PlatformIO downloads the native `linux_aarch64` toolchain instead — no
emulation needed.

```bash
./build.sh              # compile -> .pio/build/esp12e/firmware.bin
./build.sh -t clean      # pio run -t clean
./upload.sh              # flash firmware.bin to /dev/cu.usbserial-110
./upload.sh /dev/cu.usbmodem1101   # ...or a different port
```

`build.sh` builds a small `python:3.11-slim`-based image (see
`Containerfile`) on first use, and caches PlatformIO packages in a podman
volume (`joahatunrecht-pio-cache`) so the toolchain/libs aren't re-downloaded
on every build.

**Flashing doesn't happen through `build.sh`** — podman on macOS can't pass
the USB serial device through to the container, and `pio run -t upload`
would re-trigger a host-side compile anyway (the SCons build signature
embeds the compiler's absolute path, which differs between the container's
Linux toolchain and the host's broken macOS one, so PlatformIO always
considers the build stale on the host). `upload.sh` instead calls
`tool-esptoolpy`'s `esptool.py` directly with the prebuilt `firmware.bin` —
pure Python, runs natively on arm64, no compiler involved. Re-run
`./build.sh` after any source change, then `./upload.sh`.
