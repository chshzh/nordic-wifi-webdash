# Network module CMakeLists.txt
target_sources(app PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/net_event_mgmt.c
	${CMAKE_CURRENT_SOURCE_DIR}/wifi_utils.c
)
