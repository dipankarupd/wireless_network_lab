/*
 * Session 6, Tasks 2-9 — P2P ping-pong protocol on the ESP32-C6.
 *
 * This file is the ESP32 "platform layer": it receives frames and timer events
 * from the lab library's fsm_event_queue, does the 802.11-level checks, and
 * feeds the events into the platform-independent state machines in p2p_fsm.c.
 * It also implements the plat_* functions those state machines call
 * (send, timer, LED, random, clock, log).
 *
 * The same p2p_fsm.c also runs in the laptop simulator (Prosim/p2p).
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>

#include "global_variables.h"
#include "ttm4138_communication.h"
#include "ttm4138_setup.h"
#include "ttm4138_utils.h"
#include "p2p_fsm.h"

#define FCS_LEN 4   /* sig_len from the radio includes the 4-byte FCS */

static p2p_party_t party;   /* this board's protocol state */

/* ================================================================== */
/* Platform layer used by p2p_fsm.c                                    */
/* ================================================================== */

void plat_send(p2p_party_t* p, const uint8_t* data, uint16_t len) {
    transmit(remote_mac_addr, (uint8_t*)data, len);
}

void plat_timer_start(p2p_party_t* p) {
    start_timeout_timer();
}

void plat_timer_stop(p2p_party_t* p) {
    xTimerStop(timeout_timer, 0);
}

void plat_indicate(p2p_party_t* p) {
    cycle_light();
}

void plat_random(uint8_t* buf, uint16_t len) {
    esp_fill_random(buf, len);   /* hardware TRNG (Session 4) */
}

uint32_t plat_now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void plat_delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void plat_log(const p2p_party_t* p, char level, const char* fmt, ...) {
    char line[160];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    switch (level) {
    case 'E': ESP_LOGE(p->tag, "%s", line); break;
    case 'W': ESP_LOGW(p->tag, "%s", line); break;
    case 'D': ESP_LOGD(p->tag, "%s", line); break;
    default:  ESP_LOGI(p->tag, "%s", line); break;
    }
}

/* ================================================================== */
/* 802.11 frame checks, dispatcher, entry point                        */
/* ================================================================== */

/* Returns the P2P payload of a received frame, or NULL for frames that are
 * not ours (e.g. the ACKs the radio also delivers, which are only 14 bytes). */
static const uint8_t* p2p_payload(const event_t* ev, uint16_t* len) {
    static const uint8_t snap_oui[3] = { 0x69, 0x69, 0x69 };
    const ieee80211_frame_t* f = ev->frame;

    if (ev->length < sizeof(ieee80211_frame_t) + FCS_LEN) return NULL;
    if ((f->frame_ctrl & 0x0C) != 0x08) return NULL;   /* not a data frame */
    if (memcmp(f->SNAP_OUI, snap_oui, sizeof(snap_oui)) != 0) return NULL;

    *len = ev->length - sizeof(ieee80211_frame_t) - FCS_LEN;
    return f->data;
}

static void fsm_event_dispatcher(void* arg) {
    event_t ev;
    while (1) {
        if (!xQueueReceive(fsm_event_queue, &ev, portMAX_DELAY)) {
            continue;
        }
        if (ev.event == EV_FRAME) {
            uint16_t len;
            const uint8_t* payload = p2p_payload(&ev, &len);
            if (payload) {
                p2p_handle_event(&party, EV_FRAME, ev.frame->addr2, payload, len);
            } else {
                ESP_LOGD(party.tag, "ignored non-P2P frame");
            }
            free(ev.frame);
        } else {
            p2p_handle_event(&party, ev.event, NULL, NULL, 0);
        }
    }
}

void app_main(void) {
    strcpy(initiator_mac_addr_string, P2P_INITIATOR_MAC);
    strcpy(responder_mac_addr_string, P2P_RESPONDER_MAC);
    setup(P2P_TIMEOUT_MS, REC_UNICAST);
    esp_log_level_set("transmit", ESP_LOG_WARN);  /* silence per-frame "transmitted" lines */

    ESP_LOGI("P2P", "This board's MAC is %02x:%02x:%02x:%02x:%02x:%02x",
             local_mac_addr[0], local_mac_addr[1], local_mac_addr[2],
             local_mac_addr[3], local_mac_addr[4], local_mac_addr[5]);
    ESP_LOGI("P2P", "n0=%lu T=%d AEAD=%s timeout=%dms retries=%d",
             (unsigned long)P2P_N0, P2P_T, P2P_USE_AEAD ? "on" : "OFF (plaintext)",
             P2P_TIMEOUT_MS, P2P_MAX_RETRIES);

    switch (device_communication_role) {
    case ROLE_INITIATOR: {
        ESP_LOGI("P2P", "Role: INITIATOR");
        p2p_party_init(&party, P2P_INITIATOR, "P2P-I", local_mac_addr, remote_mac_addr);
        xTaskCreate(fsm_event_dispatcher, "p2p_fsm", 6144, NULL, 10, NULL);
        event_t start = { .event = EV_START };
        xQueueSend(fsm_event_queue, &start, 0);
        break;
    }
    case ROLE_RESPONDER:
        ESP_LOGI("P2P", "Role: RESPONDER");
        p2p_party_init(&party, P2P_RESPONDER, "P2P-R", local_mac_addr, remote_mac_addr);
        xTaskCreate(fsm_event_dispatcher, "p2p_fsm", 6144, NULL, 10, NULL);
        break;
    default:
        ESP_LOGE("P2P", "MAC not listed in p2p_config.h - copy the MAC above into it");
        break;
    }
}
