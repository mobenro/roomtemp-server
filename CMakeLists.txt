cmake_minimum_required(VERSION 3.10)
set(C_STANDARD 17)

project(roomtemp_server LANGUAGES C VERSION 1.0)

add_executable(roomtemp_server
	src/main.c
	src/rts_sensor)

target_include_directories(roomtemp_server PRIVATE $(PROJECT_SOURCE_DIR)/src )

target_link_libraries(roomtemp_server pigpio)
target_link_libraries(roomtemp_server rt)
target_link_libraries(roomtemp_server pthread)
