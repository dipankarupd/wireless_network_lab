# TTM4138 Wireless Security — ESP32-C6 lab project

In this lab, we will implement and explore different wireless security protocols. Each group will use three ESP32-C6 boards: two to send messages between each other, and one to spy on the communication.

To simplify the implementation, the course staff have created a few library functions that you may use to solve the tasks. The task descriptions are available on Canvas.


---

## Repository layout

```
.
├─ components/                      # Reusable ESP-IDF components (libraries)
│  ├─ aead/                         # ASCON-128 AEAD (encrypt/decrypt)
│  ├─ auth/                         # ASCON-based PRF/Auth
│  ├─ ecc/                          # secp256r1 big-int + EC ops
│  ├─ hash/                         # ASCON HASH256
│  ├─ global_variables/             # Shared queues, timers, MACs, etc.
│  ├─ ttm4138_communication/        # Wi-Fi RX callbacks & raw TX helpers
│  ├─ ttm4138_ecc/                  # Small ECC helper wrappers for labs
│  ├─ ttm4138_setup/                # One-time board/stack setup (queues, timers, Wi-Fi mode)
│  └─ ttm4138_utils/                # LED, button, timeout helpers, misc utils
├─ documentation/                   # Per-component markdown docs
│  ├─ aead.md
│  ├─ auth.md
│  ├─ ecc.md
│  ├─ hash.md
│  ├─ ttm4138_communication.md
│  ├─ ttm4138_setup.md
│  ├─ ttm4138_utils.md
│  └─ ttm4138_ecc.md                # (second/last ECC doc; helpers built on top of ecc.md)
├─ main/                            # Example apps you can build/run, feel free to add more files here
│  └─ sniffer.c
│  └─ boilerplate_peer_to_peer.c    # Boilerplate for a satemachine based protocol
├─ managed_components/              # External IDF components (e.g., led_strip)
├─ pcap/                            # Saved packet captures
├─ RC4/                             # Independent RC4 snippet (not part of ESP-IDF build)
├─ listener.py                      # Host-side helper (e.g., serial/pcap glue)
├─ CMakeLists.txt                   # CMake entry point
├─ sdkconfig                        # Your current build config (generated)
└─ build/                           # Generated build artifacts (ignore/clean)
```

> [!NOTE]
> Everything you typically **edit** is in `main/`. Everything in `components/` is reusable and already wired into the build.

---
## Useful ESP-IDF docs on the web

- ESP-IDF Programming Guide (ESP32-C6):  
  <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/>

- ESP32-C6-DevKitC-1
  <https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c6/esp32-c6-devkitc-1/index.html>

- Wi-Fi driver & examples (ESP-IDF):  
  <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/network/esp_wifi.html>

- FreeRTOS (as used by ESP-IDF):  
  <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/system/freertos.html>

- `led_strip` component (component registry):  
  <https://components.espressif.com/components/espressif/led_strip>

- ESP32-C6 Datasheet / TRM:  
  <https://www.espressif.com/en/support/documents/technical-documents>
---

## Installation

> [!IMPORTANT]
> Do not install either the ESP toolchain or this repository in a path that contains spaces, as this will cause it to break. If you use Windows and have a space in your home folder, try installing the programs in the shared folder instead.

Follow the Installation guide on espressifs website <https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c6/get-started/index.html#>

## Build, flash, monitor (VS Code **&** CLI)

### A) VS Code

Use the **Espressif IDF** extension:

1. Install the “Espressif IDF” extension and run **ESP-IDF: Configure ESP-IDF Extension**.
2. Open this folder in VS Code.
3. In the bottom status bar:
   - Select **Device Target**: ESP32-C6  
   - Select **Serial Port** (e.g., `/dev/ttyUSB0`, `cu.usbmodem101`,`COMx` or another option depending on your device)
4. To change which app is built (e.g., `main.c` vs `sniffer.c`), edit `main/CMakeLists.txt` and set the `SRCS` list accordingly (see below). Save the file—VS Code will reconfigure automatically.
5. Use the extension buttons/commands:
   - **Build**, **Flash**, **Monitor** (or **Build, Flash and Start Monitor**).
   These are located in the bottom status bar in VS Code.

> [!TIP]
> Hover your mouse over the icons to see what they do.

### B) Command line (alternative)

Configure (optional)
```
idf.py menuconfig
```
Build
```
idf.py build
```
Flash + Monitor
```
idf.py -p /dev/ttyUSB0 flash monitor
```

> [!NOTE]
> Exit monitor with: Ctrl+t x

Erase flash if needed:

```
idf.py erase-flash
```

---

## How to compile a different app (change which C file is built)

There are multiple entry files under `main/` (e.g., `main.c`, `sniffer.c`). You control which one is compiled as the `app_main()` translation unit by editing `main/CMakeLists.txt`.

Open `main/CMakeLists.txt` and set `SRCS` to the file(s) you want:

```
idf_component_register(
    SRCS
        "sniffer.c"            # or "main.c" or your own file
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        ttm4138_setup
        ttm4138_utils
        ttm4138_communication
        hash
        auth
        aead
        ecc
        global_variables
)
```

Common choices you can drop into `SRCS` (one at a time unless you know what you’re doing):

- `main.c` (default scaffold)
- `sniffer.c`

Then build/flash via the **Espressif IDF** VS Code buttons or:

```
idf.py build flash monitor
```

---

## Documentation

ESP library documentation is available at <https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32c6/api-reference/index.html>

Course spesific documentation is available at [`documentation/`](documentation/):

- **ASCON AEAD** — [`aead.md`](documentation/aead.md)
- **ASCON Auth/PRF** — [`auth.md`](documentation/auth.md)
- **ASCON Hash** — [`hash.md`](documentation/hash.md)
- **ECC (core big-int & EC ops)** — [`ecc.md`](documentation/ecc.md)
- **ECC (helpers for labs; second/last ECC doc)** — [`ttm4138_ecc.md`](documentation/ttm4138_ecc.md)
- **Communication (Wi-Fi RX/TX helpers)** — [`ttm4138_communication.md`](documentation/ttm4138_communication.md)
- **Setup (one-time init: queues, timers, Wi-Fi)** — [`ttm4138_setup.md`](documentation/ttm4138_setup.md)
- **Utils (LED/button/timeout, helpers)** — [`ttm4138_utils.md`](documentation/ttm4138_utils.md)

---


## Troubleshooting

Some Troubleshooting tips

### Linux errors

- **Path is no readable when flashing**
  You are probably not a member of the group that owns the COM port. Check group ownership with `ls -l /dev/*`, and add yourself to the group with `usermod -a -G GROUP USERNAME`.

### Windows errors

- **Weird error message**
  Have you made sure that both the project repository and the toolchain are installed in locations with no spaces in the path?


### Errors with your code

- **Queue fills / crash**  
  Always free `received_frame_info.frame` in your consumer (see examples in  
  [`ttm4138_communication.md#print_frame`](documentation/ttm4138_communication.md#print_frame) and  
  the “Receiving packets” snippet in [`#receiving-packets`](documentation/ttm4138_communication.md#receiving-packets)).

- **Timeouts don’t occur**  
  The timeout is **not automatic**; call **`start_timeout_timer`** to (re)arm it  
  (see [`ttm4138_utils.md#start_timeout_timer`](documentation/ttm4138_utils.md#start_timeout_timer)).

