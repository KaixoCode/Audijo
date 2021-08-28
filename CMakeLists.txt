cmake_minimum_required (VERSION 3.0)
set(PRJ_NAME "Audijo")
set(CMAKE_CXX_STANDARD 20)
set(SRC "${${PRJ_NAME}_SOURCE_DIR}/")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${SRC}/cmake/")

# Main library linking and including
project (${PRJ_NAME})

file(GLOB_RECURSE SOURCE
  "${SRC}source/*.cpp"
  "${SRC}include/*.hpp"
)

set(INCLUDE_DIRS
  ${SRC}include/
)

option(AUDIJO_BUILD_DOCS "Build Docs" OFF)
option(AUDIJO_BUILD_EXAMPLE "Build Example" ON)
option(AUDIJO_USE_ASIO "Build ASIO API" ON)
option(AUDIJO_USE_WASAPI "Build WASAPI API" ON)


if(AUDIJO_USE_WASAPI)
add_definitions(-DAUDIJO_WASAPI)
endif()

if(AUDIJO_USE_ASIO)
add_definitions(-DAUDIJO_ASIO)

# We require some sdks
FIND_PACKAGE(ASIOSDK REQUIRED)

list(APPEND INCLUDE_DIRS
  ${ASIOSDK_ROOT_DIR}/common/
  ${ASIOSDK_ROOT_DIR}/host/
  ${ASIOSDK_ROOT_DIR}/host/pc/
)

list(APPEND SOURCE
  ${ASIOSDK_ROOT_DIR}/common/asio.cpp
  ${ASIOSDK_ROOT_DIR}/host/asiodrivers.cpp
  ${ASIOSDK_ROOT_DIR}/host/pc/asiolist.cpp
)
endif()

# Add the library
add_library(${PRJ_NAME} STATIC ${SOURCE})

target_include_directories(${PRJ_NAME} PUBLIC
  ${INCLUDE_DIRS}
)

target_link_libraries(${PRJ_NAME}
  ${ASIOSDK_LIBRARIES}
  winmm 
  ole32
)

# Group source according to folder layout
source_group(TREE ${SRC} FILES ${SOURCE})

# PCH
target_precompile_headers(${PRJ_NAME} PUBLIC "${SRC}include/Audijo/pch.hpp")

# Example project
if(AUDIJO_BUILD_EXAMPLE)
set(EXM_NAME "AudijoExample")
project (${EXM_NAME})
set(EXM_SRC "${${EXM_NAME}_SOURCE_DIR}/")
file(GLOB_RECURSE EXAMPLE_SOURCE "${EXM_SRC}example/*.cpp")
add_executable(${EXM_NAME} ${EXAMPLE_SOURCE})
target_include_directories(${EXM_NAME} PUBLIC "${SRC}include/")
target_link_libraries(${EXM_NAME} ${PRJ_NAME})
source_group(TREE ${EXM_SRC} FILES ${EXAMPLE_SOURCE})
endif()

if(AUDIJO_BUILD_DOCS)
add_subdirectory("docs")
endif()