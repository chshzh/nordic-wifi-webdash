#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

# Register zego modules: button, led, wifi, network, wifi_ble_prov.
get_filename_component(ZEGO_BUTTON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../zego/modules/button REALPATH)
get_filename_component(ZEGO_LED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../zego/modules/led REALPATH)
get_filename_component(ZEGO_WIFI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../zego/modules/wifi REALPATH)
get_filename_component(ZEGO_NETWORK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../zego/modules/network REALPATH)
get_filename_component(ZEGO_WIFI_BLE_PROV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../zego/modules/wifi_ble_prov REALPATH)
list(APPEND EXTRA_ZEPHYR_MODULES ${ZEGO_BUTTON_DIR} ${ZEGO_LED_DIR} ${ZEGO_WIFI_DIR} ${ZEGO_NETWORK_DIR} ${ZEGO_WIFI_BLE_PROV_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nordic_wifi_webdash)

target_include_directories(app PUBLIC ${ZEPHYR_BASE}/subsys/net/ip)

# Linker script additions for HTTP resources
zephyr_linker_sources(SECTIONS sections-rom.ld)

# Add http service
zephyr_linker_section_ifdef(CONFIG_HTTP_SERVER NAME
				http_resource_desc_webserver_service
				KVMA RAM_REGION GROUP RODATA_REGION
				SUBALIGN Z_LINK_ITERABLE_SUBALIGN)

# Add static web resources
set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)

foreach(web_resource
  index.html
  main.js
  styles.css
    )
  generate_inc_file_for_target(
    app
    src/modules/webserver/www/${web_resource}
    ${gen_dir}/${web_resource}.gz.inc
    --gzip
  )
endforeach()

# Add subdirectories for modules
# mode_selector now provided by zego/wifi (EXTRA_ZEPHYR_MODULES above)
# Shared network module provided by zego/network (EXTRA_ZEPHYR_MODULES above);
# app shim (net_event_app.c) defines CLIENT_CONNECTED_CHAN and overrides weak callbacks.
add_subdirectory(src/modules/network)
# button and led now provided by zego/button and zego/led (EXTRA_ZEPHYR_MODULES above)
add_subdirectory(src/modules/webserver)
add_subdirectory(src/modules/memory)

# APP_VERSION_STRING, PRD_VERSION, SPECS_VERSION set by zego/wifi/CMakeLists.txt
target_sources(app PRIVATE src/main.c)
