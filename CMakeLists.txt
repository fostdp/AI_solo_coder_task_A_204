cmake_minimum_required(VERSION 3.15)
project(stone_mill_dem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive-)
    add_compile_options($<$<CONFIG:Debug>:/Od /RTC1>)
    add_compile_options($<$<CONFIG:Release>:/O2>)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options($<$<CONFIG:Debug>:-O0 -g>)
    add_compile_options($<$<CONFIG:Release>:-O3>)
endif()

set(BACKEND_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/backend/include)
set(BACKEND_SOURCE_DIR ${CMAKE_SOURCE_DIR}/backend/src)

set(BACKEND_SOURCES
    ${BACKEND_SOURCE_DIR}/common.cpp
    ${BACKEND_SOURCE_DIR}/mqtt_client.cpp
    ${BACKEND_SOURCE_DIR}/clickhouse_client.cpp
    ${BACKEND_SOURCE_DIR}/dem_model.cpp
    ${BACKEND_SOURCE_DIR}/genetic_optimizer.cpp
    ${BACKEND_SOURCE_DIR}/alert_system.cpp
    ${BACKEND_SOURCE_DIR}/http_server.cpp
    ${BACKEND_SOURCE_DIR}/main.cpp
)

set(BACKEND_HEADERS
    ${BACKEND_INCLUDE_DIR}/common.h
    ${BACKEND_INCLUDE_DIR}/mqtt_client.h
    ${BACKEND_INCLUDE_DIR}/clickhouse_client.h
    ${BACKEND_INCLUDE_DIR}/dem_model.h
    ${BACKEND_INCLUDE_DIR}/genetic_optimizer.h
    ${BACKEND_INCLUDE_DIR}/alert_system.h
    ${BACKEND_INCLUDE_DIR}/http_server.h
)

add_executable(stone_mill_dem ${BACKEND_SOURCES} ${BACKEND_HEADERS})

target_include_directories(stone_mill_dem PRIVATE ${BACKEND_INCLUDE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(stone_mill_dem PRIVATE Threads::Threads)

option(USE_MQTT "Enable MQTT support with paho-mqtt" OFF)
if(USE_MQTT)
    find_package(PahoMqttCpp QUIET)
    if(PahoMqttCpp_FOUND)
        target_link_libraries(stone_mill_dem PRIVATE PahoMqttCpp::paho-mqttpp3)
        target_compile_definitions(stone_mill_dem PRIVATE USE_PAHO_MQTT)
        message(STATUS "Paho MQTT C++ library found")
    else()
        message(WARNING "Paho MQTT C++ library not found, using internal mock implementation")
    endif()
endif()

option(USE_CLICKHOUSE "Enable ClickHouse support with clickhouse-cpp" OFF)
if(USE_CLICKHOUSE)
    find_package(clickhouse-cpp QUIET)
    if(clickhouse-cpp_FOUND)
        target_link_libraries(stone_mill_dem PRIVATE clickhouse-cpp::clickhouse-cpp)
        target_compile_definitions(stone_mill_dem PRIVATE USE_CLICKHOUSE_CLIENT)
        message(STATUS "ClickHouse C++ library found")
    else()
        message(WARNING "ClickHouse C++ library not found, using internal mock implementation")
    endif()
endif()

option(USE_CIVETWEB "Enable HTTP server with civetweb" OFF)
if(USE_CIVETWEB)
    find_package(civetweb QUIET)
    if(civetweb_FOUND)
        target_link_libraries(stone_mill_dem PRIVATE civetweb::civetweb)
        target_compile_definitions(stone_mill_dem PRIVATE USE_CIVETWEB)
        message(STATUS "Civetweb library found")
    else()
        message(WARNING "Civetweb library not found, using internal mock implementation")
    endif()
endif()

option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(backend/tests)
endif()

install(TARGETS stone_mill_dem
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY frontend/ DESTINATION share/stone_mill_dem/frontend)
install(FILES clickhouse/init.sql DESTINATION share/stone_mill_dem/clickhouse)
install(PROGRAMS simulator/sensor_simulator.py DESTINATION share/stone_mill_dem/simulator)

message(STATUS "========================================")
message(STATUS "Stone Mill DEM Simulation System")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "MQTT support: ${USE_MQTT}")
message(STATUS "ClickHouse support: ${USE_CLICKHOUSE}")
message(STATUS "HTTP server: ${USE_CIVETWEB}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "========================================")
