cmake_minimum_required(VERSION 3.16)
project(fn_external_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/W4 /EHsc /DUNICODE /D_UNICODE)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -DUNICODE /D_UNICODE)
endif()

add_executable(fn_external_test
  src/main.cpp
  src/process_win.cpp
  src/memory_win.cpp
  src/driver_client.cpp
  src/pattern.cpp
  src/config.cpp
  src/diag.cpp
)

if(WIN32)
  target_link_libraries(fn_external_test PRIVATE Psapi)
endif()

