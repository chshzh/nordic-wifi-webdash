#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

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
    src/modules/www/${web_resource}
    ${gen_dir}/${web_resource}.gz.inc
    --gzip
  )
endforeach()

# Add subdirectories for modules
add_subdirectory(src/modules/mode_selector)
add_subdirectory(src/modules/network)
add_subdirectory(src/modules/button)
add_subdirectory(src/modules/led)
add_subdirectory(src/modules/webserver)
add_subdirectory(src/modules/memory)

target_sources(app PRIVATE
	src/main.c
)
