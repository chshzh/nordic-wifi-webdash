# Network module CMakeLists.txt
target_sources(app PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/network.c
)

target_include_directories(app PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	# net_private.h (for NET_IPV4_ADDR_LEN etc.) lives here
	${ZEPHYR_BASE}/subsys/net/ip
)
