cmake_minimum_required(VERSION 3.16)
project(toggle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(toggle main.cpp)

find_package(raylib REQUIRED)

target_link_libraries(toggle PRIVATE raylib)

if(UNIX AND NOT APPLE)
  target_link_libraries(toggle PRIVATE GL m pthread dl rt)
endif()

install(TARGETS toggle RUNTIME DESTINATION bin)
install(FILES IosevkaNerdFontMono-Regular.ttf DESTINATION share/toggle)
install(DIRECTORY config DESTINATION share/toggle)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/toggle.desktop.in
               ${CMAKE_CURRENT_BINARY_DIR}/toggle.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/toggle.desktop
        DESTINATION share/applications)
