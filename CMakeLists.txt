cmake_minimum_required(VERSION 3.25)
project(REL)

set(CMAKE_CXX_STANDARD 17)

# output to build directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)
set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/appicon.rc" )



add_executable(TitleChanger main.cpp ${APP_ICON_RESOURCE_WINDOWS} Addresses.h procutil.cpp procutil.h sigscan.cpp sigscan.h RobloxProcess.cpp RobloxProcess.h)


