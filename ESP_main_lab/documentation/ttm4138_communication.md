# TTM4138 Communication API Documentation

This reference documents the public communication API.  
Each entry provides a brief **purpose**, details of **arguments** and **return values**, and lists any **global state** the function modifies (without implementation details).  
Cross-references use `#` links for quick navigation.

---

## Receiving packets

After [setup](./ttm4138_setup.md), received packets are added to the queue `fsm_event_queue`. Below is an example of how to receive messages from that queue. You may choose to handle them differently based on your code’s requirements, but remember to free the received frame; otherwise, the code will eventually crash as memory fills up with packets. This example function only accepts messages that match the layout of the specified struct `YOUR_MESSAGE_STRUCT_HERE`. If you need it to handle multiple message types, you will have to modify it accordingly.

```c
int GetMessage(YOUR_MESSAGE_STRUCT_HERE* protocol_message) {
    received_frame_info_t received_frame_info;

    if (xQueueReceive(fsm_event_queue, &received_frame_info, portMAX_DELAY)) {
        if (received_frame_info.contains_frame == 0) {
            // Timeout event — no frame buffer was allocated for this entry.
            return 0;
        }

        // Copy your protocol payload out of the received frame.
        memcpy(protocol_message, received_frame_info.frame->data, sizeof(*protocol_message));

        // Always free the frame buffer after processing to avoid memory leaks.
        free(received_frame_info.frame);
        return 1;
    }

    return -1; // Queue receive failed or was interrupted.
}
```
Also remember to include the following headers:

```c
#include <string.h>              // for memcpy
#include <freertos/FreeRTOS.h>   // core FreeRTOS definitions
#include <freertos/queue.h>      // xQueueReceive, queue types

#include "global_variables.h"    // received_frame_info_t
```

---

## Definitely useful functions

Each of the following functions was used at least once in the staff solution.

---

### transmit

```c
esp_err_t transmit(uint8_t dest_addr[], uint8_t data[], uint16_t data_size);
```

**Purpose**  
Transmit a custom IEEE 802.11 data frame to a specified destination MAC with the provided payload.

**Parameters**

| Name         | Type         | Description |
|--------------|--------------|-------------|
| `dest_addr`  | `uint8_t[]`  | Destination MAC address (6 bytes expected). |
| `data`       | `uint8_t[]`  | Pointer to payload bytes to send. |
| `data_size`  | `uint16_t`   | Payload size in bytes. |

**Returns**  
- `esp_err_t` — `ESP_OK` on success; otherwise an ESP-IDF error code from `esp_wifi_80211_tx`.

**Global State Modified**  
- None (uses the Wi-Fi driver to send a frame).


## Possibly useful functions

Although not used in the staff solution, these functions might still be useful in your solutions.

---

### print_frame

```c
void print_frame(void* arg);
```

**Purpose**  
Continuously consume received frames from the FSM event queue, convert each frame to a hex string, and print it. Intended as a FreeRTOS task entry function for piping received frames to a console/host.

**Parameters**  
(none)

**Returns**  
- `void` — no return value (loops indefinitely).

**Global State Modified**  
- Drains from global `fsm_event_queue`; frees each received frame buffer after printing.

**See also**  
- Produces output for frames queued by: [fsm_callback_targeted_listen](#fsm_callback_targeted_listen), [fsm_callback_promiscious](#fsm_callback_promiscious), and [fsm_callback_unicast](#fsm_callback_unicast).

---

## Probably useless functions

These functions are mostly intended to be used in the background by the library itself, but feel free to use them if you find a use for them.

---

### configure_wifi

```c
void configure_wifi(receive_mode_t receive_mode);
```

**Purpose**  
The function is called by [setup](./ttm4138_setup.md) and does not need to be called again.

Initialize Wi-Fi for packet reception and enable promiscuous parsing. Selects which receive callback to use based on `receive_mode`:
- `REC_TARGET` → [fsm_callback_targeted_listen](#fsm_callback_targeted_listen)  
- `REC_PROMISCUOUS` → [fsm_callback_promiscious](#fsm_callback_promiscious)  
- `REC_UNICAST` → [fsm_callback_unicast](#fsm_callback_unicast)

**Parameters**

| Name           | Type             | Description |
|----------------|------------------|-------------|
| `receive_mode` | `receive_mode_t` | Which receive path to configure (`REC_TARGET`, `REC_PROMISCUOUS`, or `REC_UNICAST`). |

**Returns**  
- `void` — no return value.

**Global State Modified**  
- Initializes NVS, default event loop, and `esp_netif`.  
- Initializes, starts, and configures Wi-Fi in **STA** mode.  
- Enables promiscuous reception globally (`esp_wifi_set_promiscuous(true)`).  
- Sets the global Wi-Fi promiscuous RX callback to one of the `fsm_callback_*` handlers (see above).  

**See also**  
- [fsm_callback_targeted_listen](#fsm_callback_targeted_listen) · [fsm_callback_promiscious](#fsm_callback_promiscious) · [fsm_callback_unicast](#fsm_callback_unicast)

---

### fsm_callback_unicast

```c
void fsm_callback_unicast(void* buf, wifi_promiscuous_pkt_type_t type);
```

**Purpose**  
Receive callback for **unicast** filtering. Copies the incoming packet, checks if it is addressed to the local device (`addr1 == local_mac_addr`), and if so enqueues it to the FSM event queue.

**Parameters**

| Name   | Type                              | Description |
|--------|-----------------------------------|-------------|
| `buf`  | `void*`                            | Pointer to `wifi_promiscuous_pkt_t` provided by the Wi-Fi driver. |
| `type` | `wifi_promiscuous_pkt_type_t`      | Not used by this handler. |

**Returns**  
- `void` — no return value.

**Global State Modified**  
- Allocates a frame buffer per packet; on successful enqueue, ownership of the buffer transfers to the consumer (e.g., [print_frame](#print_frame)); if the queue is full or the packet is not relevant, the buffer is freed immediately.  
- Pushes `received_frame_info_t` into global `fsm_event_queue`.

---

### fsm_callback_promiscious

```c
void fsm_callback_promiscious(void* buf, wifi_promiscuous_pkt_type_t type);
```

> **Note:** Function name retains the original spelling “promiscious”.

**Purpose**  
Receive callback for **promiscuous (all frames)**. Copies every incoming packet and enqueues it to the FSM event queue.

**Parameters**

| Name   | Type                              | Description |
|--------|-----------------------------------|-------------|
| `buf`  | `void*`                            | Pointer to `wifi_promiscuous_pkt_t` provided by the Wi-Fi driver. |
| `type` | `wifi_promiscuous_pkt_type_t`      | Not used by this handler. |

**Returns**  
- `void` — no return value.

**Global State Modified**  
- Allocates a frame buffer per packet; on successful enqueue, ownership of the buffer transfers to the consumer (e.g., [print_frame](#print_frame)); if the queue is full, the buffer is freed.  
- Pushes `received_frame_info_t` into global `fsm_event_queue`.

---

### fsm_callback_targeted_listen

```c
void fsm_callback_targeted_listen(void* buf, wifi_promiscuous_pkt_type_t type);
```

**Purpose**  
Receive callback for **targeted sniffing**. Copies the incoming packet, checks if either `addr1` or `addr2` matches the configured `target_mac_addr`, and enqueues matching frames to the FSM event queue.

**Parameters**

| Name   | Type                              | Description |
|--------|-----------------------------------|-------------|
| `buf`  | `void*`                            | Pointer to `wifi_promiscuous_pkt_t` provided by the Wi-Fi driver. |
| `type` | `wifi_promiscuous_pkt_type_t`      | Not used by this handler. |

**Returns**  
- `void` — no return value.

**Global State Modified**  
- Allocates a frame buffer only for processing; frees it if the frame is not relevant or the queue is full.  
- Pushes `received_frame_info_t` into global `fsm_event_queue` for relevant frames.

