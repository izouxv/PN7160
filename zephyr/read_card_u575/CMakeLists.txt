cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pn7160_read_card_u575)

set(PN7160_ESP_BASE ${CMAKE_CURRENT_SOURCE_DIR}/../../ESP-PN7160-READ-CARD)

target_sources(app PRIVATE
  src/main.c
  src/tml.c
  src/tool.c
  ${PN7160_ESP_BASE}/components/NfcLibrary/NdefLibrary/src/P2P_NDEF.c
  ${PN7160_ESP_BASE}/components/NfcLibrary/NdefLibrary/src/RW_NDEF.c
  ${PN7160_ESP_BASE}/components/NfcLibrary/NdefLibrary/src/RW_NDEF_MIFARE.c
  ${PN7160_ESP_BASE}/components/NfcLibrary/NdefLibrary/src/RW_NDEF_T1T.c
  ${PN7160_ESP_BASE}/components/NfcLibrary/NdefLibrary/src/RW_NDEF_T2T.c
  ${PN7160_ESP_BASE}/components/NfcLibrary/NdefLibrary/src/RW_NDEF_T3T.c
  ${PN7160_ESP_BASE}/components/NfcLibrary/NdefLibrary/src/RW_NDEF_T4T.c
  ${PN7160_ESP_BASE}/components/NfcLibrary/NdefLibrary/src/RW_NDEF_T5T.c
  ${PN7160_ESP_BASE}/components/NfcLibrary/NdefLibrary/src/T4T_NDEF_emu.c
  ${PN7160_ESP_BASE}/components/NfcLibrary/NxpNci20/src/NxpNci20.c
)

target_include_directories(app PRIVATE
  include
  ${PN7160_ESP_BASE}/components/NfcLibrary/inc
  ${PN7160_ESP_BASE}/components/NfcLibrary/NdefLibrary/inc
  ${PN7160_ESP_BASE}/components/NfcLibrary/NxpNci20/inc
)

target_compile_definitions(app PRIVATE
  REMOVE_P2P_SUPPORT
)
