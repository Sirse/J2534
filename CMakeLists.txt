cmake_minimum_required(VERSION 3.16)
project(j2534 VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "j2534 is supported only on Windows")
endif()

set(SOURCES
  src/J2534.cpp
  src/J2534Channel.cpp
)

set(HEADERS
  j2534/BaseMessage.hpp
  j2534/J2534.hpp
  j2534/J2534Channel.hpp
  j2534/J2534_v0404.h
)

add_library(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)
target_compile_definitions(${PROJECT_NAME} PUBLIC NOMINMAX)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
