/*
 * Session 9 — MP-SAE (Protocol B) on the ESP32-C6.
 *
 * This file is the ESP32 "platform layer", exactly mirroring Session 6's
 * p2p_protocol.c split: it wires the lab library's radio events into the
 * platform-independent protocol core (sae_mp.c) via the plat_* functions,
 * and does nothing protocol-specific itself.
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

#include "global_variables.h"
#include "ttm4138_communication.h"
#include "ttm4138_setup.h"
#include "ttm4138_utils.h"
#include "sae_mp.h"

#define FCS_LEN 4   /* sig_len from the radio includes the 4-byte FCS, as in Session 6 */

static sae_party_t party;

/* ================================================================== */
/* Platform layer used by sae_mp.c                                     */
/* ================================================================== */

void plat_send(sae_party_t* p, const uint8_t* data, uint16_t len) {
    transmit(remote_mac_addr, (uint8_t*)data, len);
}
void plat_timer_start(sae_party_t* p) { start_timeout_timer(); }
void plat_timer_stop(sae_party_t* p)  { xTimerStop(timeout_timer, 0); }
void plat_indicate(sae_party_t* p)    { cycle_light(); }

uint64_t plat_random_u64(void) {
    uint64_t v;
    esp_fill_random(&v, sizeof(v));   /* hardware TRNG, same as Session 4 / Session 6 */
    return v;
}

void plat_log(const sae_party_t* p, char level, const char* fmt, ...) {
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
/* 802.11 frame checks, dispatcher, entry point (same shape as Session 6) */
/* ================================================================== */

static const uint8_t* sae_payload(const event_t* ev, uint16_t* len) {
    static const uint8_t snap_oui[3] = { 0x69, 0x69, 0x69 };
    const ieee80211_frame_t* f = ev->frame;

    if (ev->length < sizeof(ieee80211_frame_t) + FCS_LEN) return NULL;
    if ((f->frame_ctrl & 0x0C) != 0x08) return NULL;   /* not a data frame (e.g. an ACK) */
    if (memcmp(f->SNAP_OUI, snap_oui, sizeof(snap_oui)) != 0) return NULL;

    *len = ev->length - sizeof(ieee80211_frame_t) - FCS_LEN;
    return f->data;
}

static void fsm_event_dispatcher(void* arg) {
    event_t ev;
    while (1) {
        if (!xQueueReceive(fsm_event_queue, &ev, portMAX_DELAY)) continue;
        if (ev.event == EV_FRAME) {
            uint16_t len;
            const uint8_t* payload = sae_payload(&ev, &len);
            if (payload) {
                sae_handle_event(&party, EV_FRAME, ev.frame->addr2, payload, len);
            } else {
                ESP_LOGD(party.tag, "ignored non-SAE frame");
            }
            free(ev.frame);
        } else {
            sae_handle_event(&party, ev.event, NULL, NULL, 0);
        }
    }
}

void app_main(void) {
    strcpy(initiator_mac_addr_string, SAE_A_MAC);
    strcpy(responder_mac_addr_string, SAE_B_MAC);
    setup(SAE_TIMEOUT_MS, REC_UNICAST);
    esp_log_level_set("transmit", ESP_LOG_WARN);

    ESP_LOGI("SAE", "This board's MAC is %02x:%02x:%02x:%02x:%02x:%02x",
             local_mac_addr[0], local_mac_addr[1], local_mac_addr[2],
             local_mac_addr[3], local_mac_addr[4], local_mac_addr[5]);
    ESP_LOGI("SAE", "p=%llu (62-bit safe prime) timeout=%dms retries=%d",
             (unsigned long long)SAE_PRIME_P, SAE_TIMEOUT_MS, SAE_MAX_RETRIES);

    switch (device_communication_role) {
    case ROLE_INITIATOR:   /* plays role A: sends c1, c3 */
        ESP_LOGI("SAE", "Role: A (initiator)");
        sae_party_init(&party, SAE_ROLE_A, "SAE-A", local_mac_addr, remote_mac_addr);
        xTaskCreate(fsm_event_dispatcher, "sae_fsm", 6144, NULL, 10, NULL);
        {
            event_t start = { .event = EV_START };
            xQueueSend(fsm_event_queue, &start, 0);
        }
        break;
    case ROLE_RESPONDER:   /* plays role B: sends c2, c4 */
        ESP_LOGI("SAE", "Role: B (responder)");
        sae_party_init(&party, SAE_ROLE_B, "SAE-B", local_mac_addr, remote_mac_addr);
        xTaskCreate(fsm_event_dispatcher, "sae_fsm", 6144, NULL, 10, NULL);
        break;
    default:
        ESP_LOGE("SAE", "MAC not listed in sae_config.h - copy the MAC above into it");
        break;
    }
}
