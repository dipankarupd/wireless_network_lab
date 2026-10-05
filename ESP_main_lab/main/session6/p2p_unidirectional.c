/*
 * Session 6, Task 1 — unidirectional 802.11 MAC-layer communication.
 *
 * Initiator: transmits a small frame struct every 2 seconds.
 * Responder: changes the LED colour every time such a frame arrives.
 *
 * Same binary on both boards; the role is picked from the MAC addresses in
 * p2p_config.h.
 */
#include <stdint.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "global_variables.h"
#include "ttm4138_communication.h"
#include "ttm4138_setup.h"
#include "ttm4138_utils.h"
#include "p2p_config.h"

#define TAG          "P2P-UNI"
#define SEND_PERIOD_MS 2000
#define UNI_MAGIC    0x5A
#define FCS_LEN      4     /* sig_len from the radio includes the 4-byte FCS */

typedef struct __attribute__((packed)) {
    uint8_t  magic;
    uint8_t  sender[MAC_ADDR_LEN];
    uint32_t seq;
    char     text[16];
} uni_msg_t;

static void initiator_task(void* arg) {
    uni_msg_t msg = { .magic = UNI_MAGIC, .seq = 0 };
    memcpy(msg.sender, local_mac_addr, MAC_ADDR_LEN);
    strcpy(msg.text, "hello from I");

    while (1) {
        transmit(remote_mac_addr, (uint8_t*)&msg, sizeof(msg));
        ESP_LOGI(TAG, "sent seq=%lu", (unsigned long)msg.seq);
        msg.seq++;
        vTaskDelay(pdMS_TO_TICKS(SEND_PERIOD_MS));
    }
}

static void responder_task(void* arg) {
    event_t ev;
    while (1) {
        if (!xQueueReceive(fsm_event_queue, &ev, portMAX_DELAY)) {
            continue;
        }
        if (ev.event != 1) {          /* 0 = timer event, not used here */
            continue;
        }

        ieee80211_frame_t* f = ev.frame;
        int is_ours = ev.length >= sizeof(ieee80211_frame_t) + sizeof(uni_msg_t) + FCS_LEN
                   && (f->frame_ctrl & 0x0C) == 0x08                    /* data frame */
                   && memcmp(f->addr2, remote_mac_addr, MAC_ADDR_LEN) == 0
                   && f->data[0] == UNI_MAGIC;
        if (is_ours) {
            uni_msg_t msg;
            memcpy(&msg, f->data, sizeof(msg));
            msg.text[sizeof(msg.text) - 1] = '\0';
            ESP_LOGI(TAG, "received seq=%lu \"%s\"", (unsigned long)msg.seq, msg.text);
            cycle_light();
        }
        free(ev.frame);
    }
}

void app_main(void) {
    strcpy(initiator_mac_addr_string, P2P_INITIATOR_MAC);
    strcpy(responder_mac_addr_string, P2P_RESPONDER_MAC);
    setup(SEND_PERIOD_MS, REC_UNICAST);

    ESP_LOGI(TAG, "This board's MAC is %02x:%02x:%02x:%02x:%02x:%02x",
             local_mac_addr[0], local_mac_addr[1], local_mac_addr[2],
             local_mac_addr[3], local_mac_addr[4], local_mac_addr[5]);

    switch (device_communication_role) {
    case ROLE_INITIATOR:
        ESP_LOGI(TAG, "Role: INITIATOR (sender)");
        xTaskCreate(initiator_task, "uni_tx", 4096, NULL, 5, NULL);
        break;
    case ROLE_RESPONDER:
        ESP_LOGI(TAG, "Role: RESPONDER (LED on receive)");
        xTaskCreate(responder_task, "uni_rx", 4096, NULL, 5, NULL);
        break;
    default:
        ESP_LOGE(TAG, "MAC not listed in p2p_config.h - copy the MAC above into it");
        break;
    }
}
