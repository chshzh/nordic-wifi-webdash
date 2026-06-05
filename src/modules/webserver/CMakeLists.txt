if(CONFIG_WEBSERVER_MODULE)
  target_sources(app PRIVATE webserver.c)
  target_include_directories(app PUBLIC ${ZEPHYR_BASE}/subsys/net/ip)
endif()
