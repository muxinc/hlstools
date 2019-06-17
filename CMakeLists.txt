cmake_minimum_required(VERSION 3.1)
project(muxincstreamvalidator VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)

find_package(CURL REQUIRED)
find_program(RE2C re2c)

if(RE2C)
add_custom_command( OUTPUT netutils.cpp DEPENDS netutils.re2c.cpp
    COMMAND ${RE2C} -i -o netutils.cpp netutils.re2c.cpp)
add_custom_command( OUTPUT m3u8.cpp DEPENDS m3u8.re2c.cpp
    COMMAND ${RE2C} -i -o m3u8.cpp m3u8.re2c.cpp)
else()
     error( "re2c required to compile" )
endif()

include_directories(${CURL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json/single_include/)
add_executable(muxincstreamvalidator main.cpp m3u8.cpp netutils.cpp tsanalyser.cpp)
target_link_libraries(muxincstreamvalidator ${CURL_LIBRARIES})
install (TARGETS muxincstreamvalidator DESTINATION bin)


add_executable(ttbf ttfb.cpp m3u8.cpp netutils.cpp)
target_link_libraries(ttbf ${CURL_LIBRARIES})
