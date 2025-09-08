# TTM4138 Setup API — Arguments, Return Values, Purpose & Global Side Effects

This reference documents the public setup API.  
Each entry provides a brief **purpose**, details of **arguments** and **return values**, and lists any **global state** the function modifies.  

---

## General comments

The following functions are required to use many of the other library functions we have made, but note that only one of these functions should be called in your code, and it should only be used once at the start of the program.  
Also note that the timeout does not happen automatically; it must be triggered explicitly with [start_timeout_timer](./ttm4138_utils.md#start_timeout_timer).

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

### setup_promiscuous

```c
void setup_promiscuous(uint16_t timeout_ms);
```

**Purpose**  
Configure the device for **promiscuous sniffing** (receive all frames matching promiscuous filters, no specific target). Sets the device identity to the local STA MAC and calls [setup](#setup) with `REC_PROMISCUOUS`.

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

### setup_unicast

```c
void setup_unicast(uint16_t timeout_ms, char* receiver);
```

**Purpose**  
Configure the device for **unicast communication** with a peer.

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

### setup_responder

```c
void setup_responder(uint16_t timeout_ms, char* receiver);
```

**Purpose**  
Configure the device as the **unicast responder** to a specified initiator MAC. Records the peer as initiator and local device as responder, then calls [setup](#setup) with `REC_UNICAST`.

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
Configure the device as the **unicast initiator** toward a specified responder MAC. Delegates entirely to [setup_unicast](#setup_unicast).

**Parameters**

| Name         | Type       | Description |
|--------------|------------|-------------|
| `timeout_ms` | `uint16_t` | Timeout period for the one-shot timer, in milliseconds. |
| `receiver`   | `char*`    | NUL-terminated, colon-separated MAC string of the **responder** peer (e.g., `"AA:BB:CC:DD:EE:FF"`). |

**Returns**  
- `void` — no return value.

**Global State Modified**  
- Same as [setup_unicast](#setup_unicast).

