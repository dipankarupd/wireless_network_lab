#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <esp_random.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "global_variables.h"
#include "ttm4138_setup.h"
#include "ttm4138_communication.h"
#include "aead.h"
#include "hash.h"
#include "defs.h"

#define SESSION7_TIMEOUT_MS 30000
#define RANDOM_LEN 4
#define PSK_LEN CRYPTO_KEYBYTES
#define NONCE_LEN CRYPTO_NPUBBYTES
#define TAG_LEN CRYPTO_ABYTES
#define HASH_LEN ASCON_HASH_SIZE
#define RECEIVED_MESSAGE_EVENT 1

#define MSG_R1 3
#define MSG_R1_R2 4
#define MSG_R2 5
#define MSG_SESSION_KEY 6

#define HEADER_LEN (1 + 2 * MAC_ADDR_LEN)
#define MAX_PLAINTEXT_LEN HASH_LEN
#define MAX_CIPHERTEXT_LEN (MAX_PLAINTEXT_LEN + TAG_LEN)
#define MAX_ENCRYPTED_FIELD_LEN (NONCE_LEN + MAX_CIPHERTEXT_LEN)
#define MAX_MESSAGE_LEN (HEADER_LEN + MAX_ENCRYPTED_FIELD_LEN)

static const char *TAG = "SESSION7_RESPONDER";

static const uint8_t LOCAL_MAC[MAC_ADDR_LEN] = {
    0x48, 0xf6, 0xee, 0xc7, 0x1f, 0x18
};

static const uint8_t REMOTE_MAC[MAC_ADDR_LEN] = {
    0x48, 0xf6, 0xee, 0xc7, 0x20, 0xf0
};

static const uint8_t PSK[PSK_LEN] = {
    0xa3, 0xf9, 0xe2, 0x2b,
    0x81, 0xc7, 0xd4, 0x40,
    0xe5, 0xa3, 0xf9, 0xe2,
    0x2b, 0x81, 0xc7, 0x58
};

typedef enum {
    RESP_IDLE,
    RESP_WAIT_M3,
    RESP_SEND_M4,
    RESP_WAIT_M5,
    RESP_DERIVE_SESSION_KEY,
    RESP_SEND_M6,
    RESP_AUTHENTICATED,
    RESP_ERROR
} responder_state_t;

static uint8_t r1[RANDOM_LEN];
static uint8_t r2[RANDOM_LEN];
static uint8_t session_key[HASH_LEN];

static void print_hex(const char *label, const uint8_t *data, size_t len)
{
    printf("%s", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

static int secure_compare(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t difference = 0;
    for (size_t i = 0; i < len; i++) {
        difference |= a[i] ^ b[i];
    }
    return difference == 0;
}

static int mac_equal(const uint8_t *a, const uint8_t *b)
{
    return secure_compare(a, b, MAC_ADDR_LEN);
}

static void random_bytes(uint8_t *buffer, size_t len)
{
    for (size_t i = 0; i < len; i += 4) {
        uint32_t value = esp_random();
        size_t copy_len = (len - i < 4) ? len - i : 4;
        memcpy(buffer + i, &value, copy_len);
    }
}

static void reset_session(void)
{
    memset(r1, 0, sizeof(r1));
    memset(r2, 0, sizeof(r2));
    memset(session_key, 0, sizeof(session_key));
}

static int derive_session_key(void)
{
    uint8_t input[PSK_LEN + 2 * MAC_ADDR_LEN + 2 * RANDOM_LEN];
    size_t offset = 0;

    memcpy(input + offset, PSK, PSK_LEN);
    offset += PSK_LEN;
    memcpy(input + offset, REMOTE_MAC, MAC_ADDR_LEN);
    offset += MAC_ADDR_LEN;
    memcpy(input + offset, LOCAL_MAC, MAC_ADDR_LEN);
    offset += MAC_ADDR_LEN;
    memcpy(input + offset, r1, RANDOM_LEN);
    offset += RANDOM_LEN;
    memcpy(input + offset, r2, RANDOM_LEN);

    if (hash(session_key, input, sizeof(input)) != 0) {
        ESP_LOGE(TAG, "Session-key derivation failed");
        return -1;
    }

    print_hex("Derived session key: ", session_key, HASH_LEN);
    return 0;
}

static size_t build_associated_data(uint8_t message_type, uint8_t *ad, int received)
{
    ad[0] = message_type;
    if (received) {
        memcpy(ad + 1, REMOTE_MAC, MAC_ADDR_LEN);
        memcpy(ad + 1 + MAC_ADDR_LEN, LOCAL_MAC, MAC_ADDR_LEN);
    } else {
        memcpy(ad + 1, LOCAL_MAC, MAC_ADDR_LEN);
        memcpy(ad + 1 + MAC_ADDR_LEN, REMOTE_MAC, MAC_ADDR_LEN);
    }
    return HEADER_LEN;
}

static int encrypt_field(uint8_t message_type, const uint8_t *plaintext, size_t plaintext_len, uint8_t *output, size_t *output_len)
{
    uint8_t nonce[NONCE_LEN];
    uint8_t ad[HEADER_LEN];
    unsigned long long ciphertext_len;

    random_bytes(nonce, sizeof(nonce));
    memcpy(output, nonce, NONCE_LEN);
    size_t ad_len = build_associated_data(message_type, ad, 0);

    if (ascon_aead_encrypt(output + NONCE_LEN, &ciphertext_len, plaintext, plaintext_len, ad, ad_len, nonce, PSK) != 0) {
        ESP_LOGE(TAG, "AEAD encryption failed");
        return -1;
    }

    *output_len = NONCE_LEN + (size_t)ciphertext_len;
    return 0;
}

static int decrypt_field(uint8_t message_type, const uint8_t *input, size_t input_len, uint8_t *plaintext, size_t *plaintext_len)
{
    if (input_len < NONCE_LEN + TAG_LEN) {
        ESP_LOGE(TAG, "Encrypted field too short");
        return -1;
    }

    uint8_t ad[HEADER_LEN];
    unsigned long long decrypted_len;
    const uint8_t *nonce = input;
    const uint8_t *ciphertext = input + NONCE_LEN;
    size_t ciphertext_len = input_len - NONCE_LEN;
    size_t ad_len = build_associated_data(message_type, ad, 1);

    if (ascon_aead_decrypt(plaintext, &decrypted_len, ciphertext, ciphertext_len, ad, ad_len, nonce, PSK) != 0) {
        ESP_LOGE(TAG, "AEAD verification/decryption failed");
        return -1;
    }

    *plaintext_len = (size_t)decrypted_len;
    return 0;
}

static int receive_protocol_message(uint8_t *message, size_t *message_len)
{
    event_t event;
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(SESSION7_TIMEOUT_MS);

    while (1) {
        TickType_t now = xTaskGetTickCount();
        TickType_t remaining = deadline - now;
        if ((int32_t)remaining <= 0) {
            ESP_LOGE(TAG, "Receive timeout");
            return -1;
        }

        if (!xQueueReceive(fsm_event_queue, &event, remaining)) {
            ESP_LOGE(TAG, "Receive timeout");
            return -1;
        }

        if (event.event != RECEIVED_MESSAGE_EVENT) {
            if (event.frame != NULL) {
                free(event.frame);
            }
            continue;
        }

        if (event.frame == NULL) {
            ESP_LOGE(TAG, "Received event has NULL frame");
            continue;
        }

        size_t header_length = offsetof(ieee80211_frame_t, data);
        if (event.length <= header_length) {
            ESP_LOGE(TAG, "Received frame contains no payload");
            free(event.frame);
            continue;
        }

        size_t payload_length = event.length - header_length - 4;
        if (payload_length > MAX_MESSAGE_LEN) {
            ESP_LOGE(TAG, "Received message too large");
            free(event.frame);
            continue;
        }

        memcpy(message, event.frame->data, payload_length);
        *message_len = payload_length;
        free(event.frame);
        return 0;
    }
}

static int validate_header(const uint8_t *message, size_t message_len, uint8_t expected_type)
{
    if (message_len < HEADER_LEN) {
        ESP_LOGE(TAG, "Message too short");
        return -1;
    }

    if (message[0] != expected_type) {
        ESP_LOGE(TAG, "Unexpected message type: %u", message[0]);
        return -1;
    }

    if (!mac_equal(message + 1, REMOTE_MAC)) {
        ESP_LOGE(TAG, "Invalid sender MAC");
        return -1;
    }

    if (!mac_equal(message + 1 + MAC_ADDR_LEN, LOCAL_MAC)) {
        ESP_LOGE(TAG, "Invalid receiver MAC");
        return -1;
    }

    return 0;
}

static int process_m3(void)
{
    uint8_t message[MAX_MESSAGE_LEN];
    uint8_t plaintext[RANDOM_LEN];
    size_t message_len;
    size_t plaintext_len;

    ESP_LOGI(TAG, "Waiting for M3...");
    if (receive_protocol_message(message, &message_len) != 0) {
        return -1;
    }

    if (validate_header(message, message_len, MSG_R1) != 0) {
        return -1;
    }

    ESP_LOGI(TAG, "M3 total length: %u, header: %u, encrypted field: %u",
         (unsigned)message_len,
         (unsigned)HEADER_LEN,
         (unsigned)(message_len - HEADER_LEN));

    size_t encrypted_len = message_len - HEADER_LEN;
    if (encrypted_len != NONCE_LEN + RANDOM_LEN + TAG_LEN) {
        ESP_LOGE(TAG, "Invalid M3 encrypted length");
        return -1;
    }

    if (decrypt_field(MSG_R1, message + HEADER_LEN, encrypted_len, plaintext, &plaintext_len) != 0) {
        ESP_LOGE(TAG, "Could not decrypt M3");
        return -1;
    }

    if (plaintext_len != RANDOM_LEN) {
        ESP_LOGE(TAG, "M3 contains invalid r1 length");
        return -1;
    }

    memcpy(r1, plaintext, RANDOM_LEN);
    print_hex("Received r1: ", r1, RANDOM_LEN);
    ESP_LOGI(TAG, "M3 verified");
    return 0;
}

static int send_m4(void)
{
    uint8_t plaintext[2 * RANDOM_LEN];
    uint8_t encrypted_field[NONCE_LEN + sizeof(plaintext) + TAG_LEN];
    uint8_t message[HEADER_LEN + sizeof(encrypted_field)];
    size_t encrypted_len;
    size_t offset = 0;

    memcpy(plaintext, r1, RANDOM_LEN);
    random_bytes(r2, RANDOM_LEN);
    memcpy(plaintext + RANDOM_LEN, r2, RANDOM_LEN);

    print_hex("Generated r2: ", r2, RANDOM_LEN);

    if (encrypt_field(MSG_R1_R2, plaintext, sizeof(plaintext), encrypted_field, &encrypted_len) != 0) {
        return -1;
    }

    message[offset++] = MSG_R1_R2;
    memcpy(message + offset, LOCAL_MAC, MAC_ADDR_LEN);
    offset += MAC_ADDR_LEN;
    memcpy(message + offset, REMOTE_MAC, MAC_ADDR_LEN);
    offset += MAC_ADDR_LEN;
    memcpy(message + offset, encrypted_field, encrypted_len);
    offset += encrypted_len;

    esp_err_t result = transmit((uint8_t *)REMOTE_MAC, message, offset);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to transmit M4: %s", esp_err_to_name(result));
        return -1;
    }

    ESP_LOGI(TAG, "M4 sent");
    return 0;
}

static int process_m5(void)
{
    uint8_t message[MAX_MESSAGE_LEN];
    uint8_t plaintext[RANDOM_LEN];
    size_t message_len;
    size_t plaintext_len;

    ESP_LOGI(TAG, "Waiting for M5...");
    if (receive_protocol_message(message, &message_len) != 0) {
        return -1;
    }

    if (validate_header(message, message_len, MSG_R2) != 0) {
        return -1;
    }

    size_t encrypted_len = message_len - HEADER_LEN;
    if (encrypted_len != NONCE_LEN + RANDOM_LEN + TAG_LEN) {
        ESP_LOGE(TAG, "Invalid M5 encrypted length");
        return -1;
    }

    if (decrypt_field(MSG_R2, message + HEADER_LEN, encrypted_len, plaintext, &plaintext_len) != 0) {
        ESP_LOGE(TAG, "Could not decrypt M5");
        return -1;
    }

    if (plaintext_len != RANDOM_LEN) {
        ESP_LOGE(TAG, "M5 contains invalid s2 length");
        return -1;
    }

    if (!secure_compare(plaintext, r2, RANDOM_LEN)) {
        ESP_LOGE(TAG, "Authentication failed: s2 != r2");
        return -1;
    }

    ESP_LOGI(TAG, "M5 verified: initiator authenticated");
    return 0;
}

static int send_m6(void)
{
    uint8_t encrypted_field[NONCE_LEN + HASH_LEN + TAG_LEN];
    uint8_t message[HEADER_LEN + sizeof(encrypted_field)];
    size_t encrypted_len;
    size_t offset = 0;

    if (encrypt_field(MSG_SESSION_KEY, session_key, HASH_LEN, encrypted_field, &encrypted_len) != 0) {
        return -1;
    }

    message[offset++] = MSG_SESSION_KEY;
    memcpy(message + offset, LOCAL_MAC, MAC_ADDR_LEN);
    offset += MAC_ADDR_LEN;
    memcpy(message + offset, REMOTE_MAC, MAC_ADDR_LEN);
    offset += MAC_ADDR_LEN;
    memcpy(message + offset, encrypted_field, encrypted_len);
    offset += encrypted_len;

    esp_err_t result = transmit((uint8_t *)REMOTE_MAC, message, offset);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to transmit M6: %s", esp_err_to_name(result));
        return -1;
    }

    ESP_LOGI(TAG, "M6 sent");
    return 0;
}

static void responder_fsm(void)
{
    responder_state_t state = RESP_IDLE;

    while (1) {
        switch (state) {
        case RESP_IDLE:
            reset_session();
            ESP_LOGI(TAG, "Responder FSM started");
            state = RESP_WAIT_M3;
            break;

        case RESP_WAIT_M3:
            state = (process_m3() == 0) ? RESP_SEND_M4 : RESP_ERROR;
            break;

        case RESP_SEND_M4:
            state = (send_m4() == 0) ? RESP_WAIT_M5 : RESP_ERROR;
            break;

        case RESP_WAIT_M5:
            state = (process_m5() == 0) ? RESP_DERIVE_SESSION_KEY : RESP_ERROR;
            break;

        case RESP_DERIVE_SESSION_KEY:
            state = (derive_session_key() == 0) ? RESP_SEND_M6 : RESP_ERROR;
            break;

        case RESP_SEND_M6:
            state = (send_m6() == 0) ? RESP_AUTHENTICATED : RESP_ERROR;
            break;

        case RESP_AUTHENTICATED:
            ESP_LOGI(TAG, "MUTUAL AUTHENTICATION SUCCESSFUL");
            ESP_LOGI(TAG, "Responder authenticated Initiator");
            ESP_LOGI(TAG, "Both sides now share the session key");
            return;

        case RESP_ERROR:
            ESP_LOGE(TAG, "AUTHENTICATION FAILED - restarting");
            reset_session();
            vTaskDelay(pdMS_TO_TICKS(1000));
            state = RESP_WAIT_M3;
            break;

        default:
            ESP_LOGE(TAG, "Invalid FSM state - restarting");
            reset_session();
            state = RESP_WAIT_M3;
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    char receiver[MAC_ADDR_STR_LEN];

    memcpy(local_mac_addr, LOCAL_MAC, MAC_ADDR_LEN);
    memcpy(remote_mac_addr, REMOTE_MAC, MAC_ADDR_LEN);
    device_communication_role = ROLE_RESPONDER;

    snprintf(receiver, sizeof(receiver), "%02x:%02x:%02x:%02x:%02x:%02x", REMOTE_MAC[0], REMOTE_MAC[1], REMOTE_MAC[2], REMOTE_MAC[3], REMOTE_MAC[4], REMOTE_MAC[5]);

    ESP_LOGI(TAG, "Starting Session 7 Responder");
    ESP_LOGI(TAG, "Responder MAC: %02x:%02x:%02x:%02x:%02x:%02x", LOCAL_MAC[0], LOCAL_MAC[1], LOCAL_MAC[2], LOCAL_MAC[3], LOCAL_MAC[4], LOCAL_MAC[5]);
    ESP_LOGI(TAG, "Initiator MAC: %s", receiver);

    setup_responder(SESSION7_TIMEOUT_MS, receiver);
    responder_fsm();
}
