cmake_minimum_required (VERSION 3.0)
set(PRJ_NAME "Audijo")
set(CMAKE_CXX_STANDARD 20)
set(AUDIJO_SRC "${${PRJ_NAME}_SOURCE_DIR}/")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${AUDIJO_SRC}/cmake/")

# Main library linking and including
project (${PRJ_NAME})

file(GLOB_RECURSE AUDIJO_SOURCE
  "${AUDIJO_SRC}source/*.cpp"
  "${AUDIJO_SRC}include/*.hpp"
)

set(AUDIJO_INCLUDE_DIRS
  ${AUDIJO_SRC}include/
)

option(AUDIJO_BUILD_DOCS "Build Docs" OFF)
option(AUDIJO_BUILD_EXAMPLE "Build Example" ON)
option(AUDIJO_USE_ASIO "Build ASIO API" OFF)
option(AUDIJO_USE_WASAPI "Build WASAPI API" ON)


if(AUDIJO_USE_ASIO)

# We require some sdks
FIND_PACKAGE(ASIOSDK REQUIRED)

list(APPEND AUDIJO_INCLUDE_DIRS
  ${ASIOSDK_ROOT_DIR}/common/
  ${ASIOSDK_ROOT_DIR}/host/
  ${ASIOSDK_ROOT_DIR}/host/pc/
)

list(APPEND AUDIJO_SOURCE
  ${ASIOSDK_ROOT_DIR}/common/asio.cpp
  ${ASIOSDK_ROOT_DIR}/host/asiodrivers.cpp
  ${ASIOSDK_ROOT_DIR}/host/pc/asiolist.cpp
)
endif()

# Add the library
add_library(${PRJ_NAME} STATIC ${AUDIJO_SOURCE})

if(AUDIJO_USE_WASAPI)
target_compile_definitions(${PRJ_NAME} PUBLIC AUDIJO_WASAPI)
endif()

if(AUDIJO_USE_ASIO)
target_compile_definitions(${PRJ_NAME} PUBLIC AUDIJO_ASIO)
endif()

target_include_directories(${PRJ_NAME} PUBLIC
  ${AUDIJO_INCLUDE_DIRS}
)

target_link_libraries(${PRJ_NAME}
  ${ASIOSDK_LIBRARIES}
  winmm 
  ole32
)

# Group source according to folder layout
source_group(TREE ${AUDIJO_SRC} FILES ${AUDIJO_SOURCE})

# PCH
target_precompile_headers(${PRJ_NAME} PUBLIC "${AUDIJO_SRC}include/Audijo/pch.hpp")

# Example project
if(AUDIJO_BUILD_EXAMPLE)
set(EXM_NAME "AudijoExample")
project (${EXM_NAME})
set(EXM_SRC "${${EXM_NAME}_SOURCE_DIR}/")
file(GLOB_RECURSE EXAMPLE_SOURCE "${EXM_SRC}example/*.cpp")
add_executable(${EXM_NAME} ${EXAMPLE_SOURCE})
target_include_directories(${EXM_NAME} PUBLIC "${AUDIJO_SRC}include/")
target_link_libraries(${EXM_NAME} ${PRJ_NAME})
source_group(TREE ${EXM_SRC} FILES ${EXAMPLE_SOURCE})
endif()

if(AUDIJO_BUILD_DOCS)
add_subdirectory("docs")
endif()