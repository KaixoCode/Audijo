cmake_minimum_required (VERSION 3.0)
set(PRJ_NAME "Audijo")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set (CMAKE_CXX_STANDARD 20)
set(SRC "${${PRJ_NAME}_SOURCE_DIR}/")

# Main library linking and including
project (${PRJ_NAME})

# We require some sdks
FIND_PACKAGE(ASIOSDK)

file(GLOB_RECURSE SOURCE
  "${SRC}source/*.cpp"
  "${SRC}include/*.hpp"
)

# Add the library
add_library(${PRJ_NAME} STATIC ${SOURCE})

target_include_directories(${PRJ_NAME} PUBLIC
  ${SRC}include/
  ${ASIOSDK_INCLUDE_DIR}
)

# Link to SDKs
target_link_libraries(${PRJ_NAME} 
  ${ASIOSDK_LIBRARIES}
)

# Group source according to folder layout
source_group(TREE ${SRC} FILES ${SOURCE})

# PCH
target_precompile_headers(${PRJ_NAME} PUBLIC "${SRC}include/pch.hpp")

# Example project
set(EXM_NAME "AudijoExample")
project (${EXM_NAME})
set(EXM_SRC "${${EXM_NAME}_SOURCE_DIR}/")
file(GLOB_RECURSE EXAMPLE_SOURCE "${EXM_SRC}example/*.cpp")
add_executable(${EXM_NAME} ${EXAMPLE_SOURCE})
target_include_directories(${EXM_NAME} PUBLIC "${SRC}include/")
target_link_libraries(${EXM_NAME} ${PRJ_NAME})
source_group(TREE ${EXM_SRC} FILES ${EXAMPLE_SOURCE})

