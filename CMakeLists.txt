cmake_minimum_required(VERSION 3.14)
project(e2emu VERSION 1.1.0 LANGUAGES CXX)
get_directory_property(parentDir PARENT_DIRECTORY)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(E2EMU_SOURCE_FOLDERS core devices execution interrupts)

option(E2EMU_TEST "Compile e2emu UT suite" OFF)

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_TAG)
        string(REGEX REPLACE "^v" "" E2EMU_VERSION ${GIT_TAG})
    else()
      set(E2EMU_VERSION ${PROJECT_VERSION})
    endif()
else()
  set(E2EMU_VERSION ${PROJECT_VERSION})
endif()

if(parentDir)
  set(E2EMU_VERSION ${E2EMU_VERSION} PARENT_SCOPE)
else()
  set(E2EMU_VERSION ${E2EMU_VERSION})
endif()

foreach(dir ${E2EMU_SOURCE_FOLDERS})
  file(GLOB_RECURSE sources
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*.cpp
  )

  file(GLOB_RECURSE headers
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*.h
  )

  list(APPEND E2EMU_SOURCES ${sources})
  list(APPEND E2EMU_HEADERS ${headers})
endforeach()

add_library(e2emu STATIC ${E2EMU_SOURCES} ${E2EMU_HEADERS})

target_include_directories(e2emu PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(e2emu PUBLIC cxx_std_20)

if(E2EMU_TEST)
  # GTest Setup
  include(FetchContent)

  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(googletest)

  enable_testing()

  set(TEST_ASM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/asm")

  add_subdirectory(tests/asm)

  file(GLOB E2EMU_TEST_SOURCES tests/*.cpp)

  add_executable(e2emu_ut ${E2EMU_TEST_SOURCES})

  # Define TEST_ASM_DIR for the test executable
  target_compile_definitions(e2emu_ut PRIVATE
    TEST_ASM_DIR="${TEST_ASM_DIR}"
  )

  target_link_libraries(e2emu_ut
    PRIVATE
    e2emu
    GTest::gtest
    GTest::gtest_main
  )

  target_include_directories(e2emu_ut PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
  )

  # Ensure test binaries are assembled before running tests
  add_dependencies(e2emu_ut assemble_tests)

  include(GoogleTest)
  gtest_discover_tests(e2emu_ut)

  add_test(NAME e2emu_unit_test COMMAND e2emu_ut)

  add_custom_target(test_emu
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R e2emu
    DEPENDS e2emu_ut
  )

  add_custom_target(test_emu_verbose
    COMMAND e2emu_ut --gtest_color=yes
    DEPENDS e2emu_ut
  )

  add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS e2emu_ut
  )

  add_custom_target(check_verbose
    COMMAND e2emu_ut --gtest_color=yes
    DEPENDS e2emu_ut
  )
endif()
