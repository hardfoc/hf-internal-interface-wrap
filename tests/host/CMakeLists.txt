# Host unit tests for hf-internal-interface-wrap (no MCU, no RTOS).
#
#   cmake -S tests/host -B build/host -G Ninja
#   cmake --build build/host
#   ctest --test-dir build/host --output-on-failure
#
# Covers the MCU-independent parts: the BaseCan v2 frame type and the
# lock-free RX ring used by EspCan. HF_HOST_SANITIZE=address|thread adds
# sanitizers (the ring's two-thread test is meant for ThreadSanitizer).
cmake_minimum_required(VERSION 3.16)
project(hf_iiw_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard for the host tests")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(HF_IIW_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(HF_HOST_SANITIZE "" CACHE STRING "Sanitizer: empty, address or thread")

find_package(Threads REQUIRED)
enable_testing()

add_library(hf_iiw_host_test_main STATIC HostTestMain.cpp)
target_include_directories(hf_iiw_host_test_main PUBLIC "${CMAKE_CURRENT_LIST_DIR}")

set(HF_HOST_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror)

function(hf_iiw_host_test name)
  add_executable(${name} ${name}.cpp)
  target_include_directories(${name} PRIVATE
    "${HF_IIW_ROOT}/inc"
    "${HF_IIW_ROOT}/inc/base"
    "${HF_IIW_ROOT}/inc/utils")
  target_compile_options(${name} PRIVATE ${HF_HOST_WARNINGS})
  target_link_libraries(${name} PRIVATE hf_iiw_host_test_main Threads::Threads)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

hf_iiw_host_test(test_can_frame)
hf_iiw_host_test(test_spsc_ring)

if(HF_HOST_SANITIZE STREQUAL "address")
  foreach(t hf_iiw_host_test_main test_can_frame test_spsc_ring)
    target_compile_options(${t} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer
                                        -fno-sanitize-recover=all)
    target_link_options(${t} PRIVATE -fsanitize=address,undefined)
  endforeach()
elseif(HF_HOST_SANITIZE STREQUAL "thread")
  foreach(t hf_iiw_host_test_main test_can_frame test_spsc_ring)
    target_compile_options(${t} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(${t} PRIVATE -fsanitize=thread)
  endforeach()
elseif(NOT HF_HOST_SANITIZE STREQUAL "")
  message(FATAL_ERROR "HF_HOST_SANITIZE must be empty, address or thread")
endif()
