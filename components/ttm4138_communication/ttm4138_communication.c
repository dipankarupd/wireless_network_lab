// standard C library headers
#include <stdint.h>
#include <string.h>

// ESP headers
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <nvs_flash.h>

// project headers
#include "global_variables.h"
#include "ttm4138_communication.h"

/**
 * @brief Configure Wi-Fi interface for promiscuous packet reception.
 *
 * @param receive_mode  Mode determining which callback handles received frames:
 *                      - REC_TARGET      : Targeted frame listener
 *                      - REC_PROMISCUOUS : General promiscuous listener
 *                      - REC_UNICAST     : Unicast frame listener
 */
void configure_wifi(receive_mode_t receive_mode) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_wifi_set_promiscuous(true); // parse all incomming frames
    switch (receive_mode) {
        case REC_TARGET:
            ESP_LOGI("configure_wifi", "Set Callback to targeted listen");
            esp_wifi_set_promiscuous_rx_cb(fsm_callback_targeted_listen); // function handles incomming frames
            break;
        case REC_PROMISCUOUS:
            ESP_LOGI("configure_wifi", "Set Callback to promiscious");
            esp_wifi_set_promiscuous_rx_cb(fsm_callback_promiscious); // function handles incomming frames
            break;
        case REC_UNICAST:
            ESP_LOGI("configure_wifi", "Set Callback to unicast");
            esp_wifi_set_promiscuous_rx_cb(fsm_callback_unicast); // function handles incomming frames
            break;
    }
}

/**
 * @brief Create an IEEE 802.11 frame with specified destination and payload.
 *
 * @param dest_addr  Destination MAC address (6 bytes).
 * @param data       Pointer to payload data.
 * @param data_size  Size of payload data in bytes.
 *
 * @note Caller is responsible for freeing the memory.
 * 
 * @return Pointer to the allocated and initialized frame structure.
 */
ieee80211_frame_t* create_frame(uint8_t dest_addr[], uint8_t* data, uint16_t data_size) {
    ieee80211_frame_t* frame = malloc(sizeof(ieee80211_frame_t) + data_size); // id 1
    frame->frame_ctrl = 0x2008;
    memcpy(frame->addr1, dest_addr, MAC_ADDR_LEN);
    memcpy(frame->addr2, local_mac_addr, MAC_ADDR_LEN);
    memcpy(frame->addr3, dest_addr, MAC_ADDR_LEN);
    frame->seq_ctrl = 0xF240;
    memcpy(frame->data, data, data_size);
    frame->LLC_DSAP = 0xAA;
    frame->LLC_SSAP = 0xAA;
    frame->LLC_control = 0x03;
    uint8_t SNAP_OUI[] = {0x69, 0x69, 0x69};
    memcpy(frame->SNAP_OUI, SNAP_OUI, sizeof(SNAP_OUI));
    uint8_t SNAP_prot_ID[] = {0x69, 0x69};
    memcpy(frame->SNAP_prot_ID, SNAP_prot_ID, sizeof(SNAP_prot_ID));
    return frame;
}

/**
 * @brief Transmit an IEEE 802.11 frame with a specified destination address.
 *
 * @param dest_addr  Destination MAC address (6 bytes).
 * @param data       Pointer to payload data.
 * @param data_size  Size of payload data in bytes.
 *
 * @return ESP_OK on success, or an ESP-IDF error code on failure.
 */
esp_err_t transmit(uint8_t dest_addr[], uint8_t data[], uint16_t data_size) {
    ieee80211_frame_t* frame = create_frame(dest_addr, data, data_size);
    esp_err_t result = esp_wifi_80211_tx(WIFI_IF_STA, frame, sizeof(ieee80211_frame_t)+data_size, true);
    free(frame); // id 1
    if (result == ESP_OK) {
        ESP_LOGI("transmit", "MAC frame transmitted successfully");
    } else {
        ESP_LOGE("transmit", "Failed to transmit MAC frame");
    }
    return result;
}

/**
 * @brief Promiscuous mode callback for targeted frame reception.
 *
 * Processes all incoming Wi-Fi packets, checks if the destination MAC
 * matches the configured target, and forwards matching frames to the
 * FSM event queue.
 *
 * @param buf   Pointer to the received packet buffer.
 * @param type  Not used.
 *
 * @note Allocates memory for the frame; memory is freed if the frame is
 *       not relevant or if the queue is full. Caller is responsible for
 *       freeing memory.
 */
void fsm_callback_targeted_listen(void* buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint16_t length = pkt->rx_ctrl.sig_len;

    // copy frame
    ieee80211_frame_t* frame = malloc(length); // id 3
    memcpy(frame, pkt->payload, length);

    // determine destination
    if(memcmp(target_mac_addr, frame->addr1, MAC_ADDR_LEN) == 0 || memcmp(target_mac_addr, frame->addr2, MAC_ADDR_LEN) == 0){
        received_frame_info_t received_frame_info = {.contains_frame = 1, .frame = frame, .length = length};
        // xQueueSendFromISR(fsm_event_queue, &received_frame_info, NULL); // TODO should this be used?
        BaseType_t result = xQueueSend(fsm_event_queue, &received_frame_info, 0);
        if (result == errQUEUE_FULL) {
            free(frame);
        }
    } else {
        free(frame); // id 3
    }
    return;
}

/**
 * @brief Promiscuous mode callback for all frame reception.
 *
 * Processes every incoming Wi-Fi packet and forwards it to the FSM event queue.
 *
 * @param buf   Pointer to the received packet buffer.
 * @param type  Not used.
 *
 * @note Allocates memory for the frame; memory is freed if the queue is full.
 *       Caller is responsible for freeing memory.
 */
void fsm_callback_promiscious(void* buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint16_t length = pkt->rx_ctrl.sig_len;

    // copy frame
    ieee80211_frame_t* frame = malloc(length); // id 3
    memcpy(frame, pkt->payload, length);

    // determine destination
    received_frame_info_t received_frame_info = {.contains_frame = 1, .frame = frame, .length = length};
    BaseType_t result = xQueueSend(fsm_event_queue, &received_frame_info, 0);
    if (result == errQUEUE_FULL) {
        free(frame); // id 3
    }
}

/**
 * @brief Promiscuous mode callback for unicast frame reception.
 *
 * Processes incoming Wi-Fi packets, checks if the destination MAC matches
 * the local device's MAC, and forwards matching frames to the FSM event queue.
 *
 * @param buf   Pointer to the received packet buffer.
 * @param type  Not used.
 *
 * @note Allocates memory for the frame; memory is freed if the queue is full.
 *       Caller is responsible for freeing memory.
 */
void fsm_callback_unicast(void* buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint16_t length = pkt->rx_ctrl.sig_len;

    // copy frame
    ieee80211_frame_t* frame = malloc(length); // id 3
    memcpy(frame, pkt->payload, length);

    // determine destination
    if(memcmp(local_mac_addr, frame->addr1, MAC_ADDR_LEN) == 0){
        received_frame_info_t received_frame_info = {.contains_frame = 1, .frame = frame, .length = length};
        // xQueueSendFromISR(fsm_event_queue, &received_frame_info, NULL); // TODO should this be used?
        BaseType_t result = xQueueSend(fsm_event_queue, &received_frame_info, 0);
        if (result == errQUEUE_FULL) {
            free(frame);
        }
    } else {
        free(frame); // id 3
    }
    return;
}

/**
 * @brief Print received Wi-Fi frames in hexadecimal format for sniffing.
 *
 * Continuously reads frames from the FSM event queue, converts the payload
 * to a hex string, and prints it to the console. Used for sending frames 
 * from microcontroller to desktop computer.
 *
 * @param arg Not used.
 *
 * @note Allocates memory for the hex string per frame; memory is freed
 *       after printing. The received frame's memory is also freed after use.
 */
void print_frame(void* arg){
    received_frame_info_t received_frame_info;
    while (1) {
        if (xQueueReceive(fsm_event_queue, &received_frame_info, portMAX_DELAY)) {
            if (received_frame_info.contains_frame == 0) { // timeout
                continue;
            } else {
                uint8_t* bytes = (uint8_t*)received_frame_info.frame;

                // Each byte becomes two hex digits + optional space; we'll omit the space here
                // Total length = 2 * size + 1 for null terminator
                char* hex_str = malloc(2 * received_frame_info.length + 1); // id 4
                if (!hex_str) continue;

                for (size_t i = 0; i < received_frame_info.length; i++) {
                    sprintf(hex_str + 2 * i, "%02X", bytes[i]);
                }
                hex_str[2 * received_frame_info.length] = '\0'; // Null-terminate the string
                printf("Data: %s\n", hex_str);
                free(hex_str); // id 4
            }
            free(received_frame_info.frame); // id 3
        }
        vTaskDelay(1);
    }
}
