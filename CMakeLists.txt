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
    add_compile_options(/W3 /permissive-)
    add_compile_options($<$<CONFIG:Debug>:/Od /RTC1>)
    add_compile_options($<$<CONFIG:Release>:/O2>)
else()
    add_compile_options(-Wall -Wextra)
    add_compile_options($<$<CONFIG:Debug>:-O0 -g>)
    add_compile_options($<$<CONFIG:Release>:-O3>)
endif()

set(BACKEND_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/backend/include)
set(BACKEND_SOURCE_DIR ${CMAKE_SOURCE_DIR}/backend/src)

set(BACKEND_SOURCES
    ${BACKEND_SOURCE_DIR}/common.cpp
    ${BACKEND_SOURCE_DIR}/config_loader.cpp
    ${BACKEND_SOURCE_DIR}/dem_model.cpp
    ${BACKEND_SOURCE_DIR}/genetic_optimizer.cpp
    ${BACKEND_SOURCE_DIR}/alert_system.cpp
    ${BACKEND_SOURCE_DIR}/modules/mqtt_receiver.cpp
    ${BACKEND_SOURCE_DIR}/modules/dem_simulator.cpp
    ${BACKEND_SOURCE_DIR}/modules/size_optimizer.cpp
    ${BACKEND_SOURCE_DIR}/modules/alarm_mqtt.cpp
    ${BACKEND_SOURCE_DIR}/main.cpp
)

set(BACKEND_HEADERS
    ${BACKEND_INCLUDE_DIR}/common.h
    ${BACKEND_INCLUDE_DIR}/message_queue.h
    ${BACKEND_INCLUDE_DIR}/config_loader.h
    ${BACKEND_INCLUDE_DIR}/dem_model.h
    ${BACKEND_INCLUDE_DIR}/genetic_optimizer.h
    ${BACKEND_INCLUDE_DIR}/alert_system.h
    ${BACKEND_INCLUDE_DIR}/modules/mqtt_receiver.h
    ${BACKEND_INCLUDE_DIR}/modules/dem_simulator.h
    ${BACKEND_INCLUDE_DIR}/modules/size_optimizer.h
    ${BACKEND_INCLUDE_DIR}/modules/alarm_mqtt.h
)

add_executable(stone_mill_dem ${BACKEND_SOURCES} ${BACKEND_HEADERS})

target_include_directories(stone_mill_dem PRIVATE ${BACKEND_INCLUDE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(stone_mill_dem PRIVATE Threads::Threads)

option(USE_BOOST_LOCKFREE "Use Boost.Lockfree for inter-module queues" ON)
if(USE_BOOST_LOCKFREE)
    find_package(Boost QUIET)
    if(Boost_FOUND)
        target_include_directories(stone_mill_dem PRIVATE ${Boost_INCLUDE_DIRS})
        target_compile_definitions(stone_mill_dem PRIVATE USE_BOOST_LOCKFREE)
        message(STATUS "Boost found: using Boost.Lockfree queues")
    else()
        message(WARNING "Boost not found, falling back to std::queue + mutex (thread-safe but not lock-free)")
        target_compile_definitions(stone_mill_dem PRIVATE FALLBACK_MUTEX_QUEUE)
    endif()
endif()

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

install(DIRECTORY backend/config/ DESTINATION etc/stone_mill_dem)
install(DIRECTORY frontend/ DESTINATION share/stone_mill_dem/frontend)
install(FILES clickhouse/init.sql DESTINATION share/stone_mill_dem/clickhouse)
install(PROGRAMS simulator/sensor_simulator.py DESTINATION share/stone_mill_dem/simulator)

message(STATUS "========================================")
message(STATUS "Stone Mill DEM Simulation System (Modular)")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Boost.Lockfree: ${USE_BOOST_LOCKFREE}")
message(STATUS "MQTT support: ${USE_MQTT}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "========================================")
