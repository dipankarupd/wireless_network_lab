// standard C library headers
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ESP headers
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_mac.h>
#include <esp_log.h>

// project headers
#include "global_variables.h"
#include "ttm4138_setup.h"
#include "ttm4138_utils.h"
#include "ttm4138_communication.h"

/**
 * @brief System setup function initializing peripherals and device role.
 *
 * Configures logging, LED, Wi-Fi (with specified receive mode), event queue,
 * and timeout timer. Determines the device's communication role based on
 * its MAC address compared to predefined addresses.
 *
 * @param timeout_ms    Timeout duration in milliseconds for the timer.
 * @param receive_mode  Wi-Fi receive mode to configure.
 *                      - REC_TARGET      : Targeted frame listener
 *                      - REC_PROMISCUOUS : General promiscuous listener
 *                      - REC_UNICAST     : Unicast frame listener
 */
void setup(uint16_t timeout_ms, receive_mode_t receive_mode) {
    // general setup
    esp_log_level_set("*", ESP_LOG_INFO);  // Set log level for better debugging
    configure_led();
    configure_wifi(receive_mode);

    // fsm setup
    fsm_event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(received_frame_info_t));

    // timer setup
    timeout_timer = xTimerCreate("tmo", pdMS_TO_TICKS(timeout_ms), pdFALSE, NULL, on_timeout);

    // determine device role
    uint8_t initiator_mac_addr_buffer[MAC_ADDR_LEN];  // buffer used for the next memcmp
    uint8_t responder_mac_addr_buffer[MAC_ADDR_LEN];  // buffer used for the next memcmp
    uint8_t sniffer_mac_addr_buffer[MAC_ADDR_LEN];  // buffer used for the next memcmp
    strmac_to_mac(initiator_mac_addr_buffer, initiator_mac_addr_string);
    strmac_to_mac(responder_mac_addr_buffer, responder_mac_addr_string);
    strmac_to_mac(sniffer_mac_addr_buffer, sniffer_mac_addr_string);
    esp_read_mac(local_mac_addr, ESP_MAC_WIFI_STA); // set local mac address

    if (!memcmp(local_mac_addr, initiator_mac_addr_buffer, MAC_ADDR_LEN)) { // set remote mac address
        strmac_to_mac(remote_mac_addr, responder_mac_addr_string);
        device_communication_role = ROLE_INITIATOR;

    } else if(!memcmp(local_mac_addr, responder_mac_addr_buffer, MAC_ADDR_LEN)){
        strmac_to_mac(remote_mac_addr, initiator_mac_addr_string);
        device_communication_role = ROLE_RESPONDER;

    } else if(!memcmp(local_mac_addr, sniffer_mac_addr_buffer, MAC_ADDR_LEN)){
        ESP_LOGI("Setup", "Setting %s as target", target_mac_addr_string);
        strmac_to_mac(target_mac_addr, target_mac_addr_string);
        device_communication_role = ROLE_SNIFFER;

    } else{
        strmac_to_mac(remote_mac_addr, initiator_mac_addr_string);
        device_communication_role = ROLE_UNDEFINED;
    }
}

/**
 * @brief Configure the device for promiscuous sniffing.
 *
 * Initializes logging, LED, Wi-Fi in promiscuous receive mode, the event
 * queue, and a one-shot timeout timer. Reads the device's Wi-Fi STA MAC
 * address and sets it as the sniffer identity. Frames will be received
 * according to the promiscuous filter (no specific target).
 *
 * @param timeout_ms  Timeout duration in milliseconds for the one-shot timer.
 *
 * @post
 *  - Global logging level set for debugging.
 *  - LED subsystem configured.
 *  - Wi-Fi configured in REC_PROMISCUOUS mode.
 *  - FSM event queue created (size = EVENT_QUEUE_SIZE).
 *  - Timeout timer created with the given period and on_timeout callback.
 *  - sniffer_mac_addr_string updated to local MAC.
 */
void setup_promiscuous(uint16_t timeout_ms){
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);  // Get MAC for Wi-Fi station

    char macStr[18];  // "AA:BB:CC:DD:EE:FF" + null terminator
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    strcpy(sniffer_mac_addr_string, macStr);
    //strcpy(target_mac_addr_string, "f0:f5:bd:01:7c:04");
    setup(timeout_ms, REC_PROMISCUOUS);
}

/**
 * @brief Configure the device for targeted sniffing of a specific MAC.
 *
 * Initializes logging, LED, Wi-Fi in targeted receive mode, the event queue,
 * and a one-shot timeout timer. Uses the device's STA MAC as the sniffer
 * identity and sets @p target as the MAC address to follow.
 *
 * @param timeout_ms  Timeout duration in milliseconds for the one-shot timer.
 * @param target      Colon-separated MAC address string of the sniff target
 *                    (e.g., "AA:BB:CC:DD:EE:FF"). Must be NUL-terminated.
 *
 * @post
 *  - Global logging level set for debugging.
 *  - LED subsystem configured.
 *  - Wi-Fi configured in REC_TARGET mode.
 *  - FSM event queue created (size = EVENT_QUEUE_SIZE).
 *  - Timeout timer created with the given period and on_timeout callback.
 *  - Local STA MAC stored; device role determined as sniffer.
 *  - sniffer_mac_addr_string updated to local MAC.
 *  - target_mac_addr_string set to @p target.
 */
void setup_target(uint16_t timeout_ms, char* target){
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);  // Get MAC for Wi-Fi station

    char macStr[18];  // "AA:BB:CC:DD:EE:FF" + null terminator
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    strcpy(sniffer_mac_addr_string, macStr);
    strcpy(target_mac_addr_string, target);
    setup(timeout_ms, REC_TARGET);
}

/**
 * @brief Configure the device for unicast communication with a peer.
 *
 * Initializes logging, LED, Wi-Fi in unicast receive mode, the event queue,
 * and a one-shot timeout timer.
 *
 * @param timeout_ms  Timeout duration in milliseconds for the one-shot timer.
 * @param target      Colon-separated MAC address string of the peer
 *                    (e.g., "AA:BB:CC:DD:EE:FF"). Must be NUL-terminated.
 *
 * @post
 *  - Global logging level set for debugging.
 *  - LED subsystem configured.
 *  - Wi-Fi configured in REC_UNICAST mode.
 *  - FSM event queue created (size = EVENT_QUEUE_SIZE).
 *  - Timeout timer created with the given period and on_timeout callback.
 *  - initiator_mac_addr_string set to local MAC.
 *  - responder_mac_addr_string set to @p target.
 *  - Local STA MAC stored; device role determined (typically initiator).
 *
 * @note
 * device_communication_role might me nonsensical if this setup method is used,
 * device_communication_role should therefore not be used in the remaining code.
 */
void setup_unicast(uint16_t timeout_ms, char* target){
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);  // Get MAC for Wi-Fi station
    char macStr[18];  // "AA:BB:CC:DD:EE:FF" + null terminator
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    strcpy(initiator_mac_addr_string, macStr);
    strcpy(responder_mac_addr_string, target);
    setup(timeout_ms, REC_UNICAST);
}

/**
 * @brief Configure the device as the unicast initiator toward a peer.
 *
 * Initializes logging, LED, Wi-Fi in unicast receive mode, the event queue,
 * and a one-shot timeout timer. Declares the local device as initiator and
 * @p target as the responder via MAC string assignment.
 *
 * @param timeout_ms  Timeout duration in milliseconds for the one-shot timer.
 * @param target      Colon-separated MAC address string of the responder peer
 *                    (e.g., "AA:BB:CC:DD:EE:FF"). Must be NUL-terminated.
 *
 * @post
 *  - Global logging level set for debugging.
 *  - LED subsystem configured.
 *  - Wi-Fi configured in REC_UNICAST mode.
 *  - FSM event queue created (size = EVENT_QUEUE_SIZE).
 *  - Timeout timer created with the given period and on_timeout callback.
 *  - initiator_mac_addr_string set to local MAC.
 *  - responder_mac_addr_string set to @p target.
 */
void setup_initiator(uint16_t timeout_ms, char* target){
    setup_unicast(timeout_ms, target);
}

/**
 * @brief Configure the device as the unicast responder to a peer.
 *
 * Initializes logging, LED, Wi-Fi in unicast receive mode, the event queue,
 * and a one-shot timeout timer. Declares @p target as the initiator and
 * the local device as responder via MAC string assignment.
 *
 * @param timeout_ms  Timeout duration in milliseconds for the one-shot timer.
 * @param target      Colon-separated MAC address string of the initiator peer
 *                    (e.g., "AA:BB:CC:DD:EE:FF"). Must be NUL-terminated.
 *
 * @post
 *  - Global logging level set for debugging.
 *  - LED subsystem configured.
 *  - Wi-Fi configured in REC_UNICAST mode.
 *  - FSM event queue created (size = EVENT_QUEUE_SIZE).
 *  - Timeout timer created with the given period and on_timeout callback.
 *  - initiator_mac_addr_string set to @p target.
 *  - responder_mac_addr_string set to local MAC.
 */
void setup_responder(uint16_t timeout_ms, char* target){
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);  // Get MAC for Wi-Fi station
    char macStr[18];  // "AA:BB:CC:DD:EE:FF" + null terminator
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    strcpy(initiator_mac_addr_string, target);
    strcpy(responder_mac_addr_string, macStr);
    setup(timeout_ms, REC_UNICAST);
}
