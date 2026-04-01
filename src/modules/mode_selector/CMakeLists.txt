#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

if(CONFIG_APP_MODE_SELECTOR)
  target_sources(app PRIVATE mode_selector.c)
  target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()
