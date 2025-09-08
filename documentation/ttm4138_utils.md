# TTM4138 Utils API — Arguments, Return Values, Purpose & Global Side Effects

Each entry provides a brief **purpose**, details of **arguments** and **return values**, and lists any **global state** the function modifies (without implementation details). Cross-references use `#` links for quick navigation.

---

### configure_led

```c
void configure_led(void);
```

**Purpose**  
The function is called by [setup](./ttm4138_setup.md) and does not need to be called again.

Initialize and configure the onboard LED strip driver (RMT-backed), then clear and refresh the LED.

**Parameters**  
*(none)*

**Returns**  
- `void` — no return value.

**Global State Modified**  
- Initializes and assigns the LED driver handle `led_strip`.  
- Updates the physical LED (cleared, then refreshed).

---

### cycle_light

```c
void cycle_light(void);
```

**Purpose**  
Cycle the on-board LED through **red → green → blue** states. Useful for visualizing state changes.

**Parameters**  
*(none)*

**Returns**  
- `void` — no return value.

**Global State Modified**  
- Reads and updates `light_state` (advances to the next color).  
- Uses `led_strip` to set the LED color and calls its refresh routine.

---

### print_bytes

```c
void print_bytes(uint8_t* start, uint16_t length);
```

**Purpose**  
Print a sequence of bytes in hexadecimal format to standard output.

**Parameters**

| Name     | Type        | Description |
|----------|-------------|-------------|
| `start`  | `uint8_t*`  | Pointer to the first byte to print. |
| `length` | `uint16_t`  | Number of bytes to print. |

**Returns**  
- `void` — no return value.

**Global State Modified**  
- None (writes to stdout only).

---

### strmac_to_mac

```c
int strmac_to_mac(uint8_t mac[MAC_ADDR_LEN], char* str_mac);
```

**Purpose**  
Convert a MAC address string like `"aa:bb:cc:dd:ee:ff"` into a 6-byte array.

**Parameters**

| Name      | Type                    | Description |
|-----------|-------------------------|-------------|
| `mac`     | `uint8_t[MAC_ADDR_LEN]` | Output buffer for the 6 parsed bytes. |
| `str_mac` | `char*`                 | Input MAC address string (`"xx:xx:xx:xx:xx:xx"`). |

**Returns**  
- `int` — `1` on success; `0` on parse failure.

**Global State Modified**  
- None.

---

### create_frame

```c
ieee80211_frame_t* create_frame(uint8_t dest_addr[], uint8_t* data, uint16_t data_size);
```

**Purpose**  
Allocate and populate an IEEE 802.11 frame structure for transmission, setting the destination address and attaching the provided payload.

**Parameters**

| Name        | Type        | Description |
|-------------|-------------|-------------|
| `dest_addr` | `uint8_t[]` | Destination MAC address (6 bytes expected). |
| `data`      | `uint8_t*`  | Pointer to payload data to include in the frame. |
| `data_size` | `uint16_t`  | Size of the payload in bytes. |

**Returns**  
- `ieee80211_frame_t*` — Pointer to the created frame on success; may be `NULL` on allocation/validation failure.

**Global State Modified**  
- None (expected).

---

### on_timeout

```c
void on_timeout(void);
```

**Purpose**  
Timeout callback that signals a timeout event to the FSM by sending a no-frame marker into the event queue.

**Parameters**  
*(none)*

**Returns**  
- `void` — no return value.

**Global State Modified**  
- Sends a `received_frame_info_t` with `contains_frame = 0` to the global queue `fsm_event_queue`.

**See also**  
- [start_timeout_timer](#start_timeout_timer) — to (re)start the timer that ultimately triggers this callback.

---

### start_timeout_timer

```c
void start_timeout_timer(void);
```

**Purpose**  
(Re)start the non-repeating timeout timer so that it will expire once and invoke [on_timeout](#on_timeout).

**Parameters**  
*(none)*

**Returns**  
- `void` — no return value.

**Global State Modified**  
- Stops and restarts the global FreeRTOS timer `timeout_timer`.

---

### wait_for_button_press

```c
void wait_for_button_press(void);
```

**Purpose**  
Block until the **BOOT** button (GPIO 9) is pressed (active-low) and then released.

**Parameters**  
*(none)*

**Returns**  
- `void` — no return value.

**Global State Modified**  
- None (polls GPIO and delays the calling task until press-and-release is observed).

