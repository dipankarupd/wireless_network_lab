# TTM4138 Setup API Documentation

> [!info]
> The following functions are required in order to use many of the other ttm4138 library functions. However, note that you may only use one of the setup functions, and that it should only be used once at the start of the program.

> [!info]
> Also note that the timeout does not happen automatically; it must be triggered explicitly with start_timeout_timer.

---

## Definitely useful functions

Each of the following functions was used at least once in the staff solution.

---

### setup_promiscuous

```c
void setup_promiscuous(uint16_t timeout_ms);
```

**Purpose**  
Configure the device for **promiscuous sniffing** (receive all frames matching promiscuous filters, no specific target). Sets the device identity to the local STA MAC and calls [setup](#setup) with `REC_PROMISCUOUS`.

> [!IMPORTANT]
> In most environments, this mode will pick up too many packets to handle, and it will therefore drop a lot of packets. If all packets in a handshake are needed, use `setup_target` instead.

**Parameters**

| Name         | Type       | Description |
|--------------|------------|-------------|
| `timeout_ms` | `uint16_t` | Timeout period for the one-shot timer, in milliseconds. |

**Returns**  
- `void` — no return value.

**Global State Modified**  
- `sniffer_mac_addr_string` set to the local STA MAC string.  
- Common setup side effects via [setup](#setup).

---

### setup_target

```c
void setup_target(uint16_t timeout_ms, char* target);
```

**Purpose**  
Configure the device for **targeted sniffing** of a specific MAC address. Sets the sniffer identity to the local STA MAC and tracks the provided target. Internally calls [setup](#setup) with `REC_TARGET`.

**Parameters**

| Name         | Type     | Description |
|--------------|----------|-------------|
| `timeout_ms` | `uint16_t` | Timeout period for the one-shot timer, in milliseconds. |
| `target`     | `char*`    | NUL-terminated, colon-separated MAC string of the sniff target (e.g., `"AA:BB:CC:DD:EE:FF"`). |

**Returns**  
- `void` — no return value.

**Global State Modified**  
- `sniffer_mac_addr_string` set to local STA MAC string.  
- `target_mac_addr_string` set to `target`.  
- Common setup side effects via [setup](#setup).

---

### setup_responder

```c
void setup_responder(uint16_t timeout_ms, char* receiver);
```

**Purpose**  
Configure the device as the **unicast responder** to a specified initiator MAC. Records the peer as initiator and local device as responder, then calls [setup](#setup) with `REC_UNICAST`. Is made to be used together with another device running `setup_initiator`

**Parameters**

| Name         | Type       | Description |
|--------------|------------|-------------|
| `timeout_ms` | `uint16_t` | Timeout period for the one-shot timer, in milliseconds. |
| `receiver`   | `char*`    | NUL-terminated, colon-separated MAC string of the **initiator** peer (e.g., `"AA:BB:CC:DD:EE:FF"`). |

**Returns**  
- `void` — no return value.

**Global State Modified**  
- `initiator_mac_addr_string` set to `receiver`.  
- `responder_mac_addr_string` set to local STA MAC string.  
- Common setup side effects via [setup](#setup).

---

### setup_initiator

```c
void setup_initiator(uint16_t timeout_ms, char* receiver);
```

**Purpose**  
Configure the device as the **unicast initiator** toward a specified responder MAC. Delegates entirely to [setup_unicast](#setup_unicast). Is made to be used together with another device running `setup_responder`

**Parameters**

| Name         | Type       | Description |
|--------------|------------|-------------|
| `timeout_ms` | `uint16_t` | Timeout period for the one-shot timer, in milliseconds. |
| `receiver`   | `char*`    | NUL-terminated, colon-separated MAC string of the **responder** peer (e.g., `"AA:BB:CC:DD:EE:FF"`). |

**Returns**  
- `void` — no return value.

**Global State Modified**  
- Same as [setup_unicast](#setup_unicast).

---

## Possibly useful functions

Although not used in the staff solution, these functions might still be useful in your solutions.

---

### setup_unicast

```c
void setup_unicast(uint16_t timeout_ms, char* peer);
```

**Purpose**  
Configure the device for **unicast communication** with a peer.

> [!WARNING]  
> The global variables `initiator_mac_addr_string` and `responder_mac_addr_string`, is nonsensical and should not be used together with setup_unicast

**Parameters**

| Name         | Type       | Description |
|--------------|------------|-------------|
| `timeout_ms` | `uint16_t` | Timeout period for the one-shot timer, in milliseconds. |
| `receiver`   | `char*`    | NUL-terminated, colon-separated MAC string of the peer (e.g., `"AA:BB:CC:DD:EE:FF"`). |

**Returns**  
- `void` — no return value.

**Global State Modified**  
- `initiator_mac_addr_string` set to local STA MAC string.  
- `responder_mac_addr_string` set to `receiver`.  
- Common setup side effects via [setup](#setup).  

**Notes**  
- `device_communication_role` may not represent a meaningful role when this helper is used; avoid relying on it elsewhere.

---

## Probably useless functions

These functions are mostly intended to be used in the background by the library itself, but feel free to use them if you find a use for them.

---

### setup

```c
void setup(uint16_t timeout_ms, receive_mode_t receive_mode);
```

**Purpose**  
Initialize system subsystems and establish the device’s communication role.  
Configures logging, LED, Wi-Fi (in the specified receive mode), creates the FSM event queue and a one-shot timeout timer, reads the local MAC address, and derives the device role (initiator/responder/sniffer/undefined) by comparing known MAC addresses. 

**Parameters**

| Name           | Type              | Description |
|----------------|-------------------|-------------|
| `timeout_ms`   | `uint16_t`        | Timeout period for the one-shot timer, in milliseconds. |
| `receive_mode` | `receive_mode_t`  | Wi-Fi receive configuration (e.g., `REC_TARGET`, `REC_PROMISCUOUS`, `REC_UNICAST`). |

**Returns**  
- `void` — no return value.

**Global State Modified**  
- Logging level (system-wide).  
- LED and Wi-Fi subsystems configured.  
- `fsm_event_queue` created/assigned.  
- `timeout_timer` created/assigned.  
- `local_mac_addr` populated.  
- Potentially `remote_mac_addr` and/or `target_mac_addr` populated depending on detected role.  
- `device_communication_role` set based on MAC comparison.

---
