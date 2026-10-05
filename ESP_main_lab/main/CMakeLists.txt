# Pick the ONE lab program to build and flash (each file has its own app_main).
#
#   session3/exp3.c                    RC4 + Wikipedia test vectors (checkpoint 3)
#   session3/ascon.c                   Ascon AEAD128 + HASH256 demo (checkpoint 5)
#   session4/exp4.c                    Random numbers: 128-bit key, sampling mod p, monobit test
#   session6/p2p_unidirectional.c      Session 6 task 1: one-way frames + LED
#   session6/p2p_protocol.c            Session 6 tasks 2-9: P2P ping-pong FSM (+ Ascon AEAD)
#                                      (also needs session6/p2p_fsm.c, the shared state machines)
#   tools/sniffer.c                    Appendix B Wireshark sniffer
#   tools/test.c                       LED / button test
#   tools/boilerplate_peer_to_peer.c   Course FSM template (reference only)
set(LAB_APP "session6/p2p_unidirectional.c")

idf_component_register(SRCS ${LAB_APP}
                    PRIV_REQUIRES auth aead ecc hash ttm4138_ecc ttm4138_communication ttm4138_utils global_variables ttm4138_setup
                    INCLUDE_DIRS "." "session6"
                    REQUIRES nvs_flash driver esp_wifi esp_driver_gpio esp_hw_support esp_timer bootloader_support)
